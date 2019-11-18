/* <dory/msg_dispatch/produce_request_factory.cc>

   ----------------------------------------------------------------------------
   Copyright 2013-2014 if(we)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
   ----------------------------------------------------------------------------

   Implements <dory/msg_dispatch/produce_request_factory.h>.
 */

#include <dory/msg_dispatch/produce_request_factory.h>

#include <utility>

#include <base/counter.h>
#include <base/no_default_case.h>
#include <log/log.h>

using namespace Base;
using namespace Dory;
using namespace Dory::Batch;
using namespace Dory::Compress;
using namespace Dory::Conf;
using namespace Dory::Debug;
using namespace Dory::KafkaProto::Produce;
using namespace Dory::MsgDispatch;
using namespace Dory::Util;
using namespace Log;

DEFINE_COUNTER(BugAllTopicsEmpty);
DEFINE_COUNTER(BugMsgListMultipleTopics);
DEFINE_COUNTER(BugMsgSetEmpty);
DEFINE_COUNTER(BugMultiPartitionGroupEmpty);
DEFINE_COUNTER(MsgSetCompressionError);
DEFINE_COUNTER(MsgSetCompressionNo);
DEFINE_COUNTER(MsgSetCompressionYes);
DEFINE_COUNTER(MsgSetNotCompressible);
DEFINE_COUNTER(SerializeMsg);
DEFINE_COUNTER(SerializeMsgSet);
DEFINE_COUNTER(SerializeProduceRequest);
DEFINE_COUNTER(SerializeTopicGroup);

TProduceRequestFactory::TProduceRequestFactory(const TConfig &config,
    const TGlobalBatchConfig &batch_config,
    const TCompressionConf &compression_conf,
    const std::shared_ptr<TProduceProtocol> &produce_protocol,
    size_t broker_index)
    : Config(config),
      BrokerIndex(broker_index),
      ProduceProtocol(produce_protocol),
      ProduceRequestDataLimit(batch_config.GetProduceRequestDataLimit()),
      MessageMaxBytes(batch_config.GetMessageMaxBytes()),
      SingleMsgOverhead(produce_protocol->GetSingleMsgOverhead()),
      MaxCompressionRatio(compression_conf.SizeThresholdPercent / 100.0f),
      RequestWriter(produce_protocol->CreateProduceRequestWriter()),
      MsgSetWriter(produce_protocol->CreateMsgSetWriter()),
      DefaultTopicCompressionInfo(compression_conf.DefaultTopicConfig) {
  InitTopicDataMap(compression_conf);
}

void TProduceRequestFactory::Init(
    const TCompressionConf &compression_conf,
    const std::shared_ptr<TMetadata> &md) {
  assert(this);
  DefaultTopicCompressionInfo =
      TCompressionInfo(compression_conf.DefaultTopicConfig);
  Metadata = md;
  CorrIdCounter = 0;
  InitTopicDataMap(compression_conf);
}

void TProduceRequestFactory::Reset() {
  assert(this);
  Metadata.reset();
  CorrIdCounter = 0;
  TopicDataMap.clear();
}

TOpt<TProduceRequest> TProduceRequestFactory::BuildRequest(
    std::vector<uint8_t> &dst) {
  assert(this);

  if (IsEmpty()) {
    return TOpt<TProduceRequest>();
  }

  TProduceRequest request(++CorrIdCounter, BuildRequestContents());

  if (request.second.empty()) {
    assert(false);
    BugAllTopicsEmpty.Increment();
    LOG_R(TPri::ERR, std::chrono::seconds(30)) << "Bug!!! TAllTopics is empty";
    return TOpt<TProduceRequest>();
  }

  const char *client_id_begin = Config.ClientId.data();
  RequestWriter->OpenRequest(dst, request.first, client_id_begin,
      client_id_begin + Config.ClientId.size(), Config.RequiredAcks,
      static_cast<int32_t>(Config.ReplicationTimeout));
  const TAllTopics &all_topics = request.second;
  assert(!all_topics.empty());

  for (const auto &topic_elem : all_topics) {
    const std::string &topic = topic_elem.first;
    const char *topic_begin = topic.data();
    RequestWriter->OpenTopic(topic_begin, topic_begin + topic.size());
    const TMultiPartitionGroup &partition_group = topic_elem.second;
    assert(!partition_group.empty());

    for (const auto &partition_group_elem : partition_group) {
      RequestWriter->OpenMsgSet(partition_group_elem.first);
      WriteOneMsgSet(partition_group_elem.second,
          GetTopicData(topic).CompressionInfo, dst);
      RequestWriter->CloseMsgSet();
      SerializeMsgSet.Increment();
    }

    RequestWriter->CloseTopic();
    SerializeTopicGroup.Increment();
  }

  RequestWriter->CloseRequest();
  SerializeProduceRequest.Increment();
  return TOpt<TProduceRequest>(std::move(request));
}

