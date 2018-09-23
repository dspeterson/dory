/* <dory/mock_kafka_server/prod_req/prod_req_builder.h>

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

   Class for building a TProdReq from a produce request.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <base/no_copy_semantics.h>
#include <dory/compress/compression_type.h>
#include <dory/kafka_proto/produce/msg_set_reader_api.h>
#include <dory/kafka_proto/produce/produce_request_reader_api.h>
#include <dory/mock_kafka_server/prod_req/msg.h>
#include <dory/mock_kafka_server/prod_req/msg_set.h>
#include <dory/mock_kafka_server/prod_req/prod_req.h>
#include <dory/mock_kafka_server/prod_req/topic_group.h>

namespace Dory {

  namespace MockKafkaServer {

    namespace ProdReq {

      class TProdReqBuilder final {
        NO_COPY_SEMANTICS(TProdReqBuilder);

        public:
        class TBuildError
            : public KafkaProto::Produce::TProduceRequestReaderApi::
                  TBadProduceRequest {
          public:
          ~TBuildError() override = default;

          protected:
          explicit TBuildError(const char *msg)
              : TBadProduceRequest(msg) {
          }
        };  // TBuildError

        class TInvalidAttributes : public TBuildError {
          public:
          ~TInvalidAttributes() override = default;

          TInvalidAttributes()
              : TBuildError(
                  "Message has invalid attributes") {
          }
        };  // TInvalidAttributes

        class TCompressedMsgInvalidAttributes : public TBuildError {
          public:
          ~TCompressedMsgInvalidAttributes() override = default;

          TCompressedMsgInvalidAttributes()
              : TBuildError(
                  "Compressed message has invalid attributes") {
          }
        };  // TCompressedMsgInvalidAttributes

        class TCompressedMsgSetNotAlone : public TBuildError {
          public:
          ~TCompressedMsgSetNotAlone() override = default;

          TCompressedMsgSetNotAlone()
              : TBuildError(
                  "Compressed message set must not be part of a message set "
                  "containing other messages") {
          }
        };  // TCompressedMsgSetNotAlone

        class TCompressedMsgSetMustHaveEmptyKey : public TBuildError {
          public:
          ~TCompressedMsgSetMustHaveEmptyKey() override = default;

          TCompressedMsgSetMustHaveEmptyKey()
              : TBuildError(
                  "Compressed message set must have empty key") {
          }
        };  // TCompressedMsgSetMustHaveEmptyKey

        class TUncompressFailed : public TBuildError {
          public:
          ~TUncompressFailed() override = default;

          TUncompressFailed()
              : TBuildError("Failed to uncompress message set") {
          }
        };  // TUncompressFailed

        TProdReqBuilder(
            KafkaProto::Produce::TProduceRequestReaderApi &request_reader,
            KafkaProto::Produce::TMsgSetReaderApi &msg_set_reader)
            : RequestReader(request_reader),
              MsgSetReader(msg_set_reader) {
        }

        TProdReq BuildProdReq(const void *request, size_t request_size);

        private:
        TTopicGroup BuildTopicGroup();

        void GetCompressedData(const std::vector<TMsg> &msg_vec,
            std::vector<uint8_t> &result);

        void GzipUncompressMsgSet(
            const std::vector<uint8_t> &compressed_data,
            std::vector<uint8_t> &uncompressed_data);

        void SnappyUncompressMsgSet(
            const std::vector<uint8_t> &compressed_data,
            std::vector<uint8_t> &uncompressed_data);

        void Lz4UncompressMsgSet(
            const std::vector<uint8_t> &compressed_data,
            std::vector<uint8_t> &uncompressed_data);

        TMsgSet BuildUncompressedMsgSet(int32_t partition,
            const std::vector<uint8_t> &msg_set_data,
            Compress::TCompressionType compression_type);

        TMsgSet BuildMsgSet();

        KafkaProto::Produce::TProduceRequestReaderApi &RequestReader;

        KafkaProto::Produce::TMsgSetReaderApi &MsgSetReader;
      };  // TProdReqBuilder

    }  // ProdReq

  }  // MockKafkaServer

}  // Dory
