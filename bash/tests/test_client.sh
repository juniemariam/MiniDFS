#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

make
mkdir -p logs data/test
rm -rf data/nodes

pids=()
cleanup() {
  for pid in "${pids[@]:-}"; do
    kill "$pid" >/dev/null 2>&1 || true
  done
}
trap cleanup EXIT

bin/master > logs/master.log 2>&1 &
pids+=("$!")
bin/node 5001 > logs/node1.log 2>&1 &
pids+=("$!")
bin/node 5002 > logs/node2.log 2>&1 &
pids+=("$!")
bin/node 5003 > logs/node3.log 2>&1 &
pids+=("$!")

sleep 1

printf "hello MiniDFS\n%.0s" {1..10000} > data/test/input.txt

bin/client upload data/test/input.txt
bin/client ls | grep "input.txt"
bin/client download input.txt data/test/output.txt
cmp data/test/input.txt data/test/output.txt
bin/client delete input.txt

if bin/client download input.txt data/test/after-delete.txt >/dev/null 2>&1; then
  echo "download unexpectedly succeeded after delete"
  exit 1
fi

echo "MiniDFS client test passed."

