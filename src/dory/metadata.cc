/* <dory/metadata.cc>

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

   Implements <dory/metadata.h>.
 */

#include <dory/metadata.h>

#include <functional>
#include <memory>
#include <utility>

#include <base/time_util.h>
#include <log/log.h>
#include <server/counter.h>

using namespace Base;
using namespace Dory;
using namespace Log;

SERVER_COUNTER(MetadataDuplicateBrokerId);
SERVER_COUNTER(MetadataDuplicatePartition);
SERVER_COUNTER(MetadataDuplicateTopic);
SERVER_COUNTER(MetadataPartitionHasUnknownBroker);
SERVER_COUNTER(MetadataSanityCheckFail);
SERVER_COUNTER(MetadataSanityCheckSuccess);

bool TMetadata::TBroker::operator==(const TBroker &that) const {
  assert(this);
  return (Id == that.Id) && (Hostname == that.Hostname) &&
         (Port == that.Port) && (InService == that.InService);
}

TMetadata::TBuilder::TBuilder()
    : RandomEngine(GetEpochMilliseconds()) {
}

void TMetadata::TBuilder::OpenBrokerList() {
  assert(this);
  assert(State == TState::Initial);
  State = TState::AddingBrokers;
}

void TMetadata::TBuilder::AddBroker(int32_t kafka_id, std::string &&hostname,
    uint16_t port) {
  assert(this);
  assert(State == TState::AddingBrokers);
  auto result = BrokerMap.insert(std::make_pair(kafka_id, Brokers.size()));

  if (result.second) {
    Brokers.push_back(TBroker(kafka_id, std::move(hostname), port));
  } else {
    MetadataDuplicateBrokerId.Increment();
    LOG(TPri::WARNING) << "Ignoring duplicate broker ID " << kafka_id
        << " in metadata response";
  }
}

void TMetadata::TBuilder::CloseBrokerList() {
  assert(this);
  assert(State == TState::AddingBrokers);
  State = TState::AddingTopics;
}

bool TMetadata::TBuilder::OpenTopic(const std::string &name) {
  assert(this);
  assert(State == TState::AddingTopics);
  bool success =
      TopicNameToIndex.insert(std::make_pair(name, Topics.size())).second;

  if (success) {
    CurrentTopicIndex = Topics.size();
    Topics.push_back(TTopic());
    CurrentTopicPartitions.clear();
    State = TState::AddingOneTopic;
    assert(TopicNameToIndex.size() == Topics.size());
    CurrentTopicName = name;
  } else {
    MetadataDuplicateTopic.Increment();
    LOG(TPri::WARNING) << "Ignoring duplicate topic [" << name
        << "] in metadata response";
  }

  return success;
}

void TMetadata::TBuilder::AddPartitionToTopic(int32_t partition_id,
    int32_t broker_id, bool can_send_to_partition, int16_t error_code) {
  assert(this);
  assert(State == TState::AddingOneTopic);
  auto iter = BrokerMap.find(broker_id);

  if (iter == BrokerMap.end()) {
    MetadataPartitionHasUnknownBroker.Increment();
    LOG(TPri::WARNING) << "Ignoring partition ID " << partition_id
        << " for topic [" << CurrentTopicName
        << "] in metadata response due to unknown broker ID " << broker_id
        << ".  This can occur when a partition with only one replica resides "
        << "on a broker that is currently down.";
    return;
  }

  size_t broker_index = iter->second;

  if (broker_index >= Brokers.size()) {
    LOG(TPri::ERR) << "Bug!!! broker index " << broker_index
        << " in TMetadata::TBuilder::AddPartitionToTopic() is out of range: "
        << "size " << Brokers.size();
    assert(false);
    return;
  }

  auto ret = CurrentTopicPartitions.insert(partition_id);

  if (!ret.second) {
    MetadataDuplicatePartition.Increment();
    LOG(TPri::WARNING) << "Ignoring duplicate partition ID " << partition_id
        << " for topic [" << CurrentTopicName << "] in metadata response";
    return;
  }

  if (CurrentTopicIndex >= Topics.size()) {
    LOG(TPri::ERR) << "Bug!!! current topic index " << CurrentTopicIndex
        << " in TMetadata::TBuilder::AddPartitionToTopic() is out of range: "
        << "size " << Topics.size();
    assert(false);
    return;
  }

  TTopic &t = Topics[CurrentTopicIndex];

  if (can_send_to_partition) {
    t.OkPartitions.push_back(TPartition(partition_id, broker_index,
                                        error_code));
  } else {
    t.OutOfServicePartitions.push_back(TPartition(partition_id, broker_index,
                                                  error_code));
  }
}

