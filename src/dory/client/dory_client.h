/* <dory/client/dory_client.h>

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

   Header file for Dory client library.  This is a pure C implementation, so
   both C and C++ clients can use the library.  Here is example C code for
   sending an AnyPartition message to Dory:

       const char topic[] = "some topic";  // Kafka topic
       const char key[] = "";  // message key
       const char value[] = "hello world";  // message value

       // should be current time in milliseconds since the epoch
       int64_t timestamp = 0;

       // Figure out how much memory is needed for datagram buffer.
       size_t topic_size = strlen(topic);
       size_t key_size = strlen(key);
       size_t value_size = strlen(value);
       size_t msg_size = 0;
       int ret = dory_find_any_partition_msg_size(topic_size, key_size,
           value_size, &msg_size);

       if (ret != DORY_OK) {
         // handle error
       }

       // Allocate datagram buffer.
       void *msg_buf = malloc(msg_size);

       if (msg_buf == NULL) {
         // handle error
       }

       // Write datagram into buffer.
       ret = dory_write_any_partition_msg(msg_buf, msg_size, topic, timestamp,
           key, key_size, value, value_size);

       // Here, the return value is guaranteed to be DORY_OK, since the above
       // call to dory_find_any_partition_msg_size() already validated the
       // topic and message sizes.
       assert(ret == DORY_OK);

       dory_client_socket_t sock;

       // Initialize 'sock' (only needs to be done once).
       dory_client_socket_init(&sock);

       // bind() socket to temporary filename and store server path in
       // sockaddr_un struct for use when sending.
       ret = dory_client_socket_bind(&sock, "/path/to/dory/socket");

       if (ret != DORY_OK) {
         // handle error
       }

       // Send message to Dory.
       ret = dory_client_socket_send(&sock, msg_buf, msg_size);

       if (ret != DORY_OK) {
         // handle error
       }

       // Clean up.
       free(msg_buf);
       dory_client_socket_close(&sock);

   The code for sending a PartitionKey message is identical except that you
   call dory_find_partition_key_msg_size() instead of
   dory_find_any_partition_msg_size(), and call
   dory_write_partition_key_msg() instead of dory_write_any_partition_msg().
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <sys/un.h>

#include <dory/client/status_codes.h>

/* A thin wrapper around a UNIX domain datagram socket for sending messages to
   Dory. */
typedef struct dory_client_socket {
  /* Client socket file descriptor.  Negative when not opened. */
  int sock_fd;

  /* Address info for Dory socket. */
  struct sockaddr_un server_addr;
} dory_client_socket_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Return build ID string for client library. */
const char * dory_get_build_id();

/* See <dory/client/status_codes.h> for definitions of status codes returned
   by library functions. */

/* Compute the total size of an AnyPartition message with topic size (as
   reported by strlen()) 'topic_size', key size 'key_size', and value size
   'value_size'.  All sizes are in bytes.  On success, write total message size
   to *out_size and return DORY_OK.  Possible returned error codes are
   { DORY_TOPIC_TOO_LARGE, DORY_MSG_TOO_LARGE }. */
int dory_find_any_partition_msg_size(size_t topic_size, size_t key_size,
    size_t value_size, size_t *out_size);

/* Compute the total size of a PartitionKey message with topic size (as
   reported by strlen()) 'topic_size', key size 'key_size', and value size
   'value_size'.  All sizes are in bytes.  On success, write total message size
   to *out_size and return DORY_OK.  Possible returned error codes are
   { DORY_TOPIC_TOO_LARGE, DORY_MSG_TOO_LARGE }. */
int dory_find_partition_key_msg_size(size_t topic_size, size_t key_size,
    size_t value_size, size_t *out_size);

