#!/bin/bash

clang++ cpp/*.cpp -O3 --std=c++17 -lbenchmark -g -o perftest && ./perftest
