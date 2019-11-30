## Basic Configuration

An example Dory configuration can be found in the [config](../config) directory
of Dory's Git repository, and instructions for deploying it can be found
[here](build_install.md#installing-dory).  Before using it, you need to edit
Dory's config file (`/etc/dory/dory_conf.xml` in the example configuration) as
follows.

* Look for the `<inputSources>` XML element.  Decide which input mechanisms you
want to configure (UNIX domain datagram sockets, UNIX domain stream sockets,
and/or local TCP).
* Look for the `<logging>` XML element.  Decide how you want to configure
Dory's logging.  You can choose any combination of (stdout/stderr, syslog,
logfile).  If you enable logging to a file, `SIGUSR1` will cause Dory to close
and reopen its logfile, which is useful for logfile rotation.
* Look for the `<initialBrokers>` XML element near the bottom of the file.  You
will find a list of Kafka brokers to try contacting for Dory's initial metadata
request.  This list needs to be edited to specify the brokers in your Kafka
cluster.  Specifying a single broker is ok, since Dory will learn about other
brokers from the metadata response it receives.  However, specifying multiple
brokers is preferable to guard against a situation where the specified broker
is down.

If you wish to start Dory using its init script after following the
steps given [here](build_install.md#installing-dory), that can be done as follows:

```
chkconfig dory on
service dory start
```
Otherwise, Dory can be started manually using the example configuration as
follows:

```
dory --daemon --config-path /etc/dory/dory_conf.xml
```

The above command line arguments have the following effects:
* `--daemon` tells Dory to run as a daemon.
* `--config-path /etc/dory/dory_conf.xml` specifies the location of Dory's
config file.

Dory's config file (`/etc/dory/dory_conf.xml` in the above example) is an
XML document that specifies various settings including batching, compression,
and message rate limiting options, as well as the above-described list of
initial brokers.  The settings in the
[example config file](../config/dory_conf.xml) related to batching and
compression are somewhat arbitrary, and may require tuning.

Full details of Dory's configuration options are provided
[here](detailed_config.md).

You can shut down Dory using its init script as follows:

```
service dory stop
```

Alternatively, you can shut down Dory directly by sending it a SIGTERM or
SIGINT.  For instance:

```
kill -TERM PROCESS_ID_OF_DORY
```

or

```
kill -INT PROCESS_ID_OF_DORY
```

Once Dory has been set up with a basic configuration, you can
[send messages](sending_messages.md).

-----

basic_config.md: Copyright 2014 if(we), Inc.

basic_config.md is licensed under a Creative Commons Attribution-ShareAlike 4.0
International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
