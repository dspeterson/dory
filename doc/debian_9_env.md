## Setting Up a Debian 9 (Stretch) Build Environment

First install software as follows:

```
sudo apt-get update
sudo apt-get install build-essential
sudo apt-get install cmake
sudo apt-get install zlib1g-dev
sudo apt-get install libxerces-c-dev
sudo apt-get install git
sudo apt-get install libboost-dev
sudo apt-get install libsnappy-dev
sudo apt-get install python3-pip
sudo python3 -m pip install scons
```

Then update gcc to version 8 as follows.  First append the following line to
`/etc/apt/sources.list`:

```
deb http://deb.debian.org/debian testing main
```

Then proceed as follows:

```
sudo apt-get update
sudo apt-get -t testing install g++-8
sudo update-alternatives --remove-all cpp
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 100 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-8 \
    --slave /usr/bin/cpp cpp /usr/bin/cpp-8 \
    --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-8 \
    --slave /usr/bin/gcc-nm gcc-nm /usr/bin/gcc-nm-8 \
    --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-8 \
    --slave /usr/bin/gcov gcov /usr/bin/gcov-8 \
    --slave /usr/bin/gcov-dump gcov-dump /usr/bin/gcov-dump-8 \
    --slave /usr/bin/gcov-tool gcov-tool /usr/bin/gcov-tool-8
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 50 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-6 \
    --slave /usr/bin/cpp cpp /usr/bin/cpp-6 \
    --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-6 \
    --slave /usr/bin/gcc-nm gcc-nm /usr/bin/gcc-nm-6 \
    --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-6 \
    --slave /usr/bin/gcov gcov /usr/bin/gcov-6 \
    --slave /usr/bin/gcov-dump gcov-dump /usr/bin/gcov-dump-6 \
    --slave /usr/bin/gcov-tool gcov-tool /usr/bin/gcov-tool-6
```

This configures gcc 8 as the systemwide default version.  To switch between gcc
versions, execute the following:

```
sudo update-alternatives --config gcc
```

Now proceed to
[build, install, and configure Dory](build_install.md).

-----

debian_9_env.md: Copyright 2019 Dave Peterson <dave@dspeterson.com>

debian_9_env.md is licensed under a Creative Commons Attribution-ShareAlike 4.0
International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
