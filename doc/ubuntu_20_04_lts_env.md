## Setting Up a Ubuntu 20.04 LTS Build Environment

Install software as follows:

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

Now proceed to
[build, install, and configure Dory](build_install.md).

-----

ubuntu_20_04_lts_env.md:
Copyright 2020 Dave Peterson (dave@dspeterson.com)
Copyright 2014 if(we), Inc.

ubuntu_20_04_lts_env.md is licensed under a Creative Commons
Attribution-ShareAlike 4.0 International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
