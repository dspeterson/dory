## Design Overview

Dory's core implementation consists of three input agents for receiving
messages from clients by different interprocess communication mechanisms, a
router thread, and a message dispatcher that assigns a thread to each Kafka
broker.  There is also a main thread that starts the input agents and router
thread, and then waits for a shutdown signal.  The router thread starts and
manages the dispatcher, receives messages from the input agents, and routes
them to dispatcher threads for delivery to the Kafka brokers.  A third party
HTTP server library used for Dory's status monitoring interface creates
additional threads.

### Input Agents

There are three input agents: the UNIX datagram input agent, the UNIX stream
input agent, and the TCP input agent.  These receive messages from clients by
UNIX domain datagram socket, UNIX domain stream socket, and local TCP socket,
respectively.  For each option, the operating system provides the same reliable
delivery guarantee as for other local interprocess communication mechanisms
such as traditional UNIX pipes.  In particular, UNIX domain datagrams differ
from UDP datagrams, which do not guarantee reliable delivery.  Command line
options documented [here](detailed_config.md#command-line-arguments) determine
which of the above-mentioned communication mechanisms are available to clients.
If a given input option is not specified on the command line, the corresponding
input agent is not started.

#### Overview of Input Agent Behavior

The UNIX datagram input agent consists of a single thread that creates Dory's
UNIX datagram input socket, monitors it for messages from clients, and passes
the messages to the router thread.  The UNIX stream and TCP input agents are
nearly identical to each other in design, and share much common code.  Each of
these agents has a single thread that creates a listening socket and accepts
incoming connections.  On receipt of a connection, a worker thread is allocated
from a thread pool shared by the UNIX stream and TCP input agents, and assigned
to handle the connection.  Thus, there is one active worker thread for each
connection.  A future version of Dory may replace this implementation with one
that is more economical in its use of threads.  The thread pool places no upper
bound on the number of connections, and is intended purely to reduce the
overhead associated with creating and destroying threads.  Its size adjusts
dynamically based on demand, and decays gradually over time when a period of
high demand for workers is followed by reduced demand.  The pool has a manager
thread, which wakes up periodically to prune workers that have been idle for a
long time.  If Dory's command line options activate neither of the stream-based
input agents, the thread pool is not activated.

The input agents are designed to respond immediately to messages from clients,
and in the case of the stream input agents, accept incoming connections without
delay.  This is important to prevent clients from blocking when attempting to
write a message or open a connection to Dory.  Message handling behavior is
kept as simple as possible, with more complex and possibly time-consuming
behaviors delegated to the router thread and dispatcher threads.  When Dory
starts up, it preallocates a fixed but configurable amount of memory for
storing message content.  On receipt of a message, Dory allocates memory from
this pool to store the message contents.  If the pool does not contain enough
free memory, Dory will discard the message.  All messages discarded for any
reason are tracked and reported through Dory's web interface, as described
[here](status_monitoring.md#discard-reporting).  On successful message
creation, the receiving input agent queues the message for processing by the
router thread.  Once it has queued or discarded a message, the input agent
waits for the next incoming message.

#### Options for Clients

UNIX domain datagram sockets are the preferred option for most purposes due to
simplicity and low overhead.  However, the operating system imposes a limit on
datagram size.  This limit is platform-dependent, and has been observed to be
212959 bytes on a CentOS 7 x86_64 system, although Dory's
`--max_input_msg_size N` command line option may be used to enforce a lower
limit.  Starting Dory with the `--allow_large_unix_datagrams` option will
increase the limit (up to an observed value of 425951 bytes on a CentOS 7
x86_64 system), although clients will have to increase the SO_SNDBUF socket
option value to take advantage of the higher limit.  This may be difficult or
impossible for a client written in an interpreted scripting language.

To send larger messages, clients may use UNIX domain or local TCP stream
sockets.  Even when using stream sockets, the Kafka brokers place a limit on
message size which is determined by the `message.max.bytes` setting in the
[broker configuration](https://kafka.apache.org/08/configuration.html).  A
typical value is 1000000 bytes.  The value may be increased, although at some
point broker performance will be adversely affected.  Clients written in
programming languages that do not provide easy access to UNIX domain sockets
may send messages to Dory over a local TCP connection.  It is better to use
UNIX domain sockets if possible since this avoids the overhead imposed by the
operating system's TCP/IP implementation.

When sending messages by stream socket (UNIX domain or local TCP), a client can
send an unlimited number of messages over a single connection.  After
connecting to Dory, the client can simply start writing messages to the socket,
one after another, and close the connection when finished.  As with UNIX domain
datagrams, communication is purely one-way.  There is no need for Dory to send
an ACK to the client since the operating system guarantees reliable message
transmission.  Clients should never attempt to read from the socket, since Dory
will never write anything.  Dory's `--max_stream_input_msg_size MAX_BYTES`
option is intended to guard against a buggy client sending a ridiculously large
message over a stream socket.  It should be set to a value substantially larger
than the `message.max.bytes` setting in the Kafka broker configuration, and
large enough that a nonbuggy client is unlikely to attempt to send a message
that large.  If a client attempts to send a message that exceeds this limit (as
indicated by the `Size` field documented
[here](sending_messages.md#generic-message-format), Dory will immediately close
the connection and log an error.  Messages that do not exceed the limit, but
are still too large for Kafka to accept due to the `message.max.bytes` broker
setting, will be discarded (and reported through Dory's discard reporting web
interface).  However, in this case Dory will leave the connection open and
continue reading messages.

When the TCP input agent is active, Dory binds to the loopback interface.
Therefore only local clients can connect.  Allowing remote clients to connect
would be a mistake because sending messages to Dory is supposed to purely
one-way.  As long as clients are local, the operating system guarantees
reliable communication so there is no need for Dory to send an ACK on receipt
of a message.  This simplifies clients, since they don't have to wait for an
ACK, and possibly resend messages to Dory to ensure reliable communication.  A
large part of Dory's purpose is to enable simple one-way "send it and forget
it" message transmission from clients.

### Router Thread

The router thread's primary responsibility is to route messages to Kafka
brokers based on metadata obtained from Kafka.  The metadata provides
information such as the set of brokers in the cluster, a list of known topics,
a list of partitions for each topic along with the locations of the replicas
for each partition, and status information for the brokers, topics, and
partitions.  Dory uses this information to choose a destination broker and
partition for each message.  If Dory receives a message for an unknown topic,
it will discard the message unless automatic topic creation is enabled.  In the
case where automatic topic creation is enabled, Dory first sends a single
topic metadata request to one of the brokers, which is the mechanism for
requesting creation of a new topic.  Assuming that a response indicating
success is received, Dory then does a complete refresh of its metadata, so
that the metadata shows information about the new topic, and then handles the
message as usual.  As documented [here](sending_messages.md#message-types),
Dory provides two different message types, *AnyPartition* messages and
*PartitionKey* messages, which implement different types of routing behavior.
The Router thread shares responsibility for message batching with the
dispatcher threads, as detailed below.

Once the router thread has chosen a broker for a message or batch of messages,
it queues the message(s) for receipt by the corresponding dispatcher thread.
The router thread monitors the dispatcher for conditions referred to as
*pause events*.  These occur due to socket-related errors and certain types of
error ACKs which indicate that the metadata is no longer accurate.  On
detection of a pause event, the router thread waits for the dispatcher threads
to shut down and extracts all messages from the dispatcher.  It then fetches
new metadata, starts new dispatcher threads, and reroutes the extracted
messages based on the new metadata.  The router thread also periodically
refreshes its metadata and responds to user-initiated metadata update requests.
In these cases, it fetches new metadata, which it compares with the existing
metadata.  If the new metadata differs, it shuts down the dispatcher threads
and then proceeds in a manner similar to the handling of a pause event.

### Dispatcher

The dispatcher opens a TCP connection to each Kafka broker that serves as
leader for at least one currently available partition.  As described above,
each connection is serviced by a dedicated thread.  A pause event initiated by
any dispatcher thread will alert the router thread and cause all of the
dispatcher threads to shut down.  As detailed below, responsibility for message
batching is divided between the dispatcher threads and the router thread,
according to the type of message being sent and how batching is configured.
Compression is handled completely by the dispatcher threads, which are also
responsible for assembling message batches into produce requests and doing
final partition selection as described in the section on batching below.

An error ACK in a produce response received from Kafka will cause the
dispatcher thread that got the ACK to respond in one of four ways:

1. *Resend*: Queue the corresponding message set to be immediately resent to
   the same broker.
2. *Discard*: Discard the corresponding message set and continue processing
   ACKs.
3. *Discard and Pause*: Discard the corresponding message set and initiate a
   pause event.
4. *Pause*: Initiate a pause event without discarding the corresponding message
   set.  In this case, the router thread will collect the messages and reroute
   them once it has updated the metadata and restarted the dispatcher.

The Kafka error ACK values are documented
[here](https://cwiki.apache.org/confluence/display/KAFKA/A+Guide+To+The+Kafka+Protocol#AGuideToTheKafkaProtocol-ErrorCodes).
The following table summarizes which response Dory implements for each type of
error ACK:

| Error                            | Code | Response from Dory |
|:---------------------------------|-----:|:-------------------|
| Unknown                          |   -1 | Discard            |
| OffsetOutOfRange                 |    1 | Discard            |
| InvalidMessage                   |    2 | Resend             |
| UnknownTopicOrPartition          |    3 | Pause              |
| InvalidMessageSize               |    4 | Discard            |
| LeaderNotAvailable               |    5 | Pause              |
| NotLeaderForPartition            |    6 | Pause              |
| RequestTimedOut                  |    7 | Pause              |
| BrokerNotAvailable               |    8 | Discard            |
| ReplicaNotAvailable              |    9 | Discard            |
| MessageSizeTooLarge              |   10 | Discard            |
| StaleControllerEpochCode         |   11 | Discard            |
| OffsetMetadataTooLargeCode       |   12 | Discard            |
| GroupLoadInProgressCode          |   14 | Discard            |
| GroupCoordinatorNotAvailableCode |   15 | Discard            |
| NotCoordinatorForGroupCode       |   16 | Discard            |
| InvalidTopicCode                 |   17 | Discard            |
| RecordListTooLargeCode           |   18 | Discard            |
| NotEnoughReplicasCode            |   19 | Discard            |
| NotEnoughReplicasAfterAppendCode |   20 | Discard            |
| InvalidRequiredAcksCode          |   21 | Discard            |
| IllegalGenerationCode            |   22 | Discard            |
| InconsistentGroupProtocolCode    |   23 | Discard            |
| InvalidGroupIdCode               |   24 | Discard            |
| UnknownMemberIdCode              |   25 | Discard            |
| InvalidSessionTimeoutCode        |   26 | Discard            |
| RebalanceInProgressCode          |   27 | Discard            |
| InvalidCommitOffsetSizeCode      |   28 | Discard            |
| TopicAuthorizationFailedCode     |   29 | Discard            |
| GroupAuthorizationFailedCode     |   30 | Discard            |
| ClusterAuthorizationFailedCode   |   31 | Discard            |
| (all other values)               |    ? | Discard            |

In the case of *UnknownTopicOrPartition*, the router thread will discard the
message set during rerouting if the topic no longer exists.  Feedback from the
Kafka community regarding these choices is welcomed.  If a different response
for a given error code would be more appropriate, changes can easily be made.

Additionally, socket-related errors cause a *Pause* response.  In other words,
a pause is initiated and any messages that could not be sent or did not receive
ACKs as a result will be reclaimed by the router thread and rerouted based on
updated metadata.  When a *Pause* or *Resend* response occurs specifically due
to an error ACK, a failed delivery attempt count is incremented for each
message in the corresponding message set.  Once a message's failed delivery
attempt count exceeds a configurable threshold, the message is discarded.

### Message Batching

Batching is configurable on a per-topic basis.  Specifically, topics may be
configured with individual batching thresholds that consist of any combination
of the following limits:

- Maximum batching delay, specified in milliseconds.  This threshold is
triggered when the age of the oldest message
[timestamp](sending_messages.md#message-formats) in the batch is at least the
specified value.
- Maximum combined message data size, specified in bytes.  This includes only
the sizes of the [keys and values](sending_messages.md#message-formats).  The
size of an empty message is counted as 1 byte.  This prevents batching an
unbounded number of empty messages in the case where no limit is placed on
batching delay or message count.
- Maximum message count.

Batching may also be configured so that topics without individual batching
configurations are grouped together into combined (mixed) topic batches subject
to configurable limits of the same types that govern per-topic batching.  It is
also possible to specify that batching for certain topics is completely
disabled, although this is not recommended due to performance considerations.

When a batch is completed, it is queued for transmission by the dispatcher
thread connected to the destination broker.  While a dispatcher thread is busy
sending a produce request, multiple batches may arrive in its queue.  When it
finishes sending, it will then try to combine all queued batches into the next
produce request, up to a configurable data size limit.

#### Batching of AnyPartition Messages

For AnyPartition messages, per-topic batching is done by the router thread.
When a batch for a topic is complete, Dory chooses a destination broker as
follows.  For each topic, Dory maintains an array of available partitions.  An
index in this array is chosen in a round-robin manner, and the broker that
hosts the lead replica for that partition is chosen as the destination.
However, at this point only the destination broker is determined, and the batch
may ultimately be sent to a different partition hosted by the same broker.
Final partition selection is done by the dispatcher thread for the destination
broker as a produce request is being built.  In this manner, batches for a
given topic are sent to brokers proportionally according to how many partitions
for the topic each broker hosts.  For instance, suppose topic T has a total of
10 partitions.  If 3 of the partitions are hosted by broker B1 and 7 are hosted
by broker B2, then 30% of the batches for topic T will be sent to B1 and 70%
will be sent to B2.

Combined topics batching (in which a single batch contains multiple topics) may
be configured for topics that do not have per-topic batching configurations.
For AnyPartition messages with these topics, a broker is chosen in the same
manner as described above for per-topic batches.  However, this type of message
is batched at the broker level after the router thread has chosen a destination
broker and transferred the message to the dispatcher.

#### Batching of PartitionKey Messages

For PartitionKey messages, the chosen partition is determined by the partition
key which the client provides along with the message, as documented
[here](sending_messages.md#message-types).  Since the partition determines the
destination broker, per-topic batching of PartitionKey messages is done at the
broker level after the router thread has transferred a message to the
dispatcher.  All other aspects of batching for PartitionKey messages operate in
the same manner as for AnyPartition messages.  In the case of combined topics
batching, a single batch may contain a mixture of AnyPartition and PartitionKey
messages.

#### Produce Request Creation and Final Partition Selection

As mentioned above, the dispatcher thread for a broker may combine the contents
of multiple batches (possibly a mixture of per-topic batches of AnyPartition
messages, per-topic batches of PartitionKey messages, and combined topics
batches which may contain both types of messages) into a single produce
request.  To prevent creation of arbitrarily large produce requests, a
configurable upper bound on the combined size of all message content in a
produce request is implemented.  Enforcement of this limit may result in a
subset of the contents of a particular batch being included in a produce
request, with the remaining batch contents left for inclusion in the next
produce request.

Once a dispatcher thread has determined which messages to include in a produce
request, it must assign partitions to AnyPartition messages and group the
messages together by topic and partition.  First the messages are grouped by
topic, so that if a produce request contains messages from multiple batches,
all messages for a given topic are grouped together regardless of which batch
they came from.  Then partitions are assigned to AnyPartition messages, and
messages within a topic are grouped by partition into message sets.

For a given topic within a produce request, all AnyPartition messages are
assigned to the same partition.  To facilitate choosing a partition, the
dispatcher thread for a given broker has access to an array of available
partitions for the topic such that the lead replica for each partition is
located at the broker.  The dispatcher thread chooses a partition by cycling
through this vector in a round-robin manner.  For instance, suppose that topic
T has available partitions { P1, P2, P3, P4, P5 }.  Suppose that the lead
replica for each of { P1, P3, P4 } is hosted by broker B, while the lead
replica for each of the other partitions is hosted on some other broker.  Then
for a given produce request, the dispatcher thread for B will choose one of
{ P1, P3, P4 } as the assigned partition for all AnyPartition messages with
topic T.  Let's assume that partition P3 is chosen.  Then P4 will be chosen for
the next produce request containing AnyPartition messages for T, P1 will be
chosen next, and the dispatcher thread will continue cycling through the array
in a round-robin manner.  For a given topic within a produce request, once a
partition is chosen for all AnyPartition messages, the messages are grouped
into a single message set along with all PartitionKey messages that map to the
chosen partition.  PartitionKey messages for T that map to other partitions are
grouped into additional message sets.

### Message Compression

Kafka supports compression of individual message sets.  To send a compressed
message set, a producer such as Dory is expected to encapsulate the compressed
data inside a single message with a header field set to indicate the type of
compression.  Dory allows compression to be configured on a per-topic basis.
Although Dory was designed to support multiple compression types, only Snappy
compression is currently implemented.  Support for new compression types can
easily be added with minimal changes to Dory's core implementation.

Dory may be configured to skip compression of message sets whose uncompressed
sizes are below a certain limit, since compression of small amounts of data may
not be worthwhile.  Dory may also be configured to compute the ratio of
compressed to uncompressed message set size, and send an individual message set
uncompressed if this ratio is above a certain limit.  This prevents the brokers
from wasting CPU cycles dealing with message sets that compress poorly.  The
intended use case for this behavior is situations where most messages compress
well enough for compression to be worthwhile, but there are occasional message
sets that compress poorly.  It is best to disable compression for topics that
consistently compress poorly, so Dory avoids wasting CPU cycles on useless
compression attempts.  Future work is to add compression statistics reporting
to Dory's web interface, so topics that compress poorly can easily be
identified.  An additional possibility is to make Dory smart enough to learn
through experience which topics compress well, although this may be more effort
than it is worth.

Kafka places an upper bound on the size of a single message.  To prevent this
limit from being exceeded by a message that encapsulates a large compressed
message set, Dory limits the size of a produce request so that the
uncompressed size of each individual message set it contains does not exceed
the limit.  Although it's possible that the compressed size of a message set is
within the limit while the uncompressed size exceeds it, basing enforcement of
the limit on uncompressed size is simple and avoids wasting CPU cycles on
message sets that are found to still exceed the limit after compression.

### Message Rate Limiting

Dory provides optional message rate limiting on a per-topic basis.  The
motivation is to deal with situations in which buggy client code sends an
unreasonably large volume of messages for some topic T. Without rate limiting,
this might stress the Kafka cluster to the point where it can no longer keep up
with the message volume. The result is likely to be slowness in message
processing that affects many topics, causing Dory to discard messages across
many topics.  The goal of rate limiting is to contain the damage by discarding
excess messages for topic T, preventing the Kafka cluster from becoming
overwhelmed and forcing Dory to discard messages for other topics.  To specify
a rate limiting configuration, you provide an interval length in milliseconds
and a maximum number of messages for a given topic that should be allowed
within an interval of that length.  Dory implements rate limiting by assigning
its own internal timestamps to messages as they are created using a clock that
increases monotonically, and is guaranteed to be unaffected by changes made to
the system wall clock.  Therefore there is no danger of messages being
erroneously discarded by the rate limiting mechanism if the system clock is set
back.  As with all other types of discards, messages discarded by the rate
limiting mechanism will be included in Dory's discard reports.

Next: [detailed configuration](detailed_config.md).

-----

design.md: Copyright 2014 if(we), Inc.

design.md is licensed under a Creative Commons Attribution-ShareAlike 4.0
International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