void TMetadata::TBuilder::CloseTopic() {
  assert(this);
  assert(State == TState::AddingOneTopic);
  assert(TopicNameToIndex.size() == Topics.size());

  if (CurrentTopicIndex >= Topics.size()) {
    LOG(TPri::ERR) << "Bug!!! current topic index " << CurrentTopicIndex
        << " in TMetadata::TBuilder::CloseTopic() is out of range: size "
        << Topics.size();
    assert(false);
    return;
  }

  TTopic &t = Topics[CurrentTopicIndex];

  /* Group partitions first by broker index, and then in ascending order by
     Kafka partition ID.  Given a certain set of available partitions for a
     topic, we want their order in each broker's partition choices vector for
     that topic to be totally predictable.  Then clients who know the partition
     layout for a given topic can use that knowledge to send PartitionKey
     messages that target specific Kafka partition IDs. */
  std::sort(t.OkPartitions.begin(), t.OkPartitions.end(),
      [] (const TPartition &x, const TPartition &y) {
        if (x.GetBrokerIndex() != y.GetBrokerIndex()) {
           return (x.GetBrokerIndex() < y.GetBrokerIndex());
        }

        return (x.GetId() < y.GetId());
      });

  std::vector<TPartition>::const_iterator iter1 = t.OkPartitions.begin();
  auto iter2 = iter1;

  for (; ; ) {
    while ((iter2 != t.OkPartitions.end()) &&
           (iter2->GetBrokerIndex() == iter1->GetBrokerIndex())) {
      ++iter2;
    }

    if (iter2 == iter1) {
      assert(iter2 == t.OkPartitions.end());
      break;
    }

    size_t broker_index = iter1->GetBrokerIndex();

    if (broker_index >= Brokers.size()) {
      LOG(TPri::ERR) << "Bug!!! broker index " << broker_index
          << " in TMetadata::TBuilder::CloseTopic() is out of range: size "
          << Brokers.size();
      assert(false);
      return;
    }

    Brokers[broker_index].MarkInService();
    t.PartitionChoiceMap.insert(
        std::make_pair(broker_index,
            TPartitionChoices(TopicBrokerVec.size(),
                static_cast<size_t>(iter2 - iter1))));

    do {
      TopicBrokerVec.push_back(iter1->GetId());
    } while (++iter1 != iter2);
  }

  t.AllPartitions = t.OkPartitions;
  t.AllPartitions.insert(t.AllPartitions.end(),
      t.OutOfServicePartitions.begin(), t.OutOfServicePartitions.end());
  std::sort(t.AllPartitions.begin(), t.AllPartitions.end(),
      [](const TPartition &x, const TPartition &y) {
        return (x.GetId() < y.GetId());
      });

  /* For AnyPartition messages, a destination broker is chosen by cycling
     through this vector.  Shuffling its contents will cause different hosts to
     cycle through the brokers in different orders, which may have a somewhat
     beneficial effect on load distribution.  It's questionable whether this
     will make an observable difference in practice, but it doesn't hurt. */
  std::shuffle(t.OkPartitions.begin(), t.OkPartitions.end(), RandomEngine);

  CurrentTopicName.clear();
  State = TState::AddingTopics;
}

