/* ----------------------------------------------------------------------------
   Copyright 2021 Dave Peterson <dave@dspeterson.com>

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
 */

// Package doryclient provides functions for serializing messages to send to
// Dory, a Kafka producer daemon.  See https://github.com/dspeterson/dory for
// information on Dory.
package doryclient

import (
    "encoding/binary"
    "errors"
)

const (
    // MaxTopicSize is the makimum topic size inbytes allowed by Kafks.
    MaxTopicSize uint = (1 << 15) - 1

    // MaxMsgSize is an extremely loose upper bound on the maximum message size
    // that can be sent to Kafka, based on the maximum value that can be stored
    // in a 32-bit signed integer field.  The actual maximum is much smaller.
    // If we are sending to Dory by UNIX domain datagram socket, we are limited
    // by the maximum UNIX domain datagram size supported by the operating
    // system, which has been observed to be 212959 bytes on a CentOS 7 x86_64
    // system.  If we are sending to Dory by UNIX domain stream socket or local
    // TCP, there is a configurable maximum imposed by the Kafka brokers.  See
    // the message.max.bytes setting in the Kafka broker configuration.
    MaxMsgSize uint = (1 << 31) - 1

    msgSizeFieldSize uint = 4
    apiKeyFieldSize uint = 2
    apiVersionFieldSize uint = 2
    flagsFieldSize uint = 2
    partitionKeyFieldSize uint = 4
    topicSizeFieldSize uint = 2
    timestampFieldSize uint = 8
    keySizeFieldSize uint = 4
    valueSizeFieldSize uint = 4

    anyPartitionFixedBytes = msgSizeFieldSize + apiKeyFieldSize +
        apiVersionFieldSize + flagsFieldSize + topicSizeFieldSize +
        timestampFieldSize + keySizeFieldSize + valueSizeFieldSize

    partitionKeyFixedBytes = anyPartitionFixedBytes + partitionKeyFieldSize

    anyPartitionApiKey uint16 = 256
    anyPartitionApiVersion uint16 = 0
    partitionKeyApiKey uint16 = 257
    partitionKeyApiVersion uint16 = 0
)

var (
    // TopicTooLarge is returned to indicate that a message can't be written
    // because the topic size in bytes exceeds MaxTopicSize.
    TopicTooLarge = errors.New("topic too large")

    // MsgTooLarge is returned to indicate that a message can't be written
    // because the message size would exceed MaxMsgSize.
    MsgTooLarge = errors.New("message too large")

    // MsgBufTooSmall is returned to indicate that a message can't be written
    // because the caller-supplied buffer is too small to contain the message.
    // Note that if the size is too small, the buffer will not be extended even
    // if the capacity is large enough to contain the message.  Buffer size
    // management is the caller's responsibility.  Depending on th desired
    // message type, ComputeAnyPartitionMsgSize or ComputePartitionKeyMsgSize
    // may be called to compute the required size.
    MsgBufTooSmall = errors.New("message buffer too small")
)

func putUint16(dst []byte, v uint16) []byte {
    binary.BigEndian.PutUint16(dst, v)
    return dst[2:]
}

func putUint32(dst []byte, v uint32) []byte {
    binary.BigEndian.PutUint32(dst, v)
    return dst[4:]
}

func putUint64(dst []byte, v uint64) []byte {
    binary.BigEndian.PutUint64(dst, v)
    return dst[8:]
}

// ComputeAnyPartitionMsgSize computes the size in bytes of an AnyPartition
// message to send to Dory for the given topicSize, keySize, and valueSize in
// bytes.
func ComputeAnyPartitionMsgSize(topicSize uint, keySize uint,
        valueSize uint) uint {
    return anyPartitionFixedBytes + topicSize + keySize + valueSize
}

// ComputePartitionKeyMsgSize computes the size in bytes of a PartitionKey
// message to send to Dory for the given topicSize, keySize, and valueSize in
// bytes.
func ComputePartitionKeyMsgSize(topicSize uint, keySize uint,
        valueSize uint) uint {
    return partitionKeyFixedBytes + topicSize + keySize + valueSize
}

func doWriteAnyPartitionMsg(buf []byte, msgSize uint32, topic []byte,
        timestamp uint64, key []byte, value []byte) {
    buf = putUint32(buf, msgSize)
    buf = putUint16(buf, anyPartitionApiKey)
    buf = putUint16(buf, anyPartitionApiVersion)
    buf = putUint16(buf, 0) // flags
    topicSize := uint16(len(topic))
    buf = putUint16(buf, topicSize)
    copy(buf, topic)
    buf = buf[topicSize:]
    buf = putUint64(buf, timestamp)
    keySize := uint32(len(key))
    buf = putUint32(buf, keySize)
    copy(buf, key)
    buf = buf[keySize:]
    buf = putUint32(buf, uint32(len(value)))
    copy(buf, value)
}

