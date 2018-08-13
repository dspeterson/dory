/* <dory/kafka_proto/produce/v0/produce_request_writer.h>

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

   Class for writing a produce request to a caller-supplied growable buffer of
   type std::vector<uint8_t>.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <base/field_access.h>
#include <base/no_copy_semantics.h>
#include <dory/compress/compression_type.h>
#include <dory/kafka_proto/produce/produce_request_writer_api.h>
#include <dory/kafka_proto/produce/v0/msg_set_writer.h>
#include <dory/kafka_proto/produce/v0/produce_request_constants.h>

namespace Dory {

  namespace KafkaProto {

    namespace Produce {

      namespace V0 {

        class TProduceRequestWriter final : public TProduceRequestWriterApi {
          NO_COPY_SEMANTICS(TProduceRequestWriter);

          public:
          TProduceRequestWriter();

          ~TProduceRequestWriter() noexcept override = default;

          void Reset() override;

          void OpenRequest(std::vector<uint8_t> &result_buf, int32_t corr_id,
              const char *client_id_begin, const char *client_id_end,
              int16_t required_acks, int32_t replication_timeout) override;

          void OpenTopic(const char *topic_name_begin,
              const char *topic_name_end) override;

          void OpenMsgSet(int32_t partition) override;

          void OpenMsg(Compress::TCompressionType compression_type,
              size_t key_size, size_t value_size) override;

          size_t GetCurrentMsgKeyOffset() const override;

          size_t GetCurrentMsgValueOffset() const override;

          void AdjustValueSize(size_t new_size) override;

          void RollbackOpenMsg() override;

          void CloseMsg() override;

          void AddMsg(Compress::TCompressionType compression_type,
              const uint8_t *key_begin, const uint8_t *key_end,
              const uint8_t *value_begin, const uint8_t *value_end) override;

          void CloseMsgSet() override;

          void CloseTopic() override;

          void CloseRequest() override;

          private:
          using PRC = TProduceRequestConstants;

          enum class TState {
            Idle,
            InRequest,
            InTopic,
            InMsgSet
          };  // TState

          void WriteInt8(size_t offset, int8_t value) {
            assert(this);
            assert(Buf);
            assert(Buf->size() > offset);
            (*Buf)[offset] = static_cast<uint8_t>(value);
          }

          void WriteInt8AtOffset(int8_t value) {
            assert(this);
            WriteInt8(AtOffset, value);
            ++AtOffset;
          }

          void WriteInt16(size_t offset, int16_t value) {
            assert(this);
            assert(Buf);
            assert(Buf->size() > (offset + 1));
            WriteInt16ToHeader(&(*Buf)[offset], value);
          }

          void WriteInt16AtOffset(int16_t value) {
            assert(this);
            WriteInt16(AtOffset, value);
            AtOffset += 2;
          }

          void WriteInt32(size_t offset, int32_t value) {
            assert(this);
            assert(Buf);
            assert(Buf->size() > (offset + 3));
            WriteInt32ToHeader(&(*Buf)[offset], value);
          }

          void WriteInt32AtOffset(int32_t value) {
            assert(this);
            WriteInt32(AtOffset, value);
            AtOffset += 4;
          }

          void WriteInt64(size_t offset, int64_t value) {
            assert(this);
            assert(Buf);
            assert(Buf->size() > (offset + 7));
            WriteInt64ToHeader(&(*Buf)[offset], value);
          }

          void WriteInt64AtOffset(int64_t value) {
            assert(this);
            WriteInt64(AtOffset, value);
            AtOffset += 8;
          }

          void WriteData(size_t offset, const void *data, size_t data_size) {
            assert(this);
            assert(Buf);
            assert(Buf->size() > (offset + data_size - 1));
            std::memcpy(&(*Buf)[offset], data, data_size);
          }

          void WriteDataAtOffset(const void *data, size_t data_size) {
            assert(this);
            WriteData(AtOffset, data, data_size);
            AtOffset += data_size;
          }

          std::vector<uint8_t> *Buf;

          TState State;

          size_t AtOffset;

          size_t TopicCountOffset;

          size_t FirstTopicOffset;

          size_t CurrentTopicOffset;

          size_t CurrentTopicPartitionCountOffset;

          size_t TopicCount;

          size_t FirstPartitionOffset;

          size_t CurrentPartitionOffset;

          size_t PartitionCount;

          TMsgSetWriter MsgSetWriter;
        };  // TProduceRequestWriter

      }  // V0

    }  // Produce

  }  // KafkaProto

}  // Dory
