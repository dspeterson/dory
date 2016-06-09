## Sending Messages

### Using the Command Line Client

An easy way to get started sending messages to Dory is by using the command
line client.  This client is included in Dory's RPM package, and can be built
separately as described [here](build_install.md#building-dorys-client-library).
Sending a message to Dory can then be done as follows:

```
to_dory --socket_path /var/run/dory/dory.socket --topic test_topic \
        --value "hello world"
```

or alternatively:

```
echo -n "hello world" | to_dory --socket_path /var/run/dory/dory.socket \
        --topic test_topic --stdin
```

The above examples assume that you wish to send messages to Dory's UNIX domain
datagram socket, which is usually the preferred method of sending messages.
However, it is also possible to send messages by UNIX domain stream socket or
local TCP.  Stream sockets allow sending messages that are too large to fit in
a single datagram.  TCP makes Dory available to clients written in programming
languages that do not provide easy access to UNIX domain sockets.  To send
messages to Dory's UNIX domain stream socket, you would change the
`--socket_path /var/run/dory/dory.socket` in the above examples to something
like `--stream_socket_path /var/run/dory/dory.stream_socket`.  To send messages
to Dory over a local TCP connection, you would replace
`--socket_path /var/run/dory/dory.socket` with `--port N` where `N` is the port
Dory listens on.  A full listing of the client's command line options may be
obtained by typing `to_dory --help`.

To send messages to Dory using a given communication mechanism (UNIX domain
datagram sockets, UNIX domain stream sockets, or local TCP), Dory must be
started with a command line option enabling the mechanism and specifying the
UNIX domain socket path or port number, as described
[here](sending_messages.md#communicating-with-dory).

### Other Clients

Example client code for sending messages to Dory in various programming
languages may be found in the [example_clients](../example_clients) directory
of Dory's Git repository.  Community contributions for additional programming
languages are much appreciated.

### Message Types

Dory supports two input message types: *AnyPartition* messages
and *PartitionKey* messages.  They differ in how a partition is chosen for
topics with multiple partitions.

If you wish to give Dory full control over partition selection, then send an
AnyPartition message.  Dory will distrubute AnyPartition messages across the
partitions for a topic using essentially a round-robin distribution scheme to
balance the load.

The PartitionKey message type provides the sender with greater control over
which partition a message is sent to.  When sending this type of message, you
specify a 32-bit partition key.  To avoid confusion, note that the partition
key discussed here is conceptually unrelated to the optional message key
defined by the
[Kafka protocol](https://cwiki.apache.org/confluence/display/KAFKA/A+Guide+To+The+Kafka+Protocol)
and further described in
[this proposal](https://cwiki.apache.org/confluence/display/KAFKA/Keyed+Messages+Proposal).
The partition keys we describe here are known to Dory, but not to the Kafka
brokers.  Dory chooses a partition by applying a mod function to the partition
key and using it as an index into an array of partitions.  For instance,
suppose that topic T has partitions 0, 1, and 2.  Then Dory's partition array
for T will look like this:

```
[0, 1, 2]
```

For key K, Dory then chooses (K % 3) as the array index of the partition to
use.  For instance, if K is 6, then array index 0 will be chosen.  Since
partition ID 0 is at position 0, a message with key K will be sent to partition
0.  In practice, this might be useful in a scenario where messages are
associated with users of a web site, and all messages associated with a given
user must be sent to the same partition.  In this case, a hash of the user ID
can be used as a partition key.  The above-mentioned partition array is
guaranteed to be sorted in ascending order.  Therefore, a sender with full
knowledge of the partition layout for a given topic can use the partition key
mechanism to directly choose a partition.  Now suppose that partition 0 becomes
temporarily unavailable.  In the case where a message with key K maps to
partition 0, Dory will realize that partition 0 is unavailable and choose the
next available partition.  For instance, if partition 1 is available, then
messages that would normally be sent to partition 0 will temporarily be sent to
partition 1.  Once partition 0 becomes available again, Dory will resume
sending these messages to partition 0.

### Message Formats

Here, low-level details are presented for the message formats that Dory expects
to receive from its UNIX domain datagram socket.  The same notation described
[here](https://cwiki.apache.org/confluence/display/KAFKA/A+Guide+To+The+Kafka+Protocol)
is used below.  All multibyte integer fields are serialized in network byte
order (big endian).

#### Generic Message Format

```
GenericMessage => Size ApiKey ApiVersion Message

Size => int32
ApiKey => int16
ApiVersion => int16
Message => AnyPartitionMessage | PartitionKeyMessage
```

Field Descriptions:
* `Size`: This is the size in bytes of the entire message, including the `Size`
field.
* `ApiKey`: This identifies a particular message type.  Currently, the only
message types are AnyPartition and PartitionKey.  A value of 256 identifies an
AnyPartition message and a value of 257 identifies a PartitionKey message.
* `ApiVersion`: This identifies the version of a given message type.  The
current version is 0 for both AnyPartition and PartitionKey messages.
* `Message`: This is the data for the message format identified by `ApiKey` and
`ApiVersion`.

#### AnyPartition Message Format

```
AnyPartitionMessage => Flags TopicSize Topic Timestamp KeySize Key ValueSize
        Value

Flags => int16
TopicSize => int16
Topic => array of TopicSize bytes
Timestamp => int64
KeySize => int32
Key => array of KeySize bytes
ValueSize => int32
Value => array of ValueSize bytes
```

Field Descriptions:
* `Flags`: Currently this value must be 0.
* `TopicSize`: This is the size in bytes of the topic.  It must be > 0, since
the topic must be nonempty.
* `Topic`: This is the Kafka topic that the message will be sent to.
* `Timestamp`: This is a timestamp for the message, represented as milliseconds
since the epoch (January 1 1970 12:00am UTC).
* `KeySize`: This is the size in bytes of the key for the message, as described
[here](https://cwiki.apache.org/confluence/display/KAFKA/A+Guide+To+The+Kafka+Protocol#AGuideToTheKafkaProtocol-Messagesets).
For an empty key, 0 must be specified.
* `Key`: This is the message key defined by the
[Kafka protocol](https://cwiki.apache.org/confluence/display/KAFKA/A+Guide+To+The+Kafka+Protocol)
and further described in
[this proposal](https://cwiki.apache.org/confluence/display/KAFKA/Keyed+Messages+Proposal)
* `ValueSize`: This is the size in bytes of the value for the message, as
described
[here](https://cwiki.apache.org/confluence/display/KAFKA/A+Guide+To+The+Kafka+Protocol#AGuideToTheKafkaProtocol-Messagesets).
* `Value`: This is the message value.

#### PartitionKey Message Format

```
PartitionKeyMessage => Flags PartitionKey TopicSize Topic Timestamp KeySize Key
        ValueSize Value

Flags => int16
PartitionKey => int32
TopicSize => int16
Topic => array of TopicSize bytes
Timestamp => int64
KeySize => int32
Key => array of KeySize bytes
ValueSize => int32
Value => array of ValueSize bytes
```

Field Descriptions:
* `Flags`: Currently this value must be 0.
* `PartitionKey`: This is the partition key, as described above, which is used
for partition selection.
* `TopicSize`: This is the size in bytes of the topic.  It must be > 0, since
the topic must be nonempty.
* `Topic`: This is the Kafka topic that the message will be sent to.
* `Timestamp`: This is a timestamp for the message, represented as milliseconds
since the epoch (January 1 1970 12:00am UTC).
* `KeySize`: This is the size in bytes of the key for the message, as described
[here](https://cwiki.apache.org/confluence/display/KAFKA/A+Guide+To+The+Kafka+Protocol#AGuideToTheKafkaProtocol-Messagesets).
For an empty key, 0 must be specified.
* `Key`: This is the message key defined by the
[Kafka protocol](https://cwiki.apache.org/confluence/display/KAFKA/A+Guide+To+The+Kafka+Protocol)
and further described in
[this proposal](https://cwiki.apache.org/confluence/display/KAFKA/Keyed+Messages+Proposal)
* `ValueSize`: This is the size in bytes of the value for the message, as
described
[here](https://cwiki.apache.org/confluence/display/KAFKA/A+Guide+To+The+Kafka+Protocol#AGuideToTheKafkaProtocol-Messagesets).
* `Value`: This is the message value.

Notice that the PartitionKey format is identical to the AnyPartition format
except for the presence of the `PartitionKey` field.

### Communicating with Dory

Three options are available for sending messages to Dory:

* UNIX domain datagram sockets
* UNIX domain stream sockets
* local TCP sockets

To send messages using one of the above mechanisms, Dory must be started with a
command line option enabling the mechanism and specifying the UNIX domain
socket path or port number.  When starting Dory, you must specify at least one
of (`--receive_socket_name PATH`, `--receive_stream_socket_name PATH`,
`--input_port PORT`), as documented
[here](detailed_config.md#command-line-arguments).  Also see the
`--receive_socket_mode MODE` and `--receive_stream_socket_mode MODE` options.
When sending messages, be sure to specify the same UNIX domain socket path or
port number that Dory is using.

All three input options are described in depth
[here](design.md#options-for-clients), along with their intended purposes, and
relative advantages and disadvantages.

Once you are able to send messages to Dory, you will probably be interested
in learning about its
[status monitoring interface](../README.md#status-monitoring).

-----

sending_messages.md: Copyright 2014 if(we), Inc.

sending_messages.md is licensed under a Creative Commons Attribution-ShareAlike
4.0 International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
