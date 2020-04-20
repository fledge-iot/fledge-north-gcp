#!/usr/bin/env bash

##--------------------------------------------------------------------
## Copyright (c) 2019 Dianomic Systems
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##--------------------------------------------------------------------

##
## Author: Mark Riddoch, Yash Tatkondawar
##


sudo apt-get install cmake

cd /tmp
git clone https://github.com/benmcollins/libjwt.git
git clone https://github.com/akheron/jansson.git
git clone https://github.com/eclipse/paho.mqtt.c.git

cd jansson
mkdir build
cd build
cmake ..
make
sudo make install

cd ../../libjwt
autoreconf -i
./configure
make
sudo make install

sudo apt-get install libssl-dev pkg-config

cd ../paho.mqtt.c
mkdir build
cd build
cmake -DPAHO_BUILD_DOCUMENTATION=FALSE -DPAHO_WITH_SSL=TRUE ..
make
sudo make install