static TOpt<int> GetRealCompressionLevel(const TCompressionConf::TConf &conf) {
  const Compress::TCompressionCodecApi *codec = GetCompressionCodec(conf.Type);
  TOpt<int> real_level = codec ?
      codec->GetRealCompressionLevel(conf.Level) : TOpt<int>();

  if (conf.Level.IsKnown()) {
    if (real_level.IsUnknown()) {
      LOG(TPri::WARNING) << "Ignoring compression level of " << *conf.Level
          << " requested for compression type that does not support levels: "
          << ToString(conf.Type);
    } else if (*real_level != *conf.Level) {
      LOG(TPri::WARNING) << "Ignoring invalid compression level of "
          << *conf.Level << " requested for compression type: "
          << ToString(conf.Type);
    }
  }

  return real_level;
}

TProduceRequestFactory::TCompressionInfo::TCompressionInfo(
    const TCompressionConf::TConf &conf)
    : CompressionCodec(GetCompressionCodec(conf.Type)),
      MinCompressionSize(conf.MinSize),
      CompressionType(conf.Type),
      CompressionLevel(GetRealCompressionLevel(conf)) {
}

TProduceRequestFactory::TTopicData::TTopicData(
    const TCompressionConf::TConf &conf)
    : CompressionInfo(conf) {
}

TProduceRequestFactory::TTopicData::TTopicData(
    const TCompressionInfo &info)
    : CompressionInfo(info) {
}

void TProduceRequestFactory::InitTopicDataMap(
    const TCompressionConf &compression_conf) {
  assert(this);
  TopicDataMap.clear();
  const TCompressionConf::TTopicMap &topic_map = compression_conf.TopicConfigs;

  for (const auto &item : topic_map) {
    TopicDataMap.insert(std::make_pair(item.first, TTopicData(item.second)));
  }
}

TProduceRequestFactory::TTopicData &
TProduceRequestFactory::GetTopicData(const std::string &topic) {
  assert(this);
  auto iter = TopicDataMap.find(topic);

  if (iter == TopicDataMap.end()) {
    auto ret = TopicDataMap.insert(
        std::make_pair(topic, TTopicData(DefaultTopicCompressionInfo)));
    iter = ret.first;
  }

  assert(iter != TopicDataMap.end());
  return iter->second;
}

/* This function should _never_ get called.  It's a damage containment
   mechanism in case of a bug. */
static bool MultipleTopicBugFixup(
    std::list<std::list<TMsg::TPtr>> &input_queue) {
  BugMsgListMultipleTopics.Increment();
  LOG_R(TPri::ERR, std::chrono::seconds(30))
      << "Bug!!! Msg list has multiple topics";
  assert(false);
  auto iter = input_queue.begin();
  assert(iter != input_queue.end());
  std::list<TMsg::TPtr> single_item_list;
  single_item_list.splice(single_item_list.begin(), *iter,
                          iter->begin());
  auto next_iter = iter;
  ++next_iter;
  input_queue.insert(next_iter, std::move(single_item_list));

  if (iter->empty()) {
    input_queue.pop_front();
    return true;
  }

  return false;
}

static void SanityCheckRequestContents(TAllTopics &contents) {
  for (auto all_topics_iter = contents.begin(),
           all_topics_next = all_topics_iter;
       all_topics_iter != contents.end();
       all_topics_iter = all_topics_next) {
    ++all_topics_next;
    TMultiPartitionGroup &group = all_topics_iter->second;

    for (auto group_iter = group.begin(), group_next = group_iter;
         group_iter != group.end();
         group_iter = group_next) {
      ++group_next;
      TMsgSet &msg_set = group_iter->second;

      if (msg_set.Contents.empty()) {
        BugMsgSetEmpty.Increment();
        LOG_R(TPri::ERR, std::chrono::seconds(30))
            << "Bug!!! TMsgSet is empty";
        assert(false);
        group.erase(group_iter);
      }
    }

    if (group.empty()) {
      BugMultiPartitionGroupEmpty.Increment();
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Bug!!! TMultiPartitionGroup is empty";
      assert(false);
      contents.erase(all_topics_iter);
    }
  }
}

