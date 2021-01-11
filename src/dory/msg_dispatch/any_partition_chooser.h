/* <dory/msg_dispatch/any_partition_chooser.h>

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

   Class used for choosing a partition for AnyPartition messages.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include <dory/metadata.h>

namespace Dory {

  namespace MsgDispatch {

    class TAnyPartitionChooser final {
      public:
      TAnyPartitionChooser() = default;

      int32_t GetChoice(size_t broker_index, const TMetadata &md,
          const std::string &topic) {
        if (!Choice) {
          Choose(broker_index, md, topic);
        }

        return *Choice;
      }

      void SetChoiceUsed() {
        ChoiceUsed = true;
      }

      void ClearChoice() {
        Choice.reset();

        if (ChoiceUsed) {
          ++Count;
          ChoiceUsed = false;
        }
      }

      private:
      void Choose(size_t broker_index, const TMetadata &md,
          const std::string &topic);

      size_t Count = 0;

      std::optional<int32_t> Choice;

      bool ChoiceUsed = false;
    };  // TAnyPartitionChooser

  }  // MsgDispatch

}  // Dory