std::unique_ptr<TMetadata> TMetadata::TBuilder::Build() {
  assert(this);
  assert((State != TState::AddingBrokers) &&
         (State != TState::AddingOneTopic));
  assert(TopicNameToIndex.size() == Topics.size());
  GroupInServiceBrokers();
  assert(InServiceBrokerCount <= Brokers.size());
  std::unique_ptr<TMetadata> result(
      new TMetadata(std::move(Brokers), InServiceBrokerCount,
                    std::move(TopicBrokerVec), std::move(Topics),
                    std::move(TopicNameToIndex)));
  Reset();
  return result;
}

void TMetadata::TBuilder::GroupInServiceBrokers() {
  assert(this);

  /* Rearrange 'Brokers' vector so all out of service brokers are at the end.
   */

  std::vector<size_t> broker_index_reorder(Brokers.size());

  for (size_t i = 0; i < broker_index_reorder.size(); ++i) {
    broker_index_reorder[i] = i;
  }

  std::sort(broker_index_reorder.begin(), broker_index_reorder.end(),
            [this](size_t old_index_1, size_t old_index_2) {
              return Brokers[old_index_1].IsInService() &&
                     !Brokers[old_index_2].IsInService();
            });

  std::vector<TBroker> new_brokers;
  new_brokers.reserve(Brokers.size());

  for (size_t i = 0; i < Brokers.size(); ++i) {
    new_brokers.push_back(std::move(Brokers[broker_index_reorder[i]]));
  }

  Brokers = std::move(new_brokers);
  size_t i = 0;

  for (; (i < Brokers.size()) && Brokers[i].IsInService(); ++i);

  InServiceBrokerCount = i;

  /* Build vector 'old_indexes_to_new' so that if i is the old index of a
     broker, old_indexes_to_new[i] gives its new index. */

  std::vector<size_t> old_indexes_to_new(Brokers.size(), 0);

  for (i = 0; i < broker_index_reorder.size(); ++i) {
    old_indexes_to_new[broker_index_reorder[i]] = i;
  }

  /* Modify each topic to use the new broker indexes. */

  std::unordered_map<size_t, TPartitionChoices> new_partition_choice_map;

  for (auto &t : Topics) {
    for (auto &part : t.OkPartitions) {
      part.BrokerIndex = old_indexes_to_new[part.BrokerIndex];
    }

    for (auto &part : t.OutOfServicePartitions) {
      part.BrokerIndex = old_indexes_to_new[part.BrokerIndex];
    }

    for (auto &part : t.AllPartitions) {
      part.BrokerIndex = old_indexes_to_new[part.BrokerIndex];
    }

    new_partition_choice_map.reserve(t.PartitionChoiceMap.size());

    for (const auto &map_item : t.PartitionChoiceMap) {
      new_partition_choice_map.insert(
          std::make_pair(old_indexes_to_new[map_item.first], map_item.second));
    }

    assert(new_partition_choice_map.size() == t.PartitionChoiceMap.size());
    t.PartitionChoiceMap = std::move(new_partition_choice_map);
    assert(new_partition_choice_map.empty());
  }
}

bool TMetadata::operator==(const TMetadata &that) const {
  assert(this);

  if ((Brokers.size() != that.Brokers.size()) ||
      (TopicBrokerVec.size() != that.TopicBrokerVec.size()) ||
      (Topics.size() != that.Topics.size())) {
    return false;
  }

  assert(TopicNameToIndex.size() == Topics.size());
  assert(that.TopicNameToIndex.size() == that.Topics.size());


  return CompareBrokers(that) && CompareTopics(that);
}

int TMetadata::FindTopicIndex(const std::string &topic) const {
  assert(this);
  auto iter = TopicNameToIndex.find(topic);

  if (iter == TopicNameToIndex.end()) {
    return -1;
  }

  size_t topic_index = iter->second;

  if (topic_index >= Topics.size()) {
    LOG(TPri::ERR) << "Bug!!! Topic index " << topic_index
        << " in metadata is out of range (size is " << Topics.size()
        << ") in TMetadata::FindTopicIndex()";
    assert(false);
    return -1;
  }

  return static_cast<int>(topic_index);
}