size_t TProduceRequestFactory::AddFirstMsg(TAllTopics &result) {
  assert(this);
  assert(!InputQueue.empty());
  TMsg::TPtr msg_ptr;

  {
    std::list<TMsg::TPtr> &first_batch = InputQueue.front();
    assert(!first_batch.empty());
    msg_ptr = std::move(first_batch.front());
    first_batch.pop_front();

    if (first_batch.empty()) {
      InputQueue.pop_front();
    }
  }

  const std::string &topic = msg_ptr->GetTopic();
  TTopicData &topic_data = GetTopicData(topic);

  if (msg_ptr->GetRoutingType() == TMsg::TRoutingType::AnyPartition) {
    msg_ptr->SetPartition(topic_data.AnyPartitionChooser.GetChoice(BrokerIndex,
        *Metadata, topic));
    topic_data.AnyPartitionChooser.SetChoiceUsed();
  }

  TMsgSet &msg_set = result[topic][msg_ptr->GetPartition()];
  size_t data_size = msg_ptr->GetKeyAndValue().Size();

  if (topic_data.CompressionInfo.CompressionCodec) {
    assert(msg_set.DataSize == 0);
    msg_set.DataSize = data_size + SingleMsgOverhead;
  }

  msg_set.Contents.push_back(std::move(msg_ptr));
  return data_size;
}

bool TProduceRequestFactory::TryConsumeFrontMsg(
    std::list<TMsg::TPtr> &next_batch, const std::string &topic,
    TTopicData &topic_data, size_t &result_data_size, TAllTopics &result) {
  assert(this);
  assert(!next_batch.empty());
  TMsg::TPtr &msg_ptr = next_batch.front();
  bool any_partition =
      (msg_ptr->GetRoutingType() == TMsg::TRoutingType::AnyPartition);

  if (any_partition) {
    msg_ptr->SetPartition(topic_data.AnyPartitionChooser.GetChoice(BrokerIndex,
        *Metadata, topic));
  }

  size_t data_size = msg_ptr->GetKeyAndValue().Size();
  size_t new_result_data_size = result_data_size + data_size;

  if (new_result_data_size > ProduceRequestDataLimit) {
    return false;
  }

  TMsgSet &msg_set = result[topic][msg_ptr->GetPartition()];

  if (topic_data.CompressionInfo.CompressionCodec) {
    size_t new_data_size = msg_set.DataSize + data_size + SingleMsgOverhead;

    if (new_data_size > MessageMaxBytes) {
      /* If we added this message to the message set, then we would get a
         MessageSizeTooLarge error from Kafka in a worst case scenario where
         compression fails to reduce the size of the message set.  Note that a
         single message can never exceed the threshold because a message that
         large will never get this far. */
      assert(!msg_set.Contents.empty());
      assert(msg_set.DataSize);
      return false;
    }

    msg_set.DataSize = new_data_size;
  }

  if (any_partition) {
    topic_data.AnyPartitionChooser.SetChoiceUsed();
  }

  result_data_size = new_result_data_size;
  msg_set.Contents.push_back(std::move(msg_ptr));
  return true;
}

TAllTopics TProduceRequestFactory::BuildRequestContents() {
  assert(this);
  assert(!InputQueue.empty());
  TAllTopics result;
  size_t result_data_size = AddFirstMsg(result);

  /* Once we have reached the data limit for an entire request, we can't add
     any more messages.  However, we allow a single message by itself to exceed
     the limit.  We don't check 'MessageMaxBytes' here because no single
     message that large will get this far. */
  if (result_data_size < ProduceRequestDataLimit) {
    bool result_full = false;

    while (!InputQueue.empty()) {
      std::list<TMsg::TPtr> &next_batch = InputQueue.front();
      assert(!next_batch.empty());
      const std::string &topic = next_batch.front()->GetTopic();
      TTopicData &topic_data = GetTopicData(topic);

      for (; ; ) {
        TMsg::TPtr &msg_ptr = next_batch.front();

        if (msg_ptr->GetTopic() != topic) {
          /* We should _never_ get here. */
          if (MultipleTopicBugFixup(InputQueue)) {
            break;
          }

          continue;
        }

        result_full = !TryConsumeFrontMsg(next_batch, topic, topic_data,
                                          result_data_size, result);

        if (result_full) {
          break;
        }

        next_batch.pop_front();

        if (next_batch.empty()) {
          InputQueue.pop_front();
          break;
        }
      }

      if (result_full) {
        break;
      }
    }
  }

  for (auto &elem : result) {
    GetTopicData(elem.first).AnyPartitionChooser.ClearChoice();
  }

  SanityCheckRequestContents(result);
  return result;
}

