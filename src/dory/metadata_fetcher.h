/* <dory/metadata_fetcher.h>

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

   Class for getting metadata from a Kafka broker.
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <netinet/in.h>

#include <base/fd.h>
#include <base/no_copy_semantics.h>
#include <base/stream_msg_with_size_reader.h>
#include <dory/kafka_proto/metadata/metadata_protocol.h>
#include <dory/metadata.h>

namespace Dory {

  class TMetadataFetcher final {
    NO_COPY_SEMANTICS(TMetadataFetcher);

    public:
    /* RAII convenience class for disconnecting. */
    class TDisconnecter final {
      NO_COPY_SEMANTICS(TDisconnecter);

      public:
      explicit TDisconnecter(TMetadataFetcher &fetcher)
          : Fetcher(fetcher) {
      }

      ~TDisconnecter() {
        Fetcher.Disconnect();
      }

      TMetadataFetcher &Fetcher;
    };  // TDisconnecter

    explicit TMetadataFetcher(
        const KafkaProto::Metadata::TMetadataProtocol *metadata_protocol);

    /* Return true on success or false on failure. */
    bool Connect(const char *host_name, in_port_t port);

    /* Return true on success or false on failure. */
    bool Connect(const std::string &host_name, in_port_t port) {
      assert(this);
      return Connect(host_name.c_str(), port);
    }

    void Disconnect() noexcept {
      assert(this);
      Sock.Reset();
    }

    /* On success, returned unique_ptr will contain metadata.  On failure,
       returned unique_ptr will be empty.  Timeout is specified in
       milliseconds.  A negative timeout value means "infinite timeout". */
    std::unique_ptr<TMetadata> Fetch(int timeout_ms = -1);

    enum class TTopicAutocreateResult {
      /* Topic was successfully created. */
      Success,

      /* Topic creation failed.  Give up. */
      Fail,
      
      /* Topic creation failed due to communication error.  Try again with
         different broker. */
      TryOtherBroker
    };  // TTopicAutocreateResult

    /* Attempt to create a new Kafka topic.  For this to work, the brokers must
       be configured with auto.create.topics.enable=true.  To request creation
       of a new topic, we send a single topic metadata request for the topic we
       wish to create. */
    TTopicAutocreateResult TopicAutocreate(const char *topic, int timeout_ms);

    private:
    bool SendRequest(const std::vector<uint8_t> &request, int timeout_ms);

    bool ReadResponse(int timeout_ms);

    const std::unique_ptr<const KafkaProto::Metadata::TMetadataProtocol>
        MetadataProtocol;

    /* This is the all topics metadata request that we send to a broker.  It is
       always the same sequence of bytes (since we always use a correlation ID
       of 0), so we may as well make it a constant and initialize it in the
       constructor. */
    const std::vector<uint8_t> MetadataRequest;

    Base::TFd Sock;

    using TStreamReaderType = Base::TStreamMsgWithSizeReader<int32_t>;

    /* This handles the details of reading produce responses from the socket.
     */
    TStreamReaderType StreamReader;
  };  // TMetadataFetcher

}  // Dory