const int32_t *TMetadata::FindPartitionChoices(const std::string &topic,
    size_t broker_index, size_t &num_choices) const {
  assert(this);
  num_choices = 0;
  int topic_index = FindTopicIndex(topic);

  if (topic_index < 0) {
    LOG(TPri::ERR) << "Bug!!! Bad topic " << topic
        << " passed to TMetadata::FindPartitionChoices()";
    assert(false);
    return nullptr;
  }

  assert(static_cast<size_t>(topic_index) < Topics.size());
  const TTopic &t = Topics[topic_index];

  if (broker_index >= Brokers.size()) {
    LOG(TPri::ERR) << "Bug!!! Broker index " << broker_index
        << " passed to TMetadata::FindPartitionChoices() is out of range "
        << "(size is " << Brokers.size() << ")";
    assert(false);
    return nullptr;
  }

  auto iter = t.PartitionChoiceMap.find(broker_index);

  if (iter == t.PartitionChoiceMap.end()) {
    return nullptr;
  }

  const TPartitionChoices &choices = iter->second;
  size_t choices_index = choices.GetTopicBrokerVecIndex();
  size_t choices_count = choices.GetTopicBrokerVecNumItems();

  if (choices_index >= TopicBrokerVec.size()) {
    LOG(TPri::ERR) << "Bug!!! Choices index " << choices_index
        << " is out of range (size is " << TopicBrokerVec.size()
        << ") in TMetadata::FindPartitionChoices()";
    assert(false);
    return nullptr;
  }

  if (choices_count > (TopicBrokerVec.size() - choices_index)) {
    LOG(TPri::ERR) << "Bug!!! Choices count " << choices_count
        << " is out of range (size is " << TopicBrokerVec.size()
        << ", index is " << choices_index
        << ") in TMetadata::FindPartitionChoices()";
    assert(false);
    return nullptr;
  }

  if (choices_count == 0) {
    LOG(TPri::ERR)
        << "Bug!!! Choices count is 0 in TMetadata::FindPartitionChoices()";
    assert(false);
    return nullptr;
  }

  num_choices = choices_count;
  return &TopicBrokerVec[choices_index];
}

bool TMetadata::SanityCheckOkPartitions(const TTopic &t,
    std::unordered_set<size_t> &in_service_broker_indexes,
    std::unordered_set<int32_t> &id_set_ok,
    std::unordered_map<size_t, std::unordered_set<int32_t>>
        &broker_partition_map) const {
  assert(this);

  for (const TPartition &p : t.OkPartitions) {
    id_set_ok.insert(p.Id);

    if (p.BrokerIndex >= Brokers.size()) {
      LOG(TPri::ERR)
          << "Bug!!! OkPartitions item has out of range BrokerIndex";
      return false;
    }

    auto ret = broker_partition_map.insert(
        std::make_pair(p.BrokerIndex, std::unordered_set<int32_t>()));
    ret.first->second.insert(p.Id);
    in_service_broker_indexes.insert(p.BrokerIndex);

    const auto iter = std::lower_bound(t.AllPartitions.begin(),
        t.AllPartitions.end(), p,
        [](const TPartition &x, const TPartition &y) {
          return (x.Id < y.Id);
        });

    if ((iter == t.AllPartitions.end()) || (iter->Id != p.Id)) {
      LOG(TPri::ERR) << "Bug!!! OkPartitions item not found in AllPartitions";
      return false;
    }

    if ((iter->BrokerIndex != p.BrokerIndex) ||
        (iter->ErrorCode != p.ErrorCode)) {
      LOG(TPri::ERR)
          << "Bug!!! OkPartitions item ErrorCode does not match corresponding "
          << "ErrorCode in AllPartitions";
      return false;
    }
  }

  if (id_set_ok.size() != t.OkPartitions.size()) {
    LOG(TPri::ERR) << "Bug!!! OkPartitions has duplicate IDs";
    return false;
  }

  return true;
}

