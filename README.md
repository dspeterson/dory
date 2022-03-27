# Dory

![Dory](doc/dory.jpg?raw=true)

Dory is a producer daemon for [Apache Kafka](http://kafka.apache.org).  Dory
simplifies clients that send messages to Kafka, freeing them from the
complexity of direct interaction with the Kafka cluster.  Specifically, it
handles the details of:

* Routing messages to the proper Kafka brokers, and spreading the load evenly
  across multiple partitions for a given topic.  Clients may optionally
  exercise control over partition assignment, such as ensuring that a group of
  related messages are all routed to the same partition, or even directly
  choosing a partition if the client knows the cluster topology.
* Waiting for acknowledgements, and resending messages as necessary due to
  communication failures or Kafka-reported errors
* Buffering messages to handle transient load spikes and Kafka-related problems
* Tracking message discards when serious problems occur.  Dory provides
  web-based discard reporting and status monitoring interfaces.
* Batching and compressing messages in a configurable manner for improved
  performance.  Snappy and gzip compression are currently supported.
* Optional rate limiting of messages on a per-topic basis.  This guards against
  buggy clients overwhelming the Kafka cluster with too many messages.

Dory runs on each individual host that sends messages to Kafka, receiving
messages from clients through local interprocess communication and forwarding
them to the Kafka cluster.  Once a client has written a message, no further
interaction with Dory is required.  From that point onward, Dory takes full
responsibility for reliable message delivery.  Due to the inherent reliability
of local interprocess communication, there is no need to wait for an
acknowledgement after sending a message to Dory.  Just send it and forget it.

The preferred method for sending messages to Dory is by UNIX domain datagram
socket.  Dory can also receive messages by UNIX domain stream socket or local
TCP.  The option of using stream sockets allows sending messages too large to
fit in a single datagram.  Local TCP facilitates sending messages from clients
written in programming languages that do not provide easy access to UNIX domain
sockets.  To simplify things, example client code for sending messages to Dory
is provided in a variety of popular programming languages.  Developers can
incorporate this into their own client implementations, saving them the effort
of writing their own code to serialize message data to an in-memory buffer and
write it to a socket.  Using Dory to send a message to Kafka can then be
reduced to making a simple API call.  The following client support for sending
messages to Dory is currently available:

* [C and C++](client/c_and_c%2B%2B)
* [Java](client/java/dory-client)
* Other JVM-based languages (Scala, Clojure, Groovy, etc.) via Dory's
  [Java support](client/java/dory-client)
* [Go (Golang)](client/go)
* [Python](client/python)
* [Ruby](client/ruby)
* [PHP](client/php)
* [Node.js](client/nodejs)
* [Perl](client/perl)
* [Shell scripting](client/shell_scripting)

Code contributions for clients in other programming languages are much
appreciated.  For those who are interested, low-level technical documentation
on how to send messages to Dory is provided [here](doc/sending_messages.md).
Support for [running Dory inside a Docker container](Docker) is also available.
Dory works with Kafka versions from 0.8 through the latest.  It runs on Linux,
and is supported on CentOS/RHEL, Ubuntu, and Debian.

Here are some reasons to consider using Dory:

* Dory decouples message sources from the Kafka cluster.  A client is not
  forced to wait for an ACK after sending a message, since Dory handles the
  details of waiting for ACKs from Kafka and resending messages when necessary.
  Likewise, a client is not burdened with holding onto messages until it has a
  reasonable-sized batch to send to Kafka.  If a client crashes immediately
  after sending a message to Dory, the message is safe with Dory.  However, if
  the client assumes responsibility for interacting with Kafka, a crash will
  cause the loss of all batched messages, and possibly sent messages for which
  an ACK is pending.

* Dory provides uniformity of mechanism for status monitoring and data quality
  reporting through its web interface.  Likewise, it provides a unified
  configuration mechanism for settings related to batching, compression, and
  other aspects of interaction with Kafka.  This simplifies system
  administration.

* Dory may enable more efficient interaction with the Kafka cluster.  Dory's
  C++ implementation is likely to be less resource-intensive than producers
  written in interpreted scripting languages.  Since Dory is capable of serving
  as a single access point for a variety of clients that send messages to
  Kafka, it permits more efficient batching by combining messages from multiple
  client programs into a single batch.  Batching behavior is coordinated across
  all message senders, rather than having each client act independently without
  awareness of messages from other clients.  If Dory assumes responsibility for
  all message transmission from a client host to a Kafka cluster with N
  brokers, only a single TCP connection to each broker is required, rather than
  having each client program maintain its own set of N connections.  The
  inefficiency of short-lived clients frequently opening and closing
  connections to the brokers is avoided.

* Dory simplifies adding producer support for new programming languages and
  runtime environments.  Sending a message to Kafka requires only writing a
  message in a simple binary format to a UNIX domain or local TCP socket.

Dory is the successor to [Bruce](https://github.com/ifwe/bruce), and is
maintained by [Dave Peterson](https://github.com/dspeterson), who created Bruce
while employed at [if(we)](http://www.ifwe.co/).  Code contributions and ideas
for new features and other improvements are welcomed and much appreciated.
Information for developers interested in contributing is provided
[here](doc/dev_info.md) and [here](CONTRIBUTING.md).

## Setting Up a Build Environment

To get Dory working, you need to set up a build environment.  Instructions are
available for [CentOS/RHEL 8](doc/centos_8_env.md),
[CentOS/RHEL 7](doc/centos_7_env.md),
[Ubuntu 20.04 LTS](doc/ubuntu_20_04_lts_env.md),
[Ubuntu 18.04 LTS](doc/ubuntu_18_04_lts_env.md),
[Ubuntu 16.04 LTS](doc/ubuntu_16_04_lts_env.md),
[Debian 10 (Buster)](doc/debian_10_env.md), and
[Debian 9 (Stretch)](doc/debian_9_env.md).

## Building and Installing Dory

Once your build environment is set up, the next step is to
[build and install](doc/build_install.md) Dory.

## Running Dory with Basic Configuration

Simple instructions for running Dory with a basic configuration can be found
[here](doc/basic_config.md).

## Sending Messages

Information on how to send messages to Dory can be found
[here](doc/sending_messages.md).

## Status Monitoring

Information on status monitoring can be found [here](doc/status_monitoring.md).

## Design Overview

Before going into more details on Dory's configuration options, it is helpful
to have an understanding of Dory's design, which is described
[here](doc/design.md).

## Detailed Configuration

Full details of Dory's configuration options are provided
[here](doc/detailed_config.md).

## Troubleshooting

Information that may help with troubleshooting is provided
[here](doc/troubleshooting.md).

## Developer Information

If you are interested in making custom modifications or contributing to Dory,
information is provided [here](doc/dev_info.md).

## Getting Help

If you have questions about Dory, contact Dave Peterson (dave@dspeterson.com).

-----

README.md:
Copyright 2019 Dave Peterson (dave@dspeterson.com)
Copyright 2014 if(we), Inc.

README.md is licensed under a Creative Commons Attribution-ShareAlike 4.0
International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
