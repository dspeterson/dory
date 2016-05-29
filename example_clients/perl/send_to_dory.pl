#!/usr/bin/env perl

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
# This is an example Perl script that sends messages to Dory.

use IO::Socket;
use POSIX qw(tmpnam);
use Time::HiRes qw(gettimeofday);

package DoryMsgCreator {
  # result codes returned by methods
  our $OK = 0;
  our $TOPIC_TOO_LARGE = 1;
  our $MSG_TOO_LARGE = 2;

  my $MSG_SIZE_FIELD_SIZE = 4;
  my $API_KEY_FIELD_SIZE = 2;
  my $API_VERSION_FIELD_SIZE = 2;
  my $FLAGS_FIELD_SIZE = 2;
  my $PARTITION_KEY_FIELD_SIZE = 4;
  my $TOPIC_SIZE_FIELD_SIZE = 2;
  my $TIMESTAMP_FIELD_SIZE = 8;
  my $KEY_SIZE_FIELD_SIZE = 4;
  my $VALUE_SIZE_FIELD_SIZE = 4;

  my $ANY_PARTITION_FIXED_BYTES = $MSG_SIZE_FIELD_SIZE + $API_KEY_FIELD_SIZE +
      $API_VERSION_FIELD_SIZE + $FLAGS_FIELD_SIZE + $TOPIC_SIZE_FIELD_SIZE +
      $TIMESTAMP_FIELD_SIZE + $KEY_SIZE_FIELD_SIZE + $VALUE_SIZE_FIELD_SIZE;

  my $PARTITION_KEY_FIXED_BYTES = $ANY_PARTITION_FIXED_BYTES +
      $PARTITION_KEY_FIELD_SIZE;

  my $ANY_PARTITION_API_KEY = 256;
  my $ANY_PARTITION_API_VERSION = 0;

  my $PARTITION_KEY_API_KEY = 257;
  my $PARTITION_KEY_API_VERSION = 0;

  # constructor
  sub new {
    my $type = shift;
    my $self = {};
    return bless $self, $type;
  }

  # This is the maximum topic size allowed by Kafka.
  sub getMaxTopicSize {
    return (2 ** 15) - 1;
  }

  # This is an extremely loose upper bound, based on the maximum value that can
  # be stored in a 32-bit signed integer field.  The actual maximum is much
  # smaller.  If we are sending to Dory by UNIX domain datagram socket, we are
  # limited by the maximum UNIX domain datagram size supported by the operating
  # system, which has been observed to be 212959 bytes on a CentOS 7 x86_64
  # system.  If we are sending to Dory by UNIX domain stream socket or local
  # TCP, there is a configurable maximum imposed by the Kafka brokers.  See the
  # message.max.bytes setting in the Kafka broker configuration.
  sub getMaxMsgSize {
    return (2 ** 31) - 1;
  }

  sub createAnyPartitionMsg {
    my $self = shift;
    my $topic = shift;
    my $timestamp_hi = shift;
    my $timestamp_lo = shift;
    my $key = shift;
    my $value = shift;

    my $topic_size = length($topic);
    my $key_size = length($key);
    my $value_size = length($value);

    if ($topic_size > $self->getMaxTopicSize()) {
      return ($TOPIC_TOO_LARGE, "");
    }

    my $msg_size = $ANY_PARTITION_FIXED_BYTES + $topic_size + $key_size +
        $value_size;

    if ($msg_size > $self->getMaxMsgSize()) {
      return ($MSG_TOO_LARGE, "");
    }

    my $flags = 0;
    my $msg_size_hi = $msg_size >> 16;
    my $msg_size_lo = $msg_size & 0xffff;
    my $ts_3 = $timestamp_hi >> 16;
    my $ts_2 = $timestamp_hi & 0xffff;
    my $ts_1 = $timestamp_lo >> 16;
    my $ts_0 = $timestamp_lo & 0xffff;
    my $key_size_hi = $key_size >> 16;
    my $key_size_lo = $key_size & 0xffff;
    my $value_size_hi = $value_size >> 16;
    my $value_size_lo = $value_size & 0xffff;

    my $msg =
        pack("nnnnnn", $msg_size_hi, $msg_size_lo, $ANY_PARTITION_API_KEY,
            $ANY_PARTITION_API_VERSION, $flags, $topic_size) .
        $topic .
        pack("nnnnnn", $ts_3, $ts_2, $ts_1, $ts_0, $key_size_hi,
            $key_size_lo) .
        $key .
        pack("nn", $value_size_hi, $value_size_lo) .
        $value;

    return ($OK, $msg);
  }

  sub createPartitionKeyMsg {
    my $self = shift;
    my $partition_key = shift;
    my $topic = shift;
    my $timestamp_hi = shift;
    my $timestamp_lo = shift;
    my $key = shift;
    my $value = shift;

    my $topic_size = length($topic);
    my $key_size = length($key);
    my $value_size = length($value);

    if ($topic_size > $self->getMaxTopicSize()) {
      return ($TOPIC_TOO_LARGE, "");
    }

    my $msg_size = $PARTITION_KEY_FIXED_BYTES + $topic_size + $key_size +
        $value_size;

    if ($msg_size > $self->getMaxMsgSize()) {
      return ($MSG_TOO_LARGE, "");
    }

    my $flags = 0;
    my $msg_size_hi = $msg_size >> 16;
    my $msg_size_lo = $msg_size & 0xffff;
    my $partition_key_hi = $partition_key >> 16;
    my $partition_key_lo = $partition_key & 0xffff;
    my $ts_3 = $timestamp_hi >> 16;
    my $ts_2 = $timestamp_hi & 0xffff;
    my $ts_1 = $timestamp_lo >> 16;
    my $ts_0 = $timestamp_lo & 0xffff;
    my $key_size_hi = $key_size >> 16;
    my $key_size_lo = $key_size & 0xffff;
    my $value_size_hi = $value_size >> 16;
    my $value_size_lo = $value_size & 0xffff;

    my $msg =
        pack("nnnnnnnn", $msg_size_hi, $msg_size_lo, $PARTITION_KEY_API_KEY,
            $PARTITION_KEY_API_VERSION, $flags, $partition_key_hi,
            $partition_key_lo, $topic_size) .
        $topic .
        pack("nnnnnn", $ts_3, $ts_2, $ts_1, $ts_0, $key_size_hi,
            $key_size_lo) .
        $key .
        pack("nn", $value_size_hi, $value_size_lo) .
        $value;

    return ($OK, $msg);
  }
}

