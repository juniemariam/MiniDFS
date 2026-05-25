#!/bin/bash
set -euo pipefail

mkdir -p logs

bin/node 5001 > logs/node1.log 2>&1 &
bin/node 5002 > logs/node2.log 2>&1 &
bin/node 5003 > logs/node3.log 2>&1 &

echo "Nodes started on ports 5001, 5002, 5003."