void TProduceRequestFactory::SerializeUncompressedMsgSet(
    const std::list<TMsg::TPtr> &msg_set, std::vector<uint8_t> &dst) {
  assert(this);
  assert(!msg_set.empty());

  for (const TMsg::TPtr &msg_ptr : msg_set) {
    const TMsg &msg = *msg_ptr;
    size_t key_size = msg.GetKeySize();
    size_t value_size = msg.GetValueSize();
    RequestWriter->OpenMsg(TCompressionType::None, key_size, value_size);
    size_t key_offset = RequestWriter->GetCurrentMsgKeyOffset();
    assert(dst.size() >= key_offset);
    assert((dst.size() - key_offset) >= key_size);
    size_t value_offset = RequestWriter->GetCurrentMsgValueOffset();
    assert(dst.size() >= value_offset);
    assert((dst.size() - value_offset) == value_size);
    WriteKey(&dst[0] + key_offset, msg);
    WriteValue(&dst[0] + value_offset, msg);
    RequestWriter->CloseMsg();
    SerializeMsg.Increment();
  }
}

void TProduceRequestFactory::SerializeToCompressionBuf(
    const std::list<TMsg::TPtr> &msg_set) {
  assert(this);
  assert(!msg_set.empty());
  MsgSetWriter->OpenMsgSet(CompressionBuf, false);

  for (const TMsg::TPtr &msg_ptr : msg_set) {
    const TMsg &msg = *msg_ptr;
    size_t key_size = msg.GetKeySize();
    size_t value_size = msg.GetValueSize();
    MsgSetWriter->OpenMsg(TCompressionType::None, key_size, value_size);
    size_t key_offset = MsgSetWriter->GetCurrentMsgKeyOffset();
    assert(CompressionBuf.size() >= key_offset);
    assert((CompressionBuf.size() - key_offset) >= key_size);
    size_t value_offset = MsgSetWriter->GetCurrentMsgValueOffset();
    assert(CompressionBuf.size() >= value_offset);
    assert((CompressionBuf.size() - value_offset) == value_size);
    WriteKey(&CompressionBuf[0] + key_offset, msg);
    WriteValue(&CompressionBuf[0] + value_offset, msg);
    MsgSetWriter->CloseMsg();
    SerializeMsg.Increment();
  }

  MsgSetWriter->CloseMsgSet();
}

void TProduceRequestFactory::WriteOneMsgSet(const TMsgSet &msg_set,
    const TCompressionInfo &info, std::vector<uint8_t> &dst) {
  assert(this);

  if (info.CompressionCodec && (msg_set.DataSize >= info.MinCompressionSize)) {
    SerializeToCompressionBuf(msg_set.Contents);
    assert(info.CompressionCodec);
    const TCompressionCodecApi &codec = *info.CompressionCodec;
    bool msg_opened = false;

    try {
      /* Kafka compresses individual message sets.  A message set is compressed
         and encapsulated within a single message whose attributes are set to
         indicate that it contains a compressed message set. */
      size_t max_compressed_size = codec.ComputeCompressedResultBufSpace(
          &CompressionBuf[0], CompressionBuf.size(), info.CompressionLevel);
      RequestWriter->OpenMsg(info.CompressionType, 0,
          max_compressed_size);
      msg_opened = true;
      size_t value_offset = RequestWriter->GetCurrentMsgValueOffset();
      assert(dst.size() >= value_offset);
      assert((dst.size() - value_offset) == max_compressed_size);
      size_t compressed_size = codec.Compress(&CompressionBuf[0],
          CompressionBuf.size(), &dst[value_offset], max_compressed_size,
          info.CompressionLevel);
      /* If we get this far, compression finished without errors. */

      float compression_ratio = static_cast<float>(compressed_size) /
          static_cast<float>(CompressionBuf.size());

      if (compression_ratio <= MaxCompressionRatio) {
        /* Send the data compressed. */
        RequestWriter->AdjustValueSize(compressed_size);
        RequestWriter->CloseMsg();
        MsgSetCompressionYes.Increment();
        return;
      }

      /* If we get here, we wasted some CPU cycles on data that didn't compress
         very well.  Send it uncompressed so the broker avoids wasting more CPU
         cycles dealing with the compression.

         TODO: Add a per-topic compression statistics reporting feature to
         Dory's web interface.  This would facilitate identifying topics that
         don't compress very well, so Dory's compression config can be
         adjusted. */
      RequestWriter->RollbackOpenMsg();
      MsgSetNotCompressible.Increment();
    } catch (const TCompressionCodecApi::TError &x) {
      MsgSetCompressionError.Increment();
      LOG_R(TPri::ERR, std::chrono::seconds(30))
          << "Error compressing message set: " << x.what();

      if (msg_opened) {
        RequestWriter->RollbackOpenMsg();
      }

      /* As a fallback, send the data uncompressed. */
    }
  }

  SerializeUncompressedMsgSet(msg_set.Contents, dst);
  MsgSetCompressionNo.Increment();
}
