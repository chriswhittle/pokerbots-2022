#!/bin/bash

set -x
INCLUDES="-I../cpptqdm/ -I../eval7pp/"
# g++ -std=c++2a ${INCLUDES} -O2 -g -pg mccfr.cpp -o mccfr.exe
g++ -std=c++2a ${INCLUDES} -O3 -DNDEBUG=1 mccfr.cpp -o mccfr.exe
# g++ -std=c++2a ${INCLUDES} -O3 -DNDEBUG=1 run_equity_calcs.cpp -o run_equity_calcs.exe
# g++ -std=c++2a ${INCLUDES} -O0 test_game.cpp -o test_game.exe
g++ -std=c++2a ${INCLUDES} -O3 eval_cfr.cpp -o eval_cfr.exe

mkdir -p build
cd build
cmake -DFULL_BUILD=1 ..
make VERBOSE=1
