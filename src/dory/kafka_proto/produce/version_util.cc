/* <dory/kafka_proto/produce/version_util.cc>

   ----------------------------------------------------------------------------
   Copyright 2013-2014 if(we)
   Copyright 2017 Dave Peterson <dave@dspeterson.com>

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

   Implements <dory/kafka_proto/produce/version_util.h>
 */

#include <dory/kafka_proto/produce/version_util.h>

#include <algorithm>

#include <dory/kafka_proto/produce/v0/produce_proto.h>

using namespace Dory;
using namespace Dory::KafkaProto;
using namespace Dory::KafkaProto::Produce;

TProduceProtocol *Dory::KafkaProto::Produce::ChooseProduceProto(
    size_t api_version) {
  if (api_version == 0) {
    return new Dory::KafkaProto::Produce::V0::TProduceProto;
  }

  return nullptr;  // unsupported API version
}

const std::vector<size_t> &
Dory::KafkaProto::Produce::GetSupportedProduceApiVersions() {
  static const std::vector<size_t> supported_versions = { 0 };
  return supported_versions;
}

bool Dory::KafkaProto::Produce::IsProduceApiVersionSupported(
    size_t api_version) {
  const std::vector<size_t> &v = GetSupportedProduceApiVersions();
  return std::find(v.begin(), v.end(), api_version) != v.end();
}