bool TMetadata::SanityCheckOutOfServicePartitions(const TTopic &t,
    std::unordered_set<int32_t> &id_set_bad) const {
  assert(this);

  for (const TPartition &p : t.OutOfServicePartitions) {
    id_set_bad.insert(p.Id);

    if (p.BrokerIndex >= Brokers.size()) {
      LOG(TPri::ERR)
          << "Bug!!! OutOfServicePartitions item has out of range BrokerIndex";
      return false;
    }

    const auto iter = std::lower_bound(t.AllPartitions.begin(),
        t.AllPartitions.end(), p,
        [](const TPartition &x, const TPartition &y) {
          return (x.Id < y.Id);
        });

    if ((iter == t.AllPartitions.end()) || (iter->Id != p.Id)) {
      LOG(TPri::ERR)
          << "Bug!!! OutOfServicePartitions item not found in AllPartitions";
      return false;
    }

    if ((iter->BrokerIndex != p.BrokerIndex) ||
        (iter->ErrorCode != p.ErrorCode)) {
      LOG(TPri::ERR)
          << "Bug!!! OutOfServicePartitions item ErrorCode does not match "
          << "corresponding ErrorCode in AllPartitions";
      return false;
    }
  }

  if (id_set_bad.size() != t.OutOfServicePartitions.size()) {
    LOG(TPri::ERR) << "Bug!!! OutOfServicePartitions has duplicate IDs";
    return false;
  }

  return true;
}

bool TMetadata::SanityCheckBrokerPartitionMap(const TTopic &t,
    const std::unordered_map<size_t, std::unordered_set<int32_t>>
        &broker_partition_map,
        std::vector<size_t> &topic_broker_vec_access) const {
  assert(this);

  for (const auto &map_item : broker_partition_map) {
    auto iter = t.PartitionChoiceMap.find(map_item.first);

    if (iter == t.PartitionChoiceMap.end()) {
      LOG(TPri::ERR) << "Bug!!! Broker index missing from PartitionChoiceMap";
      return false;
    }

    const auto &choices = iter->second;
    size_t chunk_index = choices.GetTopicBrokerVecIndex();
    size_t chunk_size = choices.GetTopicBrokerVecNumItems();

    if (chunk_index >= TopicBrokerVec.size()) {
      LOG(TPri::ERR) << "Bug!!! chunk_index >= TopicBrokerVec.size()";
      return false;
    }

    if (chunk_size > (TopicBrokerVec.size() - chunk_index)) {
      LOG(TPri::ERR)
          << "Bug!!! chunk_size > (TopicBrokerVec.size() - chunk_index)";
      return false;
    }

    const int32_t *chunk_begin = &TopicBrokerVec[chunk_index];
    std::unordered_set<int32_t> partition_id_set(chunk_begin,
        chunk_begin + chunk_size);

    if (partition_id_set != map_item.second) {
      LOG(TPri::ERR)
          << "Bug!!! Partition choices referenced by PartitionChoiceMap do "
          << "not match partition IDs from OkPartitions";
      return false;
    }

    for (size_t i = 0; i < chunk_size; ++i) {
      /* For each topic/broker combination, the array of available Kafka
         partition IDs must be sorted in ascending order, so clients with
         knowledge of the partition layout can rely on this ordering if they
         want to send PartitionKey messages that target specific partition
         IDs. */
      if ((i > 0) && (chunk_begin[i] <= chunk_begin[i - 1])) {
        LOG(TPri::ERR)
            << "Bug!!! Unsorted or duplicate partition IDs in TopicBrokerVec "
            << "chunk";
        return false;
      }

      ++topic_broker_vec_access[chunk_index + i];
    }
  }

  return true;
}

