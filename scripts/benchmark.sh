#!/bin/bash

echo "Benchmarking upload performance..."
valgrind --tool=callgrind ./client upload benchmark.txt