func doWritePartitionKeyMsg(buf []byte, msgSize uint32, partitionKey uint32,
        topic []byte, timestamp uint64, key []byte, value []byte) {
    buf = putUint32(buf, msgSize)
    buf = putUint16(buf, partitionKeyApiKey)
    buf = putUint16(buf, partitionKeyApiVersion)
    buf = putUint16(buf, 0) // flags
    buf = putUint32(buf, partitionKey)
    topicSize := uint16(len(topic))
    buf = putUint16(buf, topicSize)
    copy(buf, topic)
    buf = buf[topicSize:]
    buf = putUint64(buf, timestamp)
    keySize := uint32(len(key))
    buf = putUint32(buf, keySize)
    copy(buf, key)
    buf = buf[keySize:]
    buf = putUint32(buf, uint32(len(value)))
    copy(buf, value)
}

// WriteAnyPartitionMsg writes an AnyPartition message for sending to Dory,
// storing the result in dst, which must be preallocated to a size large enough
// to contain the message (use ComputeAnyPartitionMsgSize to compute the
// required size in bytes).  It returns nil on success, or one of the following
// errors:
//     - TopicTooLarge if the topic size in bytes is larger than MaxTopicSize.
//     - MsgTooLarge if the message size would exceed MaxMsgSize.
//     - MsgBufTooSmall if dst doesn't contain enough space for the message.
//       Note that if the size is too small, dst will not be extended even if
//       the capacity is large enough to contain the message.  Buffer size
//       management is the caller's responsibility.
func WriteAnyPartitionMsg(dst []byte, topic []byte, timestamp uint64,
        key []byte, value []byte) error {
    topicSize := uint(len(topic))

    if topicSize > MaxTopicSize {
        return TopicTooLarge
    }

    msgSize := ComputeAnyPartitionMsgSize(topicSize, uint(len(key)),
        uint(len(value)))

    if msgSize > MaxMsgSize {
        return MsgTooLarge
    }

    if uint(len(dst)) < msgSize {
        return MsgBufTooSmall
    }

    doWriteAnyPartitionMsg(dst, uint32(msgSize), topic, timestamp, key, value)
    return nil
}

// CreateAnyPartitionMsg creates an AnyPartition message for sending to Dory,
// and returns a newly allocated buffer containing the message on success.  The
// following errors may be returned:
//     - TopicTooLarge if the topic size in bytes is larger than MaxTopicSize.
//     - MsgTooLarge if the message size would exceed MaxMsgSize.
func CreateAnyPartitionMsg(topic []byte, timestamp uint64, key []byte,
        value []byte) ([]byte, error) {
    topicSize := uint(len(topic))

    if topicSize > MaxTopicSize {
        return nil, TopicTooLarge
    }

    msgSize := ComputeAnyPartitionMsgSize(topicSize, uint(len(key)),
        uint(len(value)))

    if msgSize > MaxMsgSize {
        return nil, MsgTooLarge
    }

    buf := make([]byte, msgSize)
    doWriteAnyPartitionMsg(buf, uint32(msgSize), topic, timestamp, key, value)
    return buf, nil
}

// WritePartitionKeyMsg writes a PartitionKey message for sending to Dory,
// storing the result in dst, which must be preallocated to a size large enough
// to contain the message (use ComputePartitionKeyMsgSize to compute the
// required size in bytes).  It returns nil on success, or one of the following
// errors:
//     - TopicTooLarge if the topic size in bytes is larger than MaxTopicSize.
//     - MsgTooLarge if the message size would exceed MaxMsgSize.
//     - MsgBufTooSmall if dst doesn't contain enough space for the message.
//       Note that if the size is too small, dst will not be extended even if
//       the capacity is large enough to contain the message.  Buffer size
//       management is the caller's responsibility.
func WritePartitionKeyMsg(dst []byte, partitionKey uint32, topic []byte,
        timestamp uint64, key []byte, value []byte) error {
    topicSize := uint(len(topic))

    if topicSize > MaxTopicSize {
        return TopicTooLarge
    }

    msgSize := ComputePartitionKeyMsgSize(topicSize, uint(len(key)),
        uint(len(value)))

    if msgSize > MaxMsgSize {
        return MsgTooLarge
    }

    if uint(len(dst)) < msgSize {
        return MsgBufTooSmall
    }

    doWritePartitionKeyMsg(dst, uint32(msgSize), partitionKey, topic,
        timestamp, key, value)
    return nil
}

// CreatePartitionKeyMsg creates a PartitionKey message for sending to Dory,
// and returns a newly allocated buffer containing the message on success.  The
// following errors may be returned:
//     - TopicTooLarge if the topic size in bytes is larger than MaxTopicSize.
//     - MsgTooLarge if the message size would exceed MaxMsgSize.
func CreatePartitionKeyMsg(partitionKey uint32, topic []byte, timestamp uint64,
        key []byte, value []byte) ([]byte, error) {
    topicSize := uint(len(topic))

    if topicSize > MaxTopicSize {
        return nil, TopicTooLarge
    }

    msgSize := ComputePartitionKeyMsgSize(topicSize, uint(len(key)),
        uint(len(value)))

    if msgSize > MaxMsgSize {
        return nil, MsgTooLarge
    }

    buf := make([]byte, msgSize)
    doWritePartitionKeyMsg(buf, uint32(msgSize), partitionKey, topic,
        timestamp, key, value)
    return buf, nil
}
