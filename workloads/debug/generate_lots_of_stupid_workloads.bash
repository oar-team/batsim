#!/usr/bin/env bash

for (( i=1; i<=100; i++ ))
do
    ./generate_stupid_workloads.py -n 3 -p delays.json -o workload${i}.json -i 4
done
