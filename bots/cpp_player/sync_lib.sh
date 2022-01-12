#!/bin/bash

NBUCKETS=500

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

mkdir libs/cfr_bot/
cp -R ../../cfr_bot/*.h libs/cfr_bot/
cp -R ../../cfr_bot/src libs/cfr_bot/src
cp -R ../../cfr_bot/CMakeLists.txt libs/cfr_bot/

mkdir libs/cpptqdm
cp -R ../../cpptqdm/*.h libs/cpptqdm

find libs/* -type f -print0 -not -path "libs/skeleton*" | xargs -0 chmod -w

rm -rf data
mkdir data
# NOTE: Assumes the data dir lives here
DATA_DIR="../../../data"
if [ -d $DATA_DIR ]; then
    # change infosets here
    cp -R $DATA_DIR/cfr_data/infosets.bin data/infosets.bin

    cp -R $DATA_DIR/equity_data/flop_buckets_$NBUCKETS.txt data/flop_buckets.txt
    cp -R $DATA_DIR/equity_data/turn_clusters_$NBUCKETS.txt data/turn_clusters.txt
    cp -R $DATA_DIR/equity_data/river_clusters_$NBUCKETS.txt data/river_clusters.txt
    
    cp -R $DATA_DIR/equity_data/flop_clusters_$NBUCKETS.txt data/flop_clusters.txt
else
    echo "ERROR: files not found in $(pwd)/${DATA_DIR}"
fi

find data -type f -print0 | xargs -0 chmod -w
