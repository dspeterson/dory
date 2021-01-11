/* <dory/msg_dispatch/any_partition_chooser.cc>

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

   Implements <dory/msg_dispatch/any_partition_chooser.h>.
 */

#include <dory/msg_dispatch/any_partition_chooser.h>

using namespace Dory;
using namespace Dory::MsgDispatch;

void TAnyPartitionChooser::Choose(size_t broker_index, const TMetadata &md,
    const std::string &topic) {
  size_t num_choices = 0;
  const int32_t *choice_vec =
      md.FindPartitionChoices(topic, broker_index, num_choices);
  Choice.emplace(choice_vec[Count % num_choices]);
}
