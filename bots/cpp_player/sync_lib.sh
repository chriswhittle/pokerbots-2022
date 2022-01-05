#!/bin/bash

NALLOCBUCKETS=1
NBUCKETS=150

set -x

# delete all cached build data
find build -mindepth 1 -prune -exec rm -rf {} \;

# delete everything except skeleton in libs
find libs -mindepth 1 -not -path "libs/skeleton*" -type d -prune -exec rm -rf {} \;

# make directories and copy libs
mkdir libs/eval7pp/
cp -R ../../eval7pp/*.h libs/eval7pp/
cp -R ../../eval7pp/src libs/eval7pp/src
cp -R ../../eval7pp/CMakeLists.txt libs/eval7pp/

mkdir libs/real_poker_cpp/
cp -R ../../real_poker_cpp/*.h libs/real_poker_cpp/
cp -R ../../real_poker_cpp/src libs/real_poker_cpp/src
cp -R ../../real_poker_cpp/CMakeLists.txt libs/real_poker_cpp/

mkdir libs/cpptqdm
cp -R ../../cpptqdm/*.h libs/cpptqdm

find libs/* -type f -print0 -not -path "libs/skeleton*" | xargs -0 chmod -w

rm -rf data
mkdir data
# NOTE: Assumes the data dir lives here
DATA_DIR="../../../pokerbots-2021-data"
if [ -d $DATA_DIR ]; then
    # change infosets here
    cp -R $DATA_DIR/cpp/cfr_data/cpp_poker_infosets_cwhittle-buckets150_final.txt data/infosets.txt

    cp -R $DATA_DIR/cpp/equity_data/alloc_buckets_${NALLOCBUCKETS}.txt data/alloc_buckets.txt
    cp -R $DATA_DIR/cpp/equity_data/preflop_equities.txt data/
    cp -R $DATA_DIR/cpp/equity_data/flop_buckets_$NBUCKETS.txt data/flop_buckets.txt
    cp -R $DATA_DIR/cpp/equity_data/turn_clusters_$NBUCKETS.txt data/turn_clusters.txt
    cp -R $DATA_DIR/cpp/equity_data/river_clusters_$NBUCKETS.txt data/river_clusters.txt
    
    cp -R $DATA_DIR/cpp/equity_data/flop_clusters_$NBUCKETS.txt data/flop_clusters.txt
else
    echo "ERROR: you need to link the dropbox Pokerbots 2021 dir to $(pwd)/${DATA_DIR}"
fi

find data -type f -print0 | xargs -0 chmod -w
