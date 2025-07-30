#!/bin/bash

echo "Running client test..."
./client upload testfile.txt
./client download testfile.txt
