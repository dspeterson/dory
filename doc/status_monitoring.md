## Status Monitoring

Dory provides a web-based management interface.  The default port number is
9090, but can be changed as described [here](detailed_config.md).  Assuming
that Dory is running on a system named `example`, your web browser will show
the following when directed at `http://example:9090`:

![Dory management interface](web_interface.jpg?raw=true)

As shown above, for each status option you can choose either plain or JSON
output.  JSON output is intended for comsumption by monitoring tools, allowing
them to parse the output using off-the-shelf JSON libraries.  Although human
beings can read this output, the plain option is more visually compact and
oriented toward human viewers.  For example Nagios monitoring scripts that
report problems indicated by the JSON counter and discard output, see
[status_monitoring/check_dory_counters.py](../status_monitoring/check_dory_counters.py)
and
[status_monitoring/check_dory_discards.py](../status_monitoring/check_dory_discards.py).
The discards script also contains example code for writing discard reports to
an Oracle database, providing a queryable history of data quality information.

### Counter Reporting

If you choose the plain option for *Get counter values*, you will get output
that looks something like this:

```
now=1408656417 Thu Aug 21 14:26:57 2014
since=1408585285 Wed Aug 20 18:41:25 2014
pid=14246
version=1.0.6.70.ga324763

[dory/web_interface.cc, 43].MongooseUrlDecodeError=0
[dory/web_interface.cc, 42].MongooseUnknownException=0
[dory/web_interface.cc, 41].MongooseStdException=0
[dory/web_interface.cc, 40].MongooseHttpRequest=1582
[dory/web_interface.cc, 39].MongooseGetMsgStatsRequest=0
[dory/web_interface.cc, 38].MongooseGetMetadataFetchTimeRequest=0
[dory/web_interface.cc, 37].MongooseGetDiscardsRequest=790
[dory/web_interface.cc, 36].MongooseGetCountersRequest=792
[dory/web_interface.cc, 35].MongooseEventLog=0
[dory/msg.cc, 25].MsgUnprocessedDestroy=0
[dory/msg.cc, 24].MsgDestroy=26934272
[dory/msg.cc, 23].MsgCreate=26934440
[dory/util/pause_button.cc, 14].PauseStarted=0
[dory/msg_dispatch/sender.cc, 53].SendProduceRequestOk=1015702
(remaining output omitted)
```

In addition to the counter values, the above output shows the time when the
counter report was created (1408656417 seconds since the epoch), the time when
Dory started running (1408585285 seconds since the epoch), Dory's process ID
which is 14246, and Dory's version which is `1.0.6.70.ga324763`.  The counter
values track various events inside Dory, and can be used for health monitoring
and troubleshooting.  Details on the meanings of some of the more interesting
counters are provided [here](troubleshooting.md).
Also, you can look in Dory's source code to see what a counter indicates.  For
instance, near the top of [src/dory/msg.cc](../src/dory/msg.cc) you will see
the following definitions:

```C++
DEFINE_COUNTER(MsgCreate);
DEFINE_COUNTER(MsgDestroy);
DEFINE_COUNTER(MsgUnprocessedDestroy);
```

These create the counters of the same names shown above.  Then you can look for
places in the code where the counters are incremented.  For example, counters
`MsgDestroy` and `MsgUnprocessedDestroy` are incremented inside the destructor
for class `TMsg`, which represents a single message:

```C++
TMsg::~TMsg() {
  assert(this);
  MsgDestroy.Increment();

  if (State != TState::Processed) {
    MsgUnprocessedDestroy.Increment();
    static TLogRateLimiter lim(std::chrono::seconds(5));

    if (lim.Test()) {
      syslog(LOG_ERR, "Possible bug: destroying unprocessed message with "
             "topic [%s] and timestamp %llu. This is expected behavior if "
             "the server is exiting due to a fatal error.", Topic.c_str(),
             static_cast<unsigned long long>(Timestamp));
      Server::BacktraceToLog();
    }
  }
}
```

### Discard Reporting

When certain problems occur, which are detailed [here](design.md), Dory will
discard messages.  When discards occur, they are tracked and reported through
Dory's discard reporting web interface.  If you choose the plain option for
*Get discard info* in Dory's web interface shown near the top of this page,
you will get output that looks something like this:

```
pid: 94870
version: 1.0.28.102.gb2d28f2
since: 1465103159 Sat Jun  4 22:05:59 2016
now: 1465104059 Sat Jun  4 22:20:59 2016
report interval in seconds: 600

current (unfinished) reporting period:
    report ID: 1
    start time: 1465103759 Sat Jun  4 22:15:59 2016
    malformed msg count: 0
    UNIX stream unclean disconnect count: 0
    TCP unclean disconnect count: 0
    unsupported API key msg count: 0
    unsupported version msg count: 0
    bad topic msg count: 0

latest finished reporting period:
    report ID: 0
    start time: 1465103159 Sat Jun  4 22:05:59 2016
    malformed msg count: 0
    UNIX stream unclean disconnect count: 0
    TCP unclean disconnect count: 0
    unsupported API key msg count: 0
    unsupported version msg count: 0
    bad topic msg count: 0
```

The above output shows the typical case where no discards are occurring.  A
case in which discards are occurring might look something like this:

