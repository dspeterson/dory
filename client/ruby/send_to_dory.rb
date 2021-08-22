#!/usr/bin/env ruby

# -----------------------------------------------------------------------------
# Copyright 2015 Dave Peterson <dave@dspeterson.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# -----------------------------------------------------------------------------
#
# This is an example Ruby script that sends messages to Dory.

require 'socket'

class DoryMsgCreator
  # exception base class
  class Error < StandardError
  end

  # excption thrown when Kafka topic is too large
  class DoryTopicTooLarge < Error
    def message
      'Kafka topic is too large'
    end
  end

  # exception thrown when message would exceed max possible size
  class DoryMsgTooLarge < Error
    def message
      'Cannot create a message that large'
    end
  end

  MSG_SIZE_FIELD_SIZE = 4
  API_KEY_FIELD_SIZE = 2
  API_VERSION_FIELD_SIZE = 2
  FLAGS_FIELD_SIZE = 2
  PARTITION_KEY_FIELD_SIZE = 4
  TOPIC_SIZE_FIELD_SIZE = 2
  TIMESTAMP_FIELD_SIZE = 8
  KEY_SIZE_FIELD_SIZE = 4
  VALUE_SIZE_FIELD_SIZE = 4

  ANY_PARTITION_FIXED_BYTES = MSG_SIZE_FIELD_SIZE + API_KEY_FIELD_SIZE +
      API_VERSION_FIELD_SIZE + FLAGS_FIELD_SIZE + TOPIC_SIZE_FIELD_SIZE +
      TIMESTAMP_FIELD_SIZE + KEY_SIZE_FIELD_SIZE + VALUE_SIZE_FIELD_SIZE

  PARTITION_KEY_FIXED_BYTES = ANY_PARTITION_FIXED_BYTES +
      PARTITION_KEY_FIELD_SIZE

  ANY_PARTITION_API_KEY = 256
  ANY_PARTITION_API_VERSION = 0

  PARTITION_KEY_API_KEY = 257
  PARTITION_KEY_API_VERSION = 0

  # This is the maximum topic size allowed by Kafka.
  MAX_TOPIC_SIZE = (2 ** 15) - 1

  # This is an extremely loose upper bound, based on the maximum value that can
  # be stored in a 32-bit signed integer field.  The actual maximum is much
  # smaller.  If we are sending to Dory by UNIX domain datagram socket, we are
  # limited by the maximum UNIX domain datagram size supported by the operating
  # system, which has been observed to be 212959 bytes on a CentOS 7 x86_64
  # system.  If we are sending to Dory by UNIX domain stream socket or local
  # TCP, there is a configurable maximum imposed by the Kafka brokers.  See the
  # message.max.bytes setting in the Kafka broker configuration.
  MAX_MSG_SIZE = (2 ** 31) - 1

  def create_any_partition_msg(topic, timestamp, key, value)
    if topic.size > MAX_TOPIC_SIZE
      raise DoryTopicTooLarge.new
    end

    msg_size = ANY_PARTITION_FIXED_BYTES + topic.size + key.size + value.size

    if msg_size > MAX_MSG_SIZE
      raise DoryMsgTooLarge.new
    end

    timestamp_hi = timestamp >> 32
    timestamp_lo = timestamp & 0xffffffff
    result =
        [msg_size,
         ANY_PARTITION_API_KEY,
         ANY_PARTITION_API_VERSION,
         0,
         topic.size
        ].pack('Nnnnn')
    result += topic
    result +=
        [timestamp_hi,
         timestamp_lo,
         key.size
        ].pack('NNN')
    result += key
    result += [value.size].pack('N')
    result += value
    result
  end

  def create_partition_key_msg(partition_key, topic, timestamp, key, value)
    if topic.size > MAX_TOPIC_SIZE
      raise DoryTopicTooLarge.new
    end

    msg_size = PARTITION_KEY_FIXED_BYTES + topic.size + key.size + value.size

    if msg_size > MAX_MSG_SIZE
      raise DoryMsgTooLarge.new
    end

    timestamp_hi = timestamp >> 32
    timestamp_lo = timestamp & 0xffffffff
    result =
        [msg_size,
         PARTITION_KEY_API_KEY,
         PARTITION_KEY_API_VERSION,
         0,
         partition_key,
         topic.size
        ].pack('NnnnNn')
    result += topic
    result +=
        [timestamp_hi,
         timestamp_lo,
         key.size
        ].pack('NNN')
    result += key
    result += [value.size].pack('N')
    result += value
    result
  end
end

def get_epoch_milliseconds
  (Time.now.to_f * 1000).round
end

def tmp_filename(base)
  gen = Random.new
  n = gen.rand(2 ** 64)  # generate a random 64-bit unsigned integer
  '/tmp/' + base + '.' + n.to_s
end

def delete_file_if_exists(path)
  begin
    File.delete(path)
  rescue Errno::ENOENT
    # No such file: ignore error
  end
end

dory_dgram_path = '/path/to/dory/datagram_socket'
topic = 'some topic'  # Kafka topic
msg_key = ''
msg_value = 'hello world'
partition_key = 12345

mc = DoryMsgCreator.new

begin
  # Create AnyPartition message.
  any_partition_msg = mc.create_any_partition_msg(topic,
      get_epoch_milliseconds, msg_key, msg_value)

  # Create PartitionKey message.
  partition_key_msg = mc.create_partition_key_msg(partition_key, topic,
      get_epoch_milliseconds, msg_key, msg_value)
rescue DoryMsgCreator::DoryTopicTooLarge => x
  STDERR.puts x.message
  exit 1
rescue DoryMsgCreator::DoryMsgTooLarge => x
  STDERR.puts x.message
  exit 1
end

begin
  dory_sock = Socket.new(Socket::AF_UNIX, Socket::SOCK_DGRAM, 0)
  dory_sock_path = tmp_filename('dory_client')
  dory_sock.bind(Socket.pack_sockaddr_un(dory_sock_path))
  dory_sock.connect(Socket.pack_sockaddr_un(dory_dgram_path))

  # Send AnyPartition message to Dory.
  dory_sock.send(any_partition_msg, 0)

  # Send PartitionKey message to Dory.
  dory_sock.send(partition_key_msg, 0)
rescue SystemCallError => x
  STDERR.puts "Error sending to Dory by UNIX domain datagram socket: #{x}"
  exit 1
ensure
  dory_sock.close
  delete_file_if_exists(dory_sock_path)
end

# Uncomment the code below to send messages to Dory by UNIX domain _stream_
# socket.

=begin

begin
  dory_sock_2 = Socket.new(Socket::AF_UNIX, Socket::SOCK_STREAM, 0)
  dory_sock_2.connect(Socket.pack_sockaddr_un('/path/to/dory/stream_socket'))

  # Send AnyPartition message to Dory.
  dory_sock_2.send(any_partition_msg, 0)

  # Send PartitionKey message to Dory.
  dory_sock_2.send(partition_key_msg, 0)
rescue SystemCallError => x
  STDERR.puts "Error sending to Dory by UNIX domain stream socket: #{x}"
  exit 1
ensure
  dory_sock_2.close
end

=end
