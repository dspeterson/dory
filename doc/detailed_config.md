## Detailed Configuration

Before reading the full details of Dory's configuration options, you will
probably want an overview of Dory's design, which is avaliable
[here](design.md).

### Config File

Dory's config file is an XML document that specifies settings for various
features such as batching, compression, and per-topic message rate limiting, as
well as a list of initial brokers to try contacting for metadata when Dory is
starting.  Below is an example config file.  It is well commented, and should
be self-explanatory once the reader is familiar with the information provided
in the above-mentioned design section.

```XML
<?xml version="1.0" encoding="US-ASCII"?>
<!-- example Dory configuration -->
<doryConfig>
    <batching>
        <namedConfigs>
            <config name="low_latency">
                <!-- Here we define thresholds that trigger completion of a
                     batch when exceeded.  The "value" attribute of any of the
                     3 elements below can be set to "disable".  However, at
                     least one of them must have its value set to something
                     other than "disable". -->

                <!-- Set 500 milliseconds max batching delay.  Here, you must
                     specify integer values directly rather than using syntax
                     such as "10k".  Specifying "disable" for this setting is
                     not recommended, since it can cause Dory to batch a
                     message indefinitely if not enough other messages arrive
                     to exceed one of the other two batching thresholds below.
                  -->
                <time value="500" />

                <!-- No limit on message count.  You can specify a value here
                     such as "10" or "10k".  A value of "10k" is interpreted as
                     (10 * 1024) messages. -->
                <messages value="disable" />

                <!-- Somewhat arbitrary upper bound on batch data size.  As
                     above, "256k" is interpreted as (256 * 1024) bytes.  A
                     simple integer value such as "10000" can also be
                     specified.  This value applies to the actual message
                     content (keys and values).  The total size of a batch will
                     be a bit larger due to header overhead.  The size of an
                     empty message is counted as 1 byte.  This prevents
                     batching an unbounded number of empty messages in the case
                     where no limit is placed on batching delay or message
                     count.
                  -->
                <bytes value="256k" />
            </config>

            <config name="default_latency">
                <time value="10000" />
                <messages value="disable" />
                <bytes value="256k" />
            </config>
        </namedConfigs>

        <!-- Somewhat arbitrary upper bound on produce request size.  As above,
             a value such as "100k" is interpreted as (100 * 1024) bytes.  You
             can also specify simple integer values such as "100000" directly.
             Here, you can not specify "disable".  A nonnegative integer value
             must be specified.  This value applies to the actual message
             content (keys and values).  The total size of a produce request
             will be a bit larger due to header overhead.
          -->
        <produceRequestDataLimit value="1024k" />

        <!-- This value should be exactly the same as the message.max.bytes
             value in the Kafka broker configuration.  A larger value will
             cause Kafka to send MessageSizeTooLarge error ACKs to Dory for
             large compressed message sets, which will cause Dory to discard
             them.  A smaller value will not cause data loss, but will
             unnecessarily restrict the size of a compressed message set.  As
             above, you can supply a value such as "1024k".  Specifying
             "disable" here is not permitted.
          -->
        <messageMaxBytes value="1000000" />

        <!-- This specifies the configuration for combined topics batching
             (where a single batch may contain multiple topics).  Setting
             "enable" to false is strongly discouraged due to performance
             considerations.
          -->
        <combinedTopics enable="true" config="default_latency" />

        <!-- This specifies how batching is handled for topics not specified in
             "topicConfigs" below.  Allowed values for the "action" attribute
             are as follows:

                 "perTopic": This setting will cause each topic not listed in
                     "topicConfigs" to be batched individually on a per-topic
                     basis, with the "config" attribute specifying a batching
                     configuration from "namedConfigs" above.

                 "combinedTopics": This setting will cause all topics not
                     listed in "topicConfigs" to be batched together in mixed
                     topic batches.  In this case, "config" must be either
                     missing or set to the empty string, and the batching
                     configuration is determined by the "combinedTopics"
                     element above.  This is the setting that most people will
                     want.

                 "disable": This setting will cause batching to be disabled for
                     all topics not listed in "topicConfigs".  In this case,
                     the "config" attribute is optional and ignored.  This
                     setting is strongly discouraged due to performance
                     considerations.
          -->
        <defaultTopic action="combinedTopics" config="" />

        <topicConfigs>
            <!-- Uncomment and customize the settings in here if you wish to
                 have batching configurations that differ on a per-topic basis.

                 As above, allowed settings for the "action" attribute are
                 "perTopic", "combinedTopics", and "disable".  The "disable"
                 setting is strongly discouraged due to performance
                 considerations.

            <topic name="low_latency_topic_1" action="perTopic"
                   config="low_latency" />
            <topic name="low_latency_topic_2" action="perTopic"
                   config="low_latency" />
              -->
        </topicConfigs>
    </batching>

    <compression>
        <namedConfigs>
            <!-- Don't bother to compress a message set whose total size is
                 less than minSize bytes.  As above, a value such as "1k" is
                 interpreted as (1 * 1024) bytes.  Here, a value of "disable"
                 is not recognized, but you can specify "0".  The value of 128
                 below is somewhat arbitrary, and not based on experimental
                 data.  Currently the only allowed values for "type" are
                 "snappy" and "none".
              -->
            <config name="snappy_config" type="snappy" minSize="128" />

            <!-- In the case of gzip compression, an optional compression level
                 may be specified.  The compression level can be a value
                 between 0 and 9, inclusive.  0 specifies no compression,
                 1 specifies the smallest nonzero amount of compression, and 9
                 specifies the maximum amount of compression.  Higher values
                 specify more compression at the cost of reduced speed.  A
                 value of -1 may also be specified, which maps to a default
                 compression level chosen by zlib (currently 6, according to a
                 comment in library header file <zlib.h>).  If the level is
                 omitted below, a value of -1 is used.
              -->
            <config name="gzip_config" type="gzip" minSize="128" level="3" />

            <!-- LZ4 compression is not yet officially supported, but should be
                 ready in the not too distant future.
            <config name="lz4_config" type="lz4" minSize="128" level="4" />
              -->

            <!-- "minSize" is ignored (and optional) if type is "none". -->
            <config name="no_compression" type="none" />
        </namedConfigs>

        <!-- This must be an integer value at least 0 and at most 100.  If the
             compressed size of a message set is greater than this percentage
             of the uncompressed size, then Dory sends the data uncompressed,
             so the Kafka brokers don't waste CPU cycles dealing with the
             compression.  The value below is somewhat arbitrary, and not based
             on experimental data.
          -->
        <sizeThresholdPercent value="75" />

        <!-- This specifies the compression configuration for all topics not
             listed in "topicConfigs" below.
          -->
        <defaultTopic config="snappy_config" />

        <topicConfigs>
            <!-- Uncomment and customize the settings in here if you wish to
                 configure compression on a per-topic basis.

            <topic name="no_compression_topic_1" config="no_compression" />
            <topic name="no_compression_topic_2" config="no_compression" />
            <topic name="example_1" config="gzip_config">
              -->
        </topicConfigs>
    </compression>

    <topicRateLimiting>
        <namedConfigs>
            <!-- This configuration specifies that all messages should be
                 discarded.
              -->
            <config name="zero" interval="1" maxCount="0" />

            <!-- This configuration specifies no rate limit (i.e. don't discard
                 any messages regardless of their arrival rate).
              -->
            <config name="infinity" interval="1" maxCount="unlimited" />

            <!-- This configuration specifies a limit of at most 1000 messages
                 every 10000 milliseconds.  Messages that would exceed this
                 limit are discarded.
              -->
            <config name="config1" interval="10000" maxCount="1000" />

            <!-- This configuration specifies a limit of at most (4 * 1024)
                 messages every 15000 milliseconds.  Messages that would exceed
                 this limit are discarded.
              -->
            <config name="config2" interval="15000" maxCount="4k" />
        </namedConfigs>

        <!-- This specifies a default configuration for topics not listed in
             <topicConfigs> below.  Each such topic is rate-limited
             individually.  In other words, with this configuration, topic
             "topic_a" would be allowed 1000 messages every 10000 milliseconds,
             and "topic_b" would also be allowed 1000 messages every 10000
             milliseconds.
          -->
        <defaultTopic config="infinity" />

        <topicConfigs>
            <!-- Rate limit configurations for individual topics go here. -->
            <!--
            <topic name="topic1" config="zero" />
            <topic name="topic2" config="infinity" />
            <topic name="topic3" config="config1" />
            <topic name="topic4" config="config2" />
              -->
        </topicConfigs>
    </topicRateLimiting>

    <inputSources>
        <!-- Config for UNIX domain datagram input. -->
        <unixDatagram enable="true">
            <!-- Socket file path must be absolute (i.e. it must start with
                 '/') or empty.  An empty path will disable UNIX datagram input
                 regardless of "enable" setting above.
              -->
            <path value="/var/run/dory/input_datagram_sock" />

            <!-- Mode bits for opening socket file.  Must be specified in octal
                 or binary.  For instance, value 0644 is octal as indicated by
                 the leading 0, and represents user read/write, group read, and
                 other read.  A binary value is prefixed by "0b" or "0B".  For
                 instance, the same permissions represented in binary are
                 0b110010010.  A value of "unspecified" lets the process umask
                 determine the mode bits.
              -->
            <mode value="0222" />
        </unixDatagram>

        <!-- Config for UNIX domain stream input. -->
        <unixStream enable="false">
            <!-- Socket file path must be absolute (i.e. it must start with
                 '/') or empty.  An empty path will disable UNIX stream input
                 regardless of "enable" setting above.
              -->
            <path value="/var/run/dory/input_stream_sock" />

            <!-- Mode bits for opening socket file.  Must be specified in octal
                 or binary.  For instance, value 0644 is octal as indicated by
                 the leading 0, and represents user read/write, group read, and
                 other read.  A binary value is prefixed by "0b" or "0B".  For
                 instance, the same permissions represented in binary are
                 0b110010010.  A value of "unspecified" lets the process umask
                 determine the mode bits.
              -->
            <mode value="0222" />
        </unixStream>

        <!-- Config for TCP input (loopback interface only). -->
        <tcp enable="false">
            <!-- Port to listen on.  An empty value will disable TCP input,
                 regardless of "enable" setting above.
              -->
            <port value="9000" />
        </tcp>
    </inputSources>

    <inputConfig>
        <!-- Maximum amount of memory in bytes to use for buffering messages.
             If this memory is exhausted, Dory starts discarding messages.
             Suffixes k or m may be used to specify a value, where k means
             "multiply by 1024" and m means "multiply by (1024 * 1024)".
          -->
        <maxBuffer value="128m" />

        <!-- Maximum input message size in bytes expected from clients sending
             UNIX domain datagrams.  This limit does NOT apply to messages sent
             by UNIX domain stream socket or local TCP (see maxStreamMsgSize).
             Input datagrams larger than this value will be discarded.
             Suffixes k or m may be used to specify a value, where k means
             "multiply by 1024" and m means "multiply by (1024 * 1024)".
          -->
        <maxDatagramMsgSize value="64k" />

        <!-- Allow large enough values for max_input_msg_size that a client
             sending a UNIX domain datagram of the maximum allowed size will
             need to increase its SO_SNDBUF socket option above the default
             value.
          -->
        <allowLargeUnixDatagrams value="false" />

        <!-- Maximum input message size in bytes expected from clients using
             UNIX domain stream sockets or local TCP.  Input messages larger
             than this value will cause Dory to immediately log an error and
             disconnect, forcing the client to reconnect if it wishes to
             continue sending messages.  This guards against ridiculously large
             messages.  Even if a message doesn't exceed this limit, it may
             still be discarded if it is too large to send in a single produce
             request.  However, in this case Dory will still leave the
             connection open and continue reading messages.  Suffixes k or m
             may be used to specify a value, where k means "multiply by 1024"
             and m means "multiply by (1024 * 1024)".
          -->
        <maxStreamMsgSize value="16m" />
    </inputConfig>

    <msgDelivery>
        <!-- Enable support for automatic topic creation.  The Kafka brokers
             must also be configured to support this.
          -->
        <topicAutocreate enable="false" />

        <!-- Maximum number of failed delivery attempts allowed before a
             message is discarded.
          -->
        <maxFailedDeliveryAttempts value="5" />

        <!-- Maximum delay in milliseconds to allow delivery of buffered
             messages once shutdown signal is received.
          -->
        <shutdownMaxDelay value="30000" />

        <!-- Max dispatcher shutdown delay in milliseconds when restarting
             dispatcher for metadata update.
          -->
        <dispatcherRestartMaxDelay value="5000" />

        <!-- Interval in minutes (plus or minus a bit of randomness) between
             periodic metadata updates
          -->
        <metadataRefreshInterval value="15" />

        <!-- On metadata refresh, compare new metadata to old metadata, and
             replace it only if it differs.  This should be enabled for normal
             operation, but disabling it may be useful for testing.
          -->
        <compareMetadataOnRefresh value="true" />

        <!-- Socket timeout in seconds to use when communicating with Kafka
             brokers.
          -->
        <kafkaSocketTimeout value="60" />

        <!-- Initial delay value in milliseconds between consecutive metadata
             fetches due to Kafka-related errors.  The actual value has some
             randomness added.
          -->
        <pauseRateLimitInitial value="5000" />

        <!-- Maximum number of times to double pauseRateLimitInitial on
             consecutive errors.
          -->
        <pauseRateLimitMaxDouble value="4" />

        <!-- Minimum delay in milliseconds before fetching new metadata from
             Kafka in response to an error.
          -->
        <minPauseDelay value="5000" />
    </msgDelivery>

    <httpInterface>
        <!-- Port that dory listens on for its HTTP interface. -->
        <port value="9090" />

        <!-- Make HTTP interface available only on loopback interface. -->
        <loopbackOnly value="false" />

        <!-- Discard reporting interval in seconds. -->
        <discardReportInterval value="600" />

        <!-- Maximum bad message prefix size in bytes to write to discard
             report.
          -->
        <badMsgPrefixSize value="256" />
    </httpInterface>

    <discardLogging enable="false">
        <!-- Absolute pathname of local file where discards will be logged.
             This is intended for debugging.  If unspecified, logging of
             discards to a file will be disabled.
          -->
        <path value="/var/log/dory/discard.log" />

        <!-- Maximum size in bytes of discard logfile.  When the next log entry
             e would exceed the maximum, logfile f is renamed to f.N wnere N is
             the current time in milliseconds since the epoch.  Then a new file
             f is opened, and e is written to f.  See also maxArchiveSize.
             Suffixes k or m may be used to specify a value, where k means
             "multiply by 1024" and m means "multiply by (1024 * 1024)".
          -->
        <maxFileSize value="32m" />

        <!-- See description of maxFileSize.  Once a discard logfile is renamed
             from f to f.N due to the size restriction imposed by maxFileSize,
             the directory containing f.N is scanned for all old discard
             logfiles.  If their combined size exceeds maxArchiveSize in bytes,
             then old logfiles are deleted, starting with the oldest, until
             their combined size no longer exceeds the maximum.  Suffixes k or
             m may be used to specify a value, where k means "multiply by 1024"
             and m means "multiply by (1024 * 1024)".
          -->
        <maxArchiveSize value="256m" />

        <!-- Maximum message prefix size in bytes to write to discard logfile
             when discarding.  Messages larger than this limit will be
             truncated  Suffix k may be used to specify a value, where k means
             "multiply by 1024".  A value of "unlimited" may be specified here.
          -->
        <maxMsgPrefixSize value="2k" />
    </discardLogging>

    <kafkaConfig>
        <!-- Client ID to send in produce requests. -->
        <clientId value="dory" />

        <!-- Replication timeout in milliseconds to send in produce requests.
          -->
        <replicationTimeout value="10000" />
    </kafkaConfig>

    <msgDebug enable="false">
        <!-- Absolute path to directory for debug instrumentation files. -->
        <path value="/var/log/dory/msg-debug" />

        <!-- Message debugging time limit in seconds. -->
        <timeLimit value="3600" />

        <!-- Message debugging byte limit.  Suffixes k or m may be used to
             specify a value, where k means "multiply by 1024" and m means
             "multiply by (1024 * 1024)".
          -->
        <byteLimit value="2048m" />
    </msgDebug>

    <!-- Logging congifuration.  If omitted, the default behavior is to enable
         only syslog output and set the level to NOTICE.
      -->
    <logging>
        <!-- Determines logging verbosity.  Valid values are "ERR", "WARNING",
             "NOTICE", "INFO", "DEBUG".  For instance, a value of "NOTICE"
             enables log messages of levels "ERR", "WARNING", and "NOTICE", and
             disables log messages of levels "INFO" and "DEBUG".
          -->
        <level value="NOTICE" />

        <!-- A true value for 'enable' enables logging to stdout/stderr.
             If enabled, warnings and errors go to stderr, and other messages
             go to stdout.  If stdoutStderr element is omitted, then logging to
             stdout/stderr is disabled.
          -->
        <stdoutStderr enable="false" />

        <!-- A true value for 'enable' enables syslog logging.  If syslog
             element is omitted, then syslog logging is enabled.
          -->
        <syslog enable="false" />

        <!-- A true value for 'enable' enables logging to a file.  If file
             element is omitted, then file logging is disabled.
          -->
        <file enable="true">
            <!-- Logfile path must be absolute (i.e. it must start with '/') or
                 empty.  An empty path will disable file logging regardless of
                 "enable" setting above.  If logging to a file is enabled,
                 SIGUSR1 causes dory to close and reopen its logfile.  This
                 facilitates logfile rotation.
              -->
            <path value="/var/log/dory/dory.log" />

            <!-- Mode bits for opening logfile.  Must be specified in octal or
                 binary.  For instance, value 0644 is octal as indicated by the
                 leading 0, and represents user read/write, group read, and
                 other read.  A binary value is prefixed by "0b" or "0B".  For
                 instance, the same permissions represented in binary are
                 0b110010010.  A value of "unspecified" lets the process umask
                 determine the mode bits.
              -->
            <mode value="0644" />
        </file>

        <!-- If true, log messages will be emitted when messages are discarded.
             These messages are rate limited to avoid writing too much output
             if a large number of discards occur.
          -->
        <logDiscards enable="true" />
    </logging>

    <initialBrokers>
        <!-- When Dory starts, it chooses a broker in this list to contact for
             metadata.  If Dory cannot get metadata from the host it chooses,
             it tries other hosts until it succeeds.  Once Dory successfully
             gets metadata, the broker list in the metadata determines which
             brokers Dory will connect to for message transmission and future
             metadata requests.  Specifying a single host is ok, but multiple
             hosts are recommended to guard against the case where a single
             specified host is down.
          -->
        <broker host="broker_host_1" port="9092" />
        <broker host="broker_host_2" port="9092" />
    </initialBrokers>
</doryConfig>
```

### Command Line Arguments

Dory's required command line arguments are summarized below:

* `--config-path PATH`: This specifies the location of the config file.

Dory's optional command line arguments are summarized below:

* `-h --help`: Display help message.
* `--version`: Display version information and exit.
* `--daemon`: Causes Dory to run as a daemon.

Now that you are familiar with all of Dory's configuration options, you may
find information on [troubleshooting](troubleshooting.md) helpful.

-----

detailed_config.md: Copyright 2014 if(we), Inc.

detailed_config.md is licensed under a Creative Commons Attribution-ShareAlike
4.0 International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
