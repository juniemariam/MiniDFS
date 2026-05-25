#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

make
mkdir -p logs data/stress
rm -rf data/nodes data/stress/*

pids=()
client_pids=()
delete_pids=()
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

for pid in "${pids[@]}"; do
  if ! kill -0 "$pid" >/dev/null 2>&1; then
    echo "MiniDFS server failed to start. Logs:"
    cat logs/master.log logs/node1.log logs/node2.log logs/node3.log
    exit 1
  fi
done

for i in $(seq 1 8); do
  {
    input="data/stress/input_${i}.txt"
    output="data/stress/output_${i}.txt"
    {
      echo "MiniDFS concurrent client ${i}"
      seq 1 2000
    } > "$input"

    bin/client upload "$input"
    bin/client download "input_${i}.txt" "$output"
    cmp "$input" "$output"
  } &
  client_pids+=("$!")
done

for pid in "${client_pids[@]}"; do
  wait "$pid"
done

bin/client ls

for i in $(seq 1 8); do
  bin/client delete "input_${i}.txt" &
  delete_pids+=("$!")
done

for pid in "${delete_pids[@]}"; do
  wait "$pid"
done

echo "MiniDFS concurrent client stress test passed."
