## Setting Up a CentOS 6.8 Build Environment

Dory is implemented in C++, and makes extensive use of C++11 features.
Therefore, it must be built using a more recent version of gcc than what CentOS
6.8 provides.  Likewise, a newer version of Python is required to support
Dory's SCons-based Python build scripts.  Some RPM packages are also needed,
which may be installed as follows:

```
yum groupinstall "Development tools"
yum install scons cmake snappy-devel boost-devel xerces-c-devel
```

### Building and Installing gcc 4.8

For simplicity, the following instructions assume that you are running on an
x86-64 architecture Linux installation.  To build and install gcc 4.8, do the
following:

1. First install 32-bit versions of the glibc binaries (in addition to the
   existing 64-bit binaries).  To do this, start by appending the following
   line to `/etc/yum.conf`:
   ```
   multilib_policy=all
   ```
   Now install the packages shown below. This will cause 32-bit versions to be
   installed in addition to the 64-bit versions that are already installed.
   ```
   yum install glibc glibc-devel glibc-static
   ```
   Finally, remove the line you appended to `/etc/yum.conf` above.

2. Next execute the following commands:

   ```
   yum install elfutils-devel systemtap-runtime zlib-devel gettext \
               dejagnu bison flex texinfo sharutils gmp-devel mpfr-devel
   wget http://dl.fedoraproject.org/pub/epel/6/x86_64/libmpc-0.8-3.el6.x86_64.rpm
   wget http://dl.fedoraproject.org/pub/epel/6/x86_64/libmpc-devel-0.8-3.el6.x86_64.rpm
   rpm -Uvh libmpc-0.8-3.el6.x86_64.rpm
   rpm -Uvh libmpc-devel-0.8-3.el6.x86_64.rpm
   yum install rpm-build
   mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}  # or use a directory of your choice
   git clone https://github.com/dspeterson/dory.git
   cp dory/centos6/gcc48.spec ~/rpmbuild/SPECS
   wget http://mirrors.kernel.org/gnu/gcc/gcc-4.8.5/gcc-4.8.5.tar.bz2
   cp gcc-4.8.5.tar.bz2 ~/rpmbuild/SOURCES
   cd ~/rpmbuild
   rpmbuild --define "topdir `pwd`" -ba SPECS/gcc48.spec
   rpm -Uvh RPMS/x86_64/gcc48-4.8.5-1.el6.x86_64.rpm
   ```

Now that gcc 4.8 has been built and installed, you should do the following
before attempting to build and execute programs with the new compiler version:
```
export PATH=/usr/local/gcc-4.8.5/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/gcc-4.8.5/lib64
```

### Building and Installing Python 2.7

To build and install Python 2.7.3, do the following:

```
yum install zlib-devel
yum install bzip2-devel
yum install openssl-devel
yum install ncurses-devel
yum install sqlite-devel
yum install readline-devel
yum install tk-devel
wget http://python.org/ftp/python/2.7.3/Python-2.7.3.tar.bz2
tar xf Python-2.7.3.tar.bz2
cd Python-2.7.3
./configure --prefix=/usr/local
make
make altinstall  # Warning: Do _not_ type "make install".
cd ..
wget http://pypi.python.org/packages/source/d/distribute/distribute-0.6.35.tar.gz
tar xf distribute-0.6.35.tar.gz
cd distribute-0.6.35
python2.7 setup.py install
easy_install-2.7 virtualenv
easy_install-2.7 argparse
```

### Using Python 2.7 inside a virtualenv Environment

For building Dory, you need to create a Python 2.7 virtualenv environment,
which may be done as follows:

```
virtualenv --distribute ~/dory_env  # or use a directory of your choice
```

Now activate the virtualenv environment as follows:

```
$ source ~/dory_env/bin/activate
$ python --version
Python 2.7.3
$
```
As shown above, when executing commands in the shell where you activated the
virtualenv environment, Python 2.7.3 will be used by default.  Before building
Dory, you must perform this step in the shell where you are doing the build.

Now proceed to
[build, install, and configure Dory](build_install.md).

-----

centos_6_8_env.md: Copyright 2014 if(we), Inc.

centos_6_8_env.md is licensed under a Creative Commons Attribution-ShareAlike
4.0 International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
