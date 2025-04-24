#!/bin/bash

chmod u+x cactiStats.sh
chmod u+x cactiSweep.sh
chmod u+x clearStats.sh
chmod u+x getcactiResults.sh

cd $M5_PATH/ext/mcpat/cacti

make clean
make all

echo " "
echo "Testing Install"
echo " "

sleep 1
./cacti -infile cache.cfg

echo "=================================================================="
echo "Usage: ./cactiSweep config.yml bench_name config_name"
echo "bench_name and config_name are only for grouping"
#should make them optional at some point