/* Write an AnyPartition message to output buffer 'out_buf' whose size is
   'out_buf_size'.  'topic' specifies the topic string.  'timestamp' gives
   timestamp to assign to message in milliseconds since the epoch.  'key' and
   'key_size' specify message key.  'value' and 'value_size' specify message
   value.  All sizes are in bytes.  'out_buf_size' must be at least as large as
   the size reported by dory_find_any_partition_msg_size().  'key' can be null
   only if 'key_size' is 0.  'value' can be null only if 'value_size' is 0.
   Return DORY_OK on success.  Possible returned error codes are
   { DORY_BUF_TOO_SMALL, DORY_TOPIC_TOO_LARGE, DORY_MSG_TOO_LARGE }.  If
   'out_buf_size' was calculated by a successful call to
   dory_find_any_partition_msg_size() then this function is guaranteed to
   return DORY_OK. */
int dory_write_any_partition_msg(void *out_buf, size_t out_buf_size,
    const char *topic, int64_t timestamp, const void *key, size_t key_size,
    const void *value, size_t value_size);

/* Write a PartitionKey message to output buffer 'out_buf' whose size is
   'out_buf_size'.  'topic' specifies the topic string.  'timestamp' gives
   timestamp to assign to message in milliseconds since the epoch.
   'partition_key' gives the partition key for message routing.  'key' and
   'key_size' specify message key.  'value' and 'value_size' specify message
   value.  All sizes are in bytes.  'out_buf_size' must be at least as large as
   the size reported by dory_find_partition_key_msg_size().  'key' can be null
   only if 'key_size' is 0.  'value' can be null only if 'value_size' is 0.
   Return DORY_OK on success.  Possible returned error codes are
   { DORY_BUF_TOO_SMALL, DORY_TOPIC_TOO_LARGE, DORY_MSG_TOO_LARGE }.  If
   'out_buf_size' was calculated by a successful call to
   dory_find_partition_key_msg_size() then this function is guaranteed to
   return DORY_OK. */
int dory_write_partition_key_msg(void *out_buf, size_t out_buf_size,
    int32_t partition_key, const char *topic, int64_t timestamp,
    const void *key, size_t key_size, const void *value, size_t value_size);

/* Initialize a dory_client_socket_t structure.  This must be called before
   its first use, but should not be called again on the object after that.  It
   serves the same purpose as a constructor in C++.  On return, 'client_socket'
   is initialized to an empty state.  In other words, it does not yet contain a
   valid file descriptor. */
void dory_client_socket_init(dory_client_socket_t *client_socket);

/* Prepare 'client_socket' to send messages to Dory.  This amounts to doing a
   bind() operation on a temporary filename and storing 'server_path' in a
   sockaddr_un struct for later use.  Return DORY_OK on success.  On error,
   return one of two types of error codes:

       1.  If return value is negative, then it is an error code defined in
           <dory/client/status_codes.h>.  In this case, it will be one of
           { DORY_CLIENT_SOCK_IS_OPENED, DORY_SERVER_SOCK_PATH_TOO_LONG }.

       2.  If return value is > 0, then it is an errno value indicating the
           cause of failure.

   On successful return, you must call dory_client_socket_close() when done
   sending messages. */
int dory_client_socket_bind(dory_client_socket_t *client_socket,
    const char *server_path);

/* Send a message to Dory.  'client_socket' is a dory_client_socket_t for
   which dory_client_socket_bind() has successfully been called.  'msg' points
   to the message to send, and 'msg_size' gives the message size in bytes.
   'msg' must not be null, and 'msg_size' must be > 0.  Return DORY_OK on
   success.  On error, a value > 0 will be returned, which is interpreted as an
   errno value indicating what went wrong. */
int dory_client_socket_send(const dory_client_socket_t *client_socket,
    const void *msg, size_t msg_size);

/* Call this function when finished sending messages.  Calling this function
   again on an already closed dory_client_socket_t object is harmless. */
void dory_client_socket_close(dory_client_socket_t *client_socket);

#ifdef __cplusplus
}  // extern "C"
#endif
