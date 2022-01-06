#!/bin/bash

mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make VERBOSE=1 -j2
cd ..