sub getEpochMilliseconds {
  use bigint;
  my ($sec, $micro) = gettimeofday;
  my $milli = (($sec * 1000000) + $micro) / 1000;
  my $hi = $milli >> 32;
  my $lo = $milli & 0xffffffff;
  return ($hi, $lo);
}

my $dory_dgram_path = "/path/to/dory/datagram_socket";
my $topic = "some topic";
my $msg_key = "";
my $msg_value = "hello world";
my $partition_key = 12345;

my $mc = DoryMsgCreator->new();

my ($timestamp_hi, $timestamp_lo) = getEpochMilliseconds();
my ($result, $any_partition_msg) = $mc->createAnyPartitionMsg($topic,
    $timestamp_hi, $timestamp_lo, $msg_key, $msg_value);

if ($result == $DoryMsgCreator::TOPIC_TOO_LARGE) {
  print STDERR "Kafka topic is too large";
  exit(1);
}

if ($result == $DoryMsgCreator::MSG_TOO_LARGE) {
  print STDERR "Cannot create a message that large";
  exit(1);
}

my ($timestamp_hi, $timestamp_lo) = getEpochMilliseconds();
my ($result, $partition_key_msg) = $mc->createPartitionKeyMsg($partition_key,
    $topic, $timestamp_hi, $timestamp_lo, $msg_key, $msg_value);

if ($result == $DoryMsgCreator::TOPIC_TOO_LARGE) {
  print STDERR "Kafka topic is too large";
  exit(1);
}

if ($result == $DoryMsgCreator::MSG_TOO_LARGE) {
  print STDERR "Cannot create a message that large";
  exit(1);
}

my $dory_sock_path = tmpnam();
my $sock = IO::Socket::UNIX->new(Type => SOCK_DGRAM,
    Local => $dory_sock_path);

if (!defined($sock)) {
  die "Failed to create socket: $!";
}

my $peer = sockaddr_un($dory_dgram_path);
my $ret = send($sock, $any_partition_msg, 0, $peer);

if (!defined($ret)) {
  unlink $dory_sock_path;
  die "Failed to send UNIX datagram: $!";
}

my $ret = send($sock, $partition_key_msg, 0, $peer);

if (!defined($ret)) {
  unlink $dory_sock_path;
  die "Failed to send UNIX datagram: $!";
}

unlink $dory_sock_path;

# Uncomment the code below to send messages to Dory by UNIX domain _stream_
# socket.

# my $sock_addr = sockaddr_un("/path/to/dory/stream_socket");
# socket(my $server, PF_UNIX, SOCK_STREAM, 0) || die "socket() failed: $!";
# connect($server, $sock_addr) || die "connect() failed: $!";
# print $server $any_partition_msg;
# print $server $partition_key_msg;
