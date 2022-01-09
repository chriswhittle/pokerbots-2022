#!/bin/bash

set -x

### Sync the lib code into the bot directories.

for player in *_player; do
    (cd ${player} && [[ -x ./build.sh ]] && ./build.sh)
done
