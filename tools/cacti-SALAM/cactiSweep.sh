#!/bin/bash

input="$M5_PATH/benchmarks.list"
while IFS= read -r line;
do
    [[ $line =~ ^#.* ]] && continue
        python3 $M5_PATH/tools/cacti-SALAM/cactiWrapper.py $line
        sleep 1

done < "$input"
