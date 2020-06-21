/* <dory/mock_kafka_server/prod_req/prod_req_builder.cc>

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

   Implements <dory/mock_kafka_server/prod_req/prod_req_builder.h>.
 */

#include <dory/mock_kafka_server/prod_req/prod_req_builder.h>

#include <string>

#include <dory/compress/gzip/gzip_codec.h>
#include <dory/compress/lz4/lz4_codec.h>
#include <dory/compress/snappy/snappy_codec.h>

using namespace Dory;
using namespace Dory::Compress;
using namespace Dory::Compress::Gzip;
using namespace Dory::Compress::Lz4;
using namespace Dory::Compress::Snappy;
using namespace Dory::KafkaProto;
using namespace Dory::MockKafkaServer;
using namespace Dory::MockKafkaServer::ProdReq;

TProdReq TProdReqBuilder::BuildProdReq(const void *request,
    size_t request_size) {
  RequestReader.SetRequest(request, request_size);
  std::string client_id(RequestReader.GetClientIdBegin(),
                        RequestReader.GetClientIdEnd());
  TProdReq prod_req(RequestReader.GetCorrelationId(), std::move(client_id),
      RequestReader.GetRequiredAcks(), RequestReader.GetReplicationTimeout());

  while (RequestReader.NextTopic()) {
    prod_req.AddTopicGroup(BuildTopicGroup());
  }

  RequestReader.Clear();
  MsgSetReader.Clear();
  return prod_req;
}

TTopicGroup TProdReqBuilder::BuildTopicGroup() {
  std::string topic(RequestReader.GetCurrentTopicNameBegin(),
                    RequestReader.GetCurrentTopicNameEnd());
  TTopicGroup topic_group(std::move(topic));

  while (RequestReader.NextMsgSetInTopic()) {
    topic_group.AddMsgSet(BuildMsgSet());
  }

  return topic_group;
}

void TProdReqBuilder::GetCompressedData(const std::vector<TMsg> &msg_vec,
    std::vector<uint8_t> &result) {
  if (!msg_vec.empty()) {
    throw TCompressedMsgSetNotAlone();
  }

  if (RequestReader.GetCurrentMsgKeyEnd() !=
      RequestReader.GetCurrentMsgKeyBegin()) {
    throw TCompressedMsgSetMustHaveEmptyKey();
  }

  result.assign(RequestReader.GetCurrentMsgValueBegin(),
                RequestReader.GetCurrentMsgValueEnd());

  if (RequestReader.NextMsgInMsgSet()) {
    throw TCompressedMsgSetNotAlone();
  }
}

void TProdReqBuilder::GzipUncompressMsgSet(
    const std::vector<uint8_t> &compressed_data,
    std::vector<uint8_t> &uncompressed_data) {
  const TGzipCodec &codec = TGzipCodec::The();

  try {
    uncompressed_data.resize(codec.ComputeUncompressedResultBufSpace(
        &compressed_data[0], compressed_data.size()));
    size_t size = codec.Uncompress(&compressed_data[0], compressed_data.size(),
        &uncompressed_data[0], uncompressed_data.size());
    uncompressed_data.resize(size);
  } catch (const TCompressionCodecApi::TError &x) {
    throw TUncompressFailed();
  }
}

void TProdReqBuilder::SnappyUncompressMsgSet(
    const std::vector<uint8_t> &compressed_data,
    std::vector<uint8_t> &uncompressed_data) {
  const TSnappyCodec &codec = TSnappyCodec::The();

  try {
    uncompressed_data.resize(codec.ComputeUncompressedResultBufSpace(
        &compressed_data[0], compressed_data.size()));
    size_t size = codec.Uncompress(&compressed_data[0], compressed_data.size(),
        &uncompressed_data[0], uncompressed_data.size());
    uncompressed_data.resize(size);
  } catch (const TCompressionCodecApi::TError &x) {
    throw TUncompressFailed();
  }
}

