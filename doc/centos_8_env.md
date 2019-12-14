## Setting Up a CentOS 8 Build Environment

First, install software as follows:

```
yum groupinstall "Development Tools"
yum install libasan
yum install cmake
yum install zlib-devel
yum install boost-devel
yum install python3
python3 -m pip install scons
yum install epel-release  # needed for xerces-c-devel
yum install xerces-c-devel
```

Then edit `/etc/yum.repos.d/CentOS-PowerTools.repo` and change `enabled=0` to
`enabled=1`.  This is needed for installing `snappy-devel`.  Install
`snappy-devel` as follows:

```
yum install snappy-devel
```

Now proceed to
[build, install, and configure Dory](build_install.md).

-----

centos_8_env.md: Copyright 2019 Dave Peterson (dave@dspeterson.com)

centos_8_env.md is licensed under a Creative Commons Attribution-ShareAlike 4.0
International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
