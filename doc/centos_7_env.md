## Setting Up a CentOS 7 Build Environment

First, install software as follows:

```
yum groupinstall "Development Tools"
yum install libasan
yum install cmake
yum install zlib-devel
yum install boost-devel
yum install python3
python3 -m pip install scons
yum install xerces-c-devel
yum install snappy-devel
```

Then update gcc to version 8:

```
# If you are using CentOS 7:
yum install centos-release-scl

# If you are using RHEL 7:
yum-config-manager --enable rhel-server-rhscl-7-rpms

yum install devtoolset-8
yum install devtoolset-8-libasan-devel
```

Before building Dory, you need to do the following:

```
scl enable devtoolset-8 bash
```

This starts a bash shell that is configured to enable gcc 8 by default.  You
must build Dory from this shell.  Now proceed to
[build, install, and configure Dory](build_install.md).

-----

centos_7_env.md:
Copyright 2019 Dave Peterson (dave@dspeterson.com)
Copyright 2014 if(we), Inc.

centos_7_env.md is licensed under a Creative Commons Attribution-ShareAlike 4.0
International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