void TProdReqBuilder::Lz4UncompressMsgSet(
    const std::vector<uint8_t> &compressed_data,
    std::vector<uint8_t> &uncompressed_data) {
  const TLz4Codec &codec = TLz4Codec::The();

  try {
    uncompressed_data.resize(codec.ComputeUncompressedResultBufSpace(
        &compressed_data[0], compressed_data.size()));
    size_t size = codec.Uncompress(&compressed_data[0], compressed_data.size(),
        &uncompressed_data[0], uncompressed_data.size());
    uncompressed_data.resize(size);
  } catch (const TCompressionCodecApi::TError &x) {
    throw TUncompressFailed();
  }
}

TMsgSet TProdReqBuilder::BuildUncompressedMsgSet(int32_t partition,
    const std::vector<uint8_t> &msg_set_data,
    TCompressionType compression_type) {
  TMsgSet msg_set(partition);
  MsgSetReader.SetMsgSet(&msg_set_data[0], msg_set_data.size());

  while (MsgSetReader.NextMsg()) {
    if (!MsgSetReader.CurrentMsgCrcIsOk()) {
      msg_set.AddMsg(TMsg(false, nullptr, nullptr, nullptr, nullptr));
      continue;
    }

    if (MsgSetReader.GetCurrentMsgCompressionType() !=
        TCompressionType::None) {
      throw TCompressedMsgInvalidAttributes();
    }

    msg_set.AddMsg(TMsg(true, MsgSetReader.GetCurrentMsgKeyBegin(),
        MsgSetReader.GetCurrentMsgKeyEnd(),
        MsgSetReader.GetCurrentMsgValueBegin(),
        MsgSetReader.GetCurrentMsgValueEnd()));
  }

  msg_set.SetCompressionType(compression_type);
  return msg_set;
}

TMsgSet TProdReqBuilder::BuildMsgSet() {
  int32_t partition = RequestReader.GetPartitionOfCurrentMsgSet();
  TMsgSet msg_set(partition);
  const std::vector<TMsg> &msg_vec = msg_set.GetMsgVec();
  std::vector<uint8_t> compressed_data, uncompressed_data;

  while (RequestReader.NextMsgInMsgSet()) {
    if (!RequestReader.CurrentMsgCrcIsOk()) {
      msg_set.AddMsg(TMsg(false, nullptr, nullptr, nullptr, nullptr));
      continue;
    }

    switch (RequestReader.GetCurrentMsgCompressionType()) {
      case TCompressionType::None: {  // no compression
        break;
      }
      case TCompressionType::Gzip: {  // gzip compression
        GetCompressedData(msg_vec, compressed_data);
        GzipUncompressMsgSet(compressed_data, uncompressed_data);
        return BuildUncompressedMsgSet(partition, uncompressed_data,
                                       TCompressionType::Gzip);
      }
      case TCompressionType::Snappy: {  // snappy compression
        GetCompressedData(msg_vec, compressed_data);
        SnappyUncompressMsgSet(compressed_data, uncompressed_data);
        return BuildUncompressedMsgSet(partition, uncompressed_data,
                                       TCompressionType::Snappy);
      }
      case TCompressionType::Lz4: {  // lz4 compression
        GetCompressedData(msg_vec, compressed_data);
        Lz4UncompressMsgSet(compressed_data, uncompressed_data);
        return BuildUncompressedMsgSet(partition, uncompressed_data,
                                       TCompressionType::Lz4);
      }
      default: {
        throw TInvalidAttributes();
      }
    }

    msg_set.AddMsg(TMsg(true, RequestReader.GetCurrentMsgKeyBegin(),
        RequestReader.GetCurrentMsgKeyEnd(),
        RequestReader.GetCurrentMsgValueBegin(),
        RequestReader.GetCurrentMsgValueEnd()));
  }

  return msg_set;
}
