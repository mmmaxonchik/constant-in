#!/bin/sh


cd analysis/tools/egalito
make clean
make -j 8
cd ../../app
make clean
make
