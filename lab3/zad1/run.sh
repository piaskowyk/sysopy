#!/usr/bin/env bash

make clean_all
make main1
make main2
make clean

echo "Search in directory /home, (using diropen)"
./main1 /home/mleko/tmp '>' 19/03/2019 00:00:00
echo ""
echo "----------------------------------------------------------------------"
echo ""
echo "Search in directory /home, (using ftw)"
./main2 /home/mleko/tmp '>' 19/03/2019 00:00:00

echo "End"