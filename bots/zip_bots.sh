#!/bin/bash
# Zip all directories ending in '_player' for submission.

bash sync_lib.sh

for player in *_player; do
    rm ${player}.zip
    zip -r ${player}.zip ${player}
done
