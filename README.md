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
* Tracking message discards when serious problems occur; Providing web-based
  discard reporting and status monitoring interfaces
* Batching and compressing messages in a configurable manner for improved
  performance
* Optional rate limiting of messages on a per-topic basis.  This guards against
  buggy client code overwhelming the Kafka cluster with too many messages.

Dory runs on each individual host that sends messages to Kafka, receiving
messages from clients through local interprocess communication and forwarding
them to the Kafka cluster.  Once a client has written a message, no further
interaction with Dory is required.  From that point onward, Dory takes full
responsibility for reliable message delivery.  The preferred method for sending
messages to Dory is by UNIX domain datagram socket.  However, Dory can also
receive messages by UNIX domain stream socket or local TCP.  The option of
using stream sockets allows sending messages too large to fit in a single
datagram.  Local TCP facilitates sending messages from clients written in
programming languages that do not provide easy access to UNIX domain sockets.
Dory serves as a single intake point, receiving messages from diverse clients
written in a variety of programming languages.  Here are some reasons to
consider using Dory:

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
  administration, as compared to a multitude of producer mechanisms for various
  programming languages and applications, each with its own status monitoring,
  data quality reporting, and configuration mechanisms or lack thereof.

* Dory may enable more efficient interaction with the Kafka cluster.  Dory's
  C++ implementation is likely to be less resource-intensive than producers
  written in interpreted scripting languages.  Since Dory is capable of serving
  as a single access point for all clients that send messages to Kafka, it
  permits more efficient batching by combining messages from multiple client
  programs into a single batch.  Batching behavior is coordinated across all
  message senders, rather than having each client act independently without
  awareness of messages from other clients.  If Dory assumes responsibility for
  all message transmission from a client host to a Kafka cluster with N
  brokers, only a single TCP connection to each broker is required, rather than
  having each client program maintain its own set of N connections.  Scenarios
  are avoided in which short-lived clients frequently open and close
  connections to the brokers.

* Dory simplifies adding producer support for new programming languages and
  runtime environments.  Sending a message to Kafka becomes as simple as
  writing a message in a simple binary format to a UNIX domain or local TCP
  socket.

The following client support for sending messages to Dory is currently
available:

* [C and C++](example_clients/c_and_c%2B%2B)
* [Java](example_clients/java/dory-client)
* Other JVM-based languages (Scala, Clojure, Groovy, etc.) via Dory's
  [Java support](example_clients/java/dory-client)
* [Python](example_clients/python)
* [Ruby](example_clients/ruby)
* [PHP](example_clients/php)
* [Node.js](example_clients/nodejs)
* [Perl](example_clients/perl)
* [Shell scripting](example_clients/shell_scripting)

Code contributions for clients in other programming languages are much
appreciated.  Technical details on how to send messages to Dory are provided
[here](doc/sending_messages.md).  Support for [running Dory inside a Docker
container](Docker) is also available.  Dory requires at least version 0.8 of
Kafka, and has been tested on versions 0.8, 0.9, and 0.10.  It runs on Linux,
and has been tested on CentOS versions 7 and 6.5, and Ubuntu versions 16.04
LTS, 15.04 LTS, 14.04.1 LTS, and 13.10.

Dory is the successor to [Bruce](https://github.com/ifwe/bruce), and is
maintained by [Dave Peterson](https://github.com/dspeterson), who created Bruce
while employed at [if(we)](http://www.ifwe.co/).  Code contributions from the
community are welcomed and much appreciated.  Information for developers
interested in contributing is provided [here](doc/dev_info.md) and
[here](CONTRIBUTING.md).

## Setting Up a Build Environment

To get Dory working, you need to set up a build environment.  Currently,
instructions are available for [CentOS 7](doc/centos_7_env.md),
[CentOS 6.5](doc/centos_6_5_env.md),
[Ubuntu (16.04 LTS, 15.04 LTS, 14.04.1 LTS, and 13.10)](doc/ubuntu_13-15_env.md),
and [Debian 8](doc/debian_8_env.md).

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

## Getting Help

If you have questions about Dory, contact Dave Peterson (dave@dspeterson.com).

-----

README.md: Copyright 2014 if(we), Inc.

README.md is licensed under a Creative Commons Attribution-ShareAlike 4.0
International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
