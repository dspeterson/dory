## Troubleshooting

### Counters

As described [here](status_monitoring.md#counter-reporting), Dory's web
interface provides counter reports that can be used for troubleshooting.  Some
of Dory's more interesting counters are as follows:

* `MsgCreate`: This is incremented each time Dory creates a message.
* `MsgDestroy`: This is incremented each time Dory destroys a message.  The
value `MsgCreate - MsgDestroy` indicates the total number of messages Dory is
currently holding internally.  If the value gets too large, it likely indicates
that Kafka isn't keeping up with the volume of messages directed at it.
* `MsgUnprocessedDestroy`: This indicates destruction of a message before Dory
has marked it as processed.  If this occurs for any reason other than Dory
exiting on a fatal error, then there is a bug in Dory.  On occurrence of this
type of event, Dory will log a stack trace, which will help track down
problems.
* `AckOk`: This is incremented each time Dory receives a successful ACK from
Kafka.
* `AckErrorXxx`: These counters indicate various types of error ACKs received
from Kafka, as documented
[here](https://cwiki.apache.org/confluence/display/KAFKA/A+Guide+To+The+Kafka+Protocol#AGuideToTheKafkaProtocol-ErrorCodes).
* `XxxInputAgentDiscardXxx` and `DiscardXxx`: These counters are incremented
when Dory discards messages for various reasons.
* `NewUnixClient`: This is incremented each time Dory accepts an incoming
connection from a client wishing to send messages over a UNIX domain stream
socket.
* `NewTcpClient`: This is incremented each time Dory accepts an incoming
connection from a client wishing to send messages over a local TCP connection.
* `UnixStreamInputCleanDisconnect`: This is incremented each time a UNIX domain
stream client disconnects and there is no partially sent message remaining for
Dory to read.  This is the expected case when a client disconnects.
* `TcpInputCleanDisconnect`: This is incremented each time a local TCP client
disconnects and there is no partially sent message remaining for Dory to read.
This is the expected case when a client disconnects.
* `UnixStreamInputUncleanDisconnect`: This is incremented each time a UNIX
domain stream client disconnects and there is a partially sent message
remaining for Dory to read.  This indicates a problem, such as a buggy client
crashing while in the middle of sending a message.
* `TcpInputUncleanDisconnect`: This is incremented each time a local TCP client
disconnects and there is a partially sent message remaining for Dory to read.
This indicates a problem, such as a buggy client crashing while in the middle
of sending a message.
* `UnixDgInputAgentForwardMsg`: This is incremented each time the UNIX datagram
input agent receives a message from a client and queues it for processing by
the router thread.
* `UnixStreamInputForwardMsg`: This is incremented each time the UNIX stream
input agent receives a message from a client and queues it for processing by
the router thread.
* `TcpInputForwardMsg`: This is incremented each time the TCP input agent
receives a message from a client and queues it for processing by the router
thread.
* `BugXxx`: A nonzero value for any counter starting with the prefix `Bug`
indicates a bug in Dory.
* `MongooseXxx`: These counters indicate events related to Dory's web
interface.
* `NoDiscardQuery`: This is incremented when Dory stops getting asked for
discard reports at frequent enough intervals.  To avoid losing discard
information, it is recommended to query Dory's discard reporting interface at
intervals somewhat shorter that Dory's discard reporting interval.

It is also useful to look in
[status_monitoring/check_dory_counters.py](../status_monitoring/check_dory_counters.py)
to see which counters the script monitors and how it responds to incremented
values.

### Discard File Logging

For debugging and troubleshooting purposes, it may be helpful to configure
Dory to log discards to local files.  To do this, see the `<discardLogging>`
section of the config file.
As documented
[here](https://cwiki.apache.org/confluence/display/KAFKA/A+Guide+To+The+Kafka+Protocol#AGuideToTheKafkaProtocol-Messagesets),
a Kafka message consists of a key and a value, either of which may be empty.
Each line of the logfile contains information for a single discarded message.
The keys and values of the messages are written in base64 encoded form.

### Debug Logfiles

The `<msgDebug>` section of the config file configures Dory's debug logfile
mechanism, which maintains 3 separate logfiles: one for messages received from
clients, one for messages sent to Kafka, and one for messages that Dory
received a successful ACK for.  The `<path>` option specifies the directory in
which these files are placed.  To start and stop the logging mechanism, you
must send HTTP requests to Dory's web interface as follows:

* To start logging for a specific topic, send an HTTP GET to
`http://dory_host:9090/msg_debug/add_topic/name_of_topic`.
* To stop logging for a specific topic, send an HTTP GET to
`http://dory_host:9090/msg_debug/del_topic/name_of_topic`.
* To start logging for all topics, send an HTTP GET to
`http://dory_host:9090/msg_debug/add_all_topics`.
* To stop logging for all topics, send an HTTP GET to
`http://dory_host:9090/msg_debug/del_all_topics`.
* To see which topics are currently being logged, send an HTTP GET to
`http://dory_host:9090/msg_debug/get_topics`.
* To stop logging and make the debug logfiles empty, send an HTTP GET to
`http://dory_host:9090/msg_debug/truncate_files`.

Once debug logging is started, it will automatically stop when either the
time limit specified by `<timeLimit>` expires or the debug logfile size limit
specified by `<byteLimit>` is reached.  As with discard logfiles, keys and
values are written in base64 encoded form.

### Other Tools

Dory's queue status interface described
[here](status_monitoring.md#queued-message-information)
provides per-topic information on messages being processed by Dory.  In cases
where Kafka isn't keeping up with the message volume, this may be helpful in
identifying specific topics that are placing heavy load on the system.  Useful
information can also be obtained from Dory's log messages.  As documented
[here](detailed_config.md), the `<level>` option in the `<logging>` section of
the config file specifies Dory's logging verbosity.

If you are interested in making custom modifications or contributing to Dory,
information is provided [here](dev_info.md).

-----

troubleshooting.md: Copyright 2014 if(we), Inc.

troubleshooting.md is licensed under a Creative Commons Attribution-ShareAlike
4.0 International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
