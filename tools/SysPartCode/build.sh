#!/bin/sh

cd analysis/tools/egalito
export USE_LOADER=0
git checkout syspart-updated
make clean
make -j 8
cd ../../app
make clean
make
