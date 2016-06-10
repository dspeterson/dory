## Building and Installing Dory

Once you have finished [setting up your build environment](../README.md#setting-up-a-build-environment),
you are ready to build Dory.  If you are building on CentOS 6, remember to set
your `PATH` and `LD_LIBRARY_PATH` environment variables and activate your
Python virtualenv environment before building, as detailed
[here](centos_6_5_env.md).  The first step is to clone Dory's Git repository:

```
git clone https://github.com/dspeterson/dory.git
```

### Building an RPM package

If you will be running Dory on an RPM-based distribution such as CentOS or
RHEL, you will probably want to build an RPM package.  Here you have two
choices.  You can build either an RPM package that includes Dory's init script
and config files or an RPM package that omits these files.  You might prefer
the latter choice if you want to manage the config files separately using a
tool such as Puppet.  To choose the first option, do the following:

```
cd dory
./pkg rpm
```

To choose the second option, do the following:

```
cd dory
./pkg rpm_noconfig
```

In the former case, the resulting RPM packages can be found in directory
`out/pkg/rpm`.  In the latter case, they can be found in
`out/pkg/rpm_noconfig`.

The init script for Dory (see [config/dory.init](../config/dory.init)) is an
older System V type of script.  Scripts that work with the newer *systemd*
included in CentOS 7 and *upstart* included in recent Ubuntu distributions are
currently not available.  Contributions from the community would be much
appreciated.

### Building Dory Directly

For all platforms except CentOS 6, Dory may be built directly using SCons as
follows:

```
cd dory
source bash_defs
cd src/dory
build --release dory
cd ../..
```

For CentOS 6, the steps are identical except that instead of
`build --release dory` you should type `build --release --import_path dory`.
The `--import_path` option tells the SCons build configuration to use the PATH
environment variable setting from the external environment.  This is needed
so that your customized PATH setting (as described
[here](centos_6_5_env.md#building-and-installing-gcc-482)) is seen and the
newer version of gcc is used.  After performing the above steps, the path to
the newly built Dory executable is now `out/release/dory/dory`.  If you omit
`--release` above, you will create a debug build of Dory and the location of
the executable will be `out/debug/dory/dory`.  Before creating a debug build,
you should read [this](dev_info.md#debug-builds).

### Building Dory's Client Library

Additionally, there is a C library for clients that send messages to Dory.
There is also a simple command line client program that uses the library.
These may be built in the same manner as above:

```
cd dory
source bash_defs
cd src/dory/client
build --release libdory_client.so
build --release libdory_client.a
build --release to_dory
cd ../../..
```

In the case of CentOS 6, remember to use the `--import_path` option with the
`build` command.  As above, if you omit the `--release`, then you will create a
debug build, which is documented [here](dev_info.md#debug-builds).  The newly
built library files and client program are now located in
`out/release/dory/client`.  If installing them manually, rename
`libdory_client.so` to `libdory_client.so.0` when copying it to your system's
library directory (`/usr/lib64` on CentOS/RedHat, or `/usr/lib` on Ubuntu), and
remember to run `/sbin/ldconfig` afterwards.  Also remember to install the
client library header files as follows:

```
mkdir -p /usr/include/dory/client
cp src/dory/client/*.h /usr/include/dory/client
```
One of the header files,
[dory/client/dory_client_socket.h](../src/dory/client/dory_client_socket.h),
provides a simple C++ wrapper class for the library functions that deal with
Dory's UNIX domain datagram socket.  Once the library is installed, C and C++
clients can specify `-ldory_client` when building with gcc.  The client
library, header files, and command line program are included in Dory's RPM
package.  For example C code that uses the client library to send a message to
Dory, see the big comment at the top of
[dory/client/dory_client.h](../src/dory/client/dory_client.h).  For example
C++ code, see [dory/client/to_dory.cc](../src/dory/client/to_dory.cc).

### More Details on Dory's Build System (including debug builds)

Full details on Dory's build system are provided [here](dev_info.md),
including information on [building a debug version of Dory]
(dev_info.md#debug-builds).

### Installing Dory

If you built an RPM package containing Dory, then you can install it using an
RPM command such as:

```
rpm -Uvh out/pkg/rpm/dory-1.0.6.38.g66c5a2d-1.el6.x86_64.rpm
```

Otherwise, you can copy the Dory executable to a location of your choice, such
as `/usr/bin`.  If you are running on CentOS 6, remember that the gcc482 RPM
package described [here](centos_6_5_env.md#building-and-installing-gcc-482)
must be installed, and `LD_LIBRARY_PATH` must contain `/opt/gcc/lib64` in the
shell that you execute Dory from.  If you built your RPM package using the
`rpm_noconfig` option described above, or you built Dory directly using SCons,
you will need to install Dory's init script, sysconfig file, and configuration
file (which mostly contains settings related to batching and compression)
separately.  Assuming you are in the root of the Git repository (where Dory's
[SConstruct](../SConstruct) file is found), you can do this as follows:

```
cp config/dory.init /etc/init.d/dory
cp config/dory.sysconfig /etc/sysconfig/dory
mkdir /etc/dory  # or some other suitable location
cp config/dory_conf.xml /etc/dory
```

At this point, you are ready to
[run Dory with a basic configuration](basic_config.md).

-----

build_install.md: Copyright 2014 if(we), Inc.

build_install.md is licensed under a Creative Commons Attribution-ShareAlike
4.0 International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