bool TMetadata::SanityCheckOneTopic(const TTopic &t,
    std::unordered_set<size_t> &in_service_broker_indexes,
    std::vector<size_t> &topic_broker_vec_access) const {
  assert(this);

  if (t.AllPartitions.size() !=
      (t.OkPartitions.size() + t.OutOfServicePartitions.size())) {
    LOG(TPri::ERR)
        << "Bug!!! AllPartitions.size() != OkPartitions.size() + "
        << "OutOfServicePartitions.size()";
    return false;
  }

  for (size_t i = 1; i < t.AllPartitions.size(); ++i) {
    if (t.AllPartitions[i].Id <= t.AllPartitions[i - 1].Id) {
      LOG(TPri::ERR)
          << "Bug!!! AllPartitions is unsorted or has duplicate IDs";
      return false;
    }
  }

  std::unordered_set<int32_t> id_set_ok;
  std::unordered_map<size_t, std::unordered_set<int32_t>> broker_partition_map;

  if (!SanityCheckOkPartitions(t, in_service_broker_indexes, id_set_ok,
      broker_partition_map)) {
    return false;
  }

  std::unordered_set<int32_t> id_set_bad;

  if (!SanityCheckOutOfServicePartitions(t, id_set_bad)) {
    return false;
  }

  for (auto id : id_set_ok) {
    if (id_set_bad.find(id) != id_set_bad.end()) {
      LOG(TPri::ERR)
          << "Bug!!! Same ID appears in both OkPartitions and "
          << "OutOfServicePartitions";
      return false;
    }
  }

  if (broker_partition_map.size() != t.PartitionChoiceMap.size()) {
    LOG(TPri::ERR)
        << "Bug!!! broker_partition_map.size() != t.PartitionChoiceMap.size()";
    return false;
  }

  return SanityCheckBrokerPartitionMap(t, broker_partition_map,
      topic_broker_vec_access);
}

bool TMetadata::SanityCheckTopics(
    std::unordered_set<size_t> &in_service_broker_indexes) const {
  assert(this);
  std::unordered_set<size_t> topic_indexes;

  for (const auto &map_item : TopicNameToIndex) {
    if (map_item.second >= Topics.size()) {
      LOG(TPri::ERR) << "Bug!!! Index out of range in TopicNameToIndex";
      return false;
    }

    topic_indexes.insert(map_item.second);
  }

  if (topic_indexes.size() != Topics.size()) {
    LOG(TPri::ERR)
        << "Bug!!! TopicNameToIndex has wrong size and/or dupicate indexes";
    return false;
  }

  std::vector<size_t> topic_broker_vec_access(TopicBrokerVec.size(), 0);

  for (const TTopic &t : Topics) {
    if (!SanityCheckOneTopic(t, in_service_broker_indexes,
        topic_broker_vec_access)) {
      return false;
    }
  }

  for (size_t value : topic_broker_vec_access) {
    if (value != 1) {
      LOG(TPri::ERR) << "Bug!!! TopicBrokerVec item access count is wrong";
      return false;
    }
  }

  return true;
}

bool TMetadata::SanityCheck() const {
  bool result = DoSanityCheck();

  if (result) {
    MetadataSanityCheckSuccess.Increment();
  } else {
    MetadataSanityCheckFail.Increment();
  }

  return result;
}

bool TMetadata::DoSanityCheck() const {
  assert(this);
  std::unordered_set<size_t> in_service_broker_indexes;

  if (!SanityCheckTopics(in_service_broker_indexes)) {
    return false;
  }

  size_t in_svc_count = 0;
  bool found_out_of_svc = false;
  size_t adjacent_in_svc_count = 0;

  for (size_t i = 0; i < Brokers.size(); ++i) {
    const TBroker &b = Brokers[i];
    auto iter = in_service_broker_indexes.find(i);
    bool in_service = (iter != in_service_broker_indexes.end());

    if (b.IsInService()) {
      ++in_svc_count;

      if (!found_out_of_svc) {
        ++adjacent_in_svc_count;
      }
    } else {
      found_out_of_svc = true;
    }

    if (b.IsInService() != in_service) {
      LOG(TPri::ERR)
          << "Bug!!! TBroker IsInService() method returns wrong result";
      return false;
    }
  }

  if (NumInServiceBrokers() != in_svc_count) {
    LOG(TPri::ERR)
        << "Bug!!! NumInServiceBrokers() does not agree with count of brokers "
        << "for which IsInService() returns true";
    return false;
  }

  if (adjacent_in_svc_count != in_svc_count) {
    LOG(TPri::ERR)
        << "Bug!!! In service brokers should all be grouped at start of "
        << "Brokers vector";
  }

  return true;
}

