#!/bin/bash -exo pipefail
cd $(git rev-parse --show-toplevel)
cp ~/workspace/credentials/SoulDistance/conf_vendor.json ./chip/fs/conf_vendor.json
cd ./chip
mos build --verbose --local
