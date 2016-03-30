# Dory

![Dory](doc/dory.jpg?raw=true)

Dory is a producer daemon for [Apache Kafka](http://kafka.apache.org).  Dory
simplifies clients that send messages to Kafka, freeing them from the
complexity of direct interaction with the Kafka cluster.  Specifically, it
handles the details of:

* Routing messages to the proper brokers, and spreading the load evenly across
  multiple partitions for a given topic.  Clients may optionally exercise
  control over partition assignment, such as ensuring that a group of related
  messages are all routed to the same partition, or even directly choosing a
  partition if the client knows the cluster topology.
* Waiting for acknowledgements, and resending messages as necessary due to
  communication failures or Kafka-reported errors
* Buffering messages to handle transient load spikes and Kafka-related problems
* Tracking message discards when serious problems occur; Providing web-based
  discard reporting and status monitoring interfaces
* Batching and compressing messages in a configurable manner for improved
  performance
* Optional rate limiting of messages on a per-topic basis.  This guards against
  buggy client code overwhelming the Kafka cluster with too many messages.

Dory runs on each individual host that communicates with Kafka, receiving
messages from local clients over a UNIX domain datagram socket.  Clients write
messages to Dory's socket in a simple binary format.  Once a client has
written a message, no further interaction with Dory is required.  From that
point onward, Dory takes full responsibility for reliable message delivery.
Dory serves as a single intake point for a Kafka cluster, receiving messages
from diverse clients regardless of what programming language a client is
written in.  The following client support for sending messages to Dory is
currently available:

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
Kafka, and has been tested on versions 0.8 and 0.9.  It runs on Linux, and has
been tested on CentOS versions 7 and 6.5, and Ubuntu versions 15.04 LTS,
14.04.1 LTS, and 13.10.

Dory is a fork of [Bruce](https://github.com/ifwe/bruce), and is maintained by
Dave Peterson, who created Bruce while employed at if(we).  Code contributions
from the community are welcomed and much appreciated.  Information for
developers interested in contributing is provided [here](doc/dev_info.md) and
[here](CONTRIBUTING.md).

## Setting Up a Build Environment

To get Dory working, you need to set up a build environment.  Currently,
instructions are available for [CentOS 7](doc/centos_7_env.md),
[CentOS 6.5](doc/centos_6_5_env.md),
[Ubuntu (15.04 LTS, 14.04.1 LTS, and 13.10)](doc/ubuntu_13-15_env.md), and
[Debian 8](doc/debian_8_env.md).

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