bool TMetadata::CompareBrokers(const TMetadata &that) const {
  assert(this);

  struct t_hasher {
    size_t operator()(const TBroker &broker) const {
      assert(this);
      return StringHash(broker.Hostname) ^
             Int32Hash(broker.Id ^ broker.Port ^ broker.InService);
    }

    std::hash<std::string> StringHash;
    std::hash<int32_t> Int32Hash;
  };  // t_hasher

  std::unordered_set<TBroker, t_hasher>
      broker_set(Brokers.begin(), Brokers.end());
  assert(broker_set.size() == Brokers.size());

  for (const TBroker &b : that.Brokers) {
    if (broker_set.find(b) == broker_set.end()) {
      return false;
    }
  }

  return true;
}

bool TMetadata::SingleTopicCompare(const TMetadata &that,
    const TTopic &this_topic, const TTopic &that_topic) const {
  assert(this);

  struct t_part {
    int32_t Id;
    int32_t BrokerId;
    int16_t ErrorCode;

    t_part(int32_t id, int32_t broker_id, int16_t error_code)
        : Id(id),
          BrokerId(broker_id),
          ErrorCode(error_code) {
    }

    t_part(const t_part &) = default;
    t_part &operator=(const t_part &) = default;

    bool operator==(const t_part &that) const {
      assert(this);
      return (Id == that.Id) && (BrokerId == that.BrokerId) &&
             (ErrorCode == that.ErrorCode);
    }

    bool operator!=(const t_part &that) const {
      assert(this);
      return !(*this == that);
    }
  };  // t_part

  struct t_hasher {
    size_t operator()(const t_part &part) const {
      assert(this);
      return Int32Hash(part.Id ^ part.BrokerId ^ part.ErrorCode);
    }

    std::hash<int32_t> Int32Hash;
  };  // t_hasher

  if ((this_topic.OkPartitions.size() != that_topic.OkPartitions.size()) ||
      (this_topic.OutOfServicePartitions.size() !=
       that_topic.OutOfServicePartitions.size())) {
    return false;
  }

  std::unordered_set<t_part, t_hasher> part_set;

  for (const TPartition &p : this_topic.OkPartitions) {
    part_set.insert(t_part(p.Id, Brokers[p.BrokerIndex].GetId(), p.ErrorCode));
  }

  assert(part_set.size() == this_topic.OkPartitions.size());

  for (const TPartition &p : that_topic.OkPartitions) {
    if (part_set.find(
            t_part(p.Id, that.Brokers[p.BrokerIndex].GetId(), p.ErrorCode)) ==
        part_set.end()) {
      return false;
    }
  }

  part_set.clear();

  for (const TPartition &p : this_topic.OutOfServicePartitions) {
    part_set.insert(t_part(p.Id, Brokers[p.BrokerIndex].GetId(), p.ErrorCode));
  }

  assert(part_set.size() == this_topic.OutOfServicePartitions.size());

  for (const TPartition &p : that_topic.OutOfServicePartitions) {
    if (part_set.find(
            t_part(p.Id, that.Brokers[p.BrokerIndex].GetId(), p.ErrorCode)) ==
        part_set.end()) {
      return false;
    }
  }

  return true;
}

bool TMetadata::CompareTopics(const TMetadata &that) const {
  assert(this);

  for (const auto &map_item : TopicNameToIndex) {
    auto iter = that.TopicNameToIndex.find(map_item.first);

    if ((iter == that.TopicNameToIndex.end()) ||
        !SingleTopicCompare(that, Topics[map_item.second],
                            that.Topics[iter->second])) {
      return false;
    }
  }

  return true;
}
