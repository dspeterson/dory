## Setting Up a Ubuntu Build Environment (19.04, 18.04, 17.04, 16.04 LTS, 15.04, and 14.04.1 LTS)

Install software packages as follows:

```
sudo apt-get install build-essential
sudo apt-get install scons
sudo apt-get install cmake
sudo apt-get install libsnappy-dev
sudo apt-get install zlib1g-dev
sudo apt-get install libboost-dev
sudo apt-get install libxerces-c-dev
sudo apt-get install git
sudo apt-get install g++
```
Install the appropriate version of libasan, as described
[here](https://askubuntu.com/questions/1024017/difference-between-libasan-packes-libasan0-libasan2-libasan3-etc).
For instance, if `gcc --version` indicates that you are running gcc version
7.x.x, then you would install package `libasan4`.

Now proceed to
[build, install, and configure Dory](build_install.md).

-----

ubuntu_13-15_env.md: Copyright 2014 if(we), Inc.

ubuntu_13-15_env.md is licensed under a Creative Commons
Attribution-ShareAlike 4.0 International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
