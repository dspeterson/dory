## Setting Up a Debian Build Environment (version 9 or 8)

These instructions are based on limited familiarity with Debian, and can
probably be improved a lot.  Suggestions for improvements would be appreciated.

First use `apt-get` to install packages as follows:

```
sudo apt-get update
sudo apt-get install build-essential
sudo apt-get install cmake
sudo apt-get install zlib1g-dev
sudo apt-get install libxerces-c-dev
sudo apt-get install git
```
Then install Scons, Boost C++ libraries, and the Snappy compression library.
Depending on the version of Debian, you may be able to do the following:
```
sudo apt-get install scons
sudo apt-get install libsnappy-dev
sudo apt-get install libboost-dev
```
On Debian 8, you may instead have to use the `dpkg -i <some package>.deb`
command to install the packages from the [DVD images for installing Debian]
(https://www.debian.org/CD/http-ftp/) as follows:

* Scons.  For instance, you can install the following package from the Debian
  8.2.0 DVD #2: `pool/main/s/scons/scons_2.3.1-2_all.deb`
* libsnappy1.  For instance, you can install the following package from the
  Debian 8.2.0 DVD #2: `pool/main/s/snappy/libsnappy1_1.1.2-3_amd64.deb`
* libsnappy-dev.  For instance, you can install the following package from the
  Debian 8.2.0 DVD #3: `pool/main/s/snappy/libsnappy-dev_1.1.2-3_amd64.deb`
* Boost C++ libraries.  For instance, you can install the following package
  from the Debian 8.2.0 DVD #2:
  `pool/main/b/boost1.55/libboost1.55-dev_1.55.0+dfsg-3_amd64.deb`

Install the appropriate version of libasan, as described
[here](https://askubuntu.com/questions/1024017/difference-between-libasan-packes-libasan0-libasan2-libasan3-etc).
For instance, if `gcc --version` indicates that you are running gcc version
7.x.x, then you would install package `libasan4`.

Now proceed to
[build, install, and configure Dory](build_install.md).

-----

debian_env.md: Copyright 2016 Dave Peterson <dave@dspeterson.com>

debian_env.md is licensed under a Creative Commons Attribution-ShareAlike 4.0
International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