```
pid: 52331
version: 1.0.28.113.gbc41f4d
since: 1465525168 Thu Jun  9 19:19:28 2016
now: 1465525347 Thu Jun  9 19:22:27 2016
report interval in seconds: 600

current (unfinished) reporting period:
    report ID: 0
    start time: 1465525168 Thu Jun  9 19:19:28 2016
    malformed msg count: 0
    UNIX stream unclean disconnect count: 0
    TCP unclean disconnect count: 0
    unsupported API key msg count: 0
    unsupported version msg count: 0
    bad topic msg count: 25

    recent bad topic: 11[bad_topic_2]
    recent bad topic: 11[bad_topic_1]

    rate limit discard topic: 7[topic_2] count 853

    discard topic: 7[topic_1] begin [1465525287266] end [1465525294911] count 51066
    discard topic: 7[topic_2] begin [1465525287266] end [1465525295832] count 118918
```

In the above example, we see that 51066 messages were discarded for valid topic
`topic_1` and 118918 messages were discarded for valid topic `topic_2`.  Of the
discards for `topic_1`, the earliest timestamp was 1465525287266 and the latest
was 1465525294911.  Likewise, the earliest and latest timestamps of discarded
messages for `topic_2` are 1465525287266 and 1465525295832.  For `topic_2`, 853
messages were discarded due to the rate limit for that topic.  These discards
are included in the 118918 total discards for `topic_2`.  Stated differently,
of the 118918 total discards for topic `topic_2`, 853 were due to Dory's rate
limiting mechanism and the rest were due to other reasons.  Dory provides an
optional per-topic message rate limiting mechanism, as documented
[here](design.md#message-rate-limiting).  Detailed configuration information
for this mechanism is given [here](detailed_config.md).

The timestamps in the discard reports are the client-provided ones documented
[here](sending_messages.md#message-formats), and are interpreted as
milliseconds since the epoch.  A total of 25 messages with invalid topics were
received, and recently received invalid topics are `bad_topic_2` and
`bad_topic_1`.  Prefixes of recently received malformed messages also appear in
Dory's discard reports in base64-encoded form.  Dory's process ID is 52331,
and the time when the discard report was created is 1465525347 (represented in
seconds, not milliseconds, since the epoch).  The version of Dory that
produced the report is `1.0.28.113.gbc41f4d`.  The default discard report
interval, as shown above, is 600 seconds, and is configurable, as documented
[here](detailed_config.md).

### Queued Message Information

If you choose the plain option for *Get queued message info* in Dory's web
interface shown near the top of this page, you will get output that looks
something like this:

```
pid: 4446
now: 1413927753 Tue Oct 21 14:42:33 2014
version: 1.0.8.33.gf45da3b

batch:       9120  send_wait:          0  ack_wait:      40379  topic: [topic2]
batch:          0  send_wait:       2752  ack_wait:       2750  topic: [topic1]

    125472 total new
      9120 total batch
      2752 total send_wait
     43129 total ack_wait
    180473 total (all states: new + batch + send_wait + ack_wait)
```

As with discard reports, you can see the process ID, current time, and Dory's
version at the top.  It also shows that for topic `topic2`, 9120 messages are
being batched, 0 messages are waiting to be sent to a Kafka broker, and 40379
messages are waiting for acknowledgements (ACKs) from Kafka.  Likewise, for
topic `topic1`, 0 messages are being batched, 2752 messages are waiting to be
sent to a Kafka broker, and 2750 messages are waiting for ACKs.  Additionally,
125472 messages are new, which means that they have not yet been batched or
routed.

### Metadata Fetch Time

If you choose the plain option for *Get metadata fetch time* in Dory's web
interface shown near the top of this page, you will get output that looks
something like this:

```
pid: 18592
version: 1.0.6.70.ga324763
now (milliseconds since epoch): 1408668040576 Thu Aug 21 17:40:40 2014
metadata last updated at (milliseconds since epoch): 1408667094030 Thu Aug 21 17:24:54 2014
metadata last modified at (milliseconds since epoch): 1408667094030 Thu Aug 21 17:24:54 2014
```

The *last updated at* value indicates the last time when the metadata was
updated, but not necessarily modified.  If Dory requests metadata, and finds
the new metadata to be identical to what it currently has, it treats this event
as a metadata update without modification.  The *last modified at* value
indicates the last time when Dory actually replaced its metadata due to
changed information.  If the metadata has never changed since Dory started
running, then the *last modified at* value indicates the time when Dory
initialized its metadata during startup.

### Metadata Updates

Dory refreshes its metadata at regular intervals.  The interval length
defaults to 15 minutes plus or minus some randomness, which is added so that
different Dory instances will tend to spread out their requests and not all
ask for new metadata at the same time.  Configuration of the interval length is
documented [here](detailed_config.md).  Additionally, you can manually cause
Dory to update its metadata.  Clicking on the *Update metadata* button in
Dory's web interface shown near the top of this page (i.e. sending an HTTP
POST to `http://example:9090/sys/metadata_update`) causes Dory to update its
metadata.  Certain error conditions can also cause Dory to update its
metadata, as described [here](design.md).

At this point it is helpful to have some information on
[Dory's design](design.md).

-----

status_monitoring.md: Copyright 2014 if(we), Inc.

status_monitoring.md is licensed under a Creative Commons
Attribution-ShareAlike 4.0 International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
