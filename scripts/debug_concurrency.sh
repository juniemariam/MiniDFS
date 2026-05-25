#!/bin/bash
set -euo pipefail

if ! command -v valgrind >/dev/null 2>&1; then
  echo "valgrind is required for concurrency debugging."
  exit 1
fi

TOOL="${1:-helgrind}"
if [[ "$TOOL" != "helgrind" && "$TOOL" != "drd" ]]; then
  echo "Usage: scripts/debug_concurrency.sh [helgrind|drd]"
  exit 1
fi

make
mkdir -p logs data/stress
rm -rf data/nodes data/stress/*

pids=()
client_pids=()
cleanup() {
  for pid in "${pids[@]:-}"; do
    kill "$pid" >/dev/null 2>&1 || true
  done
}
trap cleanup EXIT

valgrind --tool="$TOOL" --log-file="logs/${TOOL}.master.log" bin/master > logs/master.log 2>&1 &
pids+=("$!")
valgrind --tool="$TOOL" --log-file="logs/${TOOL}.node1.log" bin/node 5001 > logs/node1.log 2>&1 &
pids+=("$!")
valgrind --tool="$TOOL" --log-file="logs/${TOOL}.node2.log" bin/node 5002 > logs/node2.log 2>&1 &
pids+=("$!")
valgrind --tool="$TOOL" --log-file="logs/${TOOL}.node3.log" bin/node 5003 > logs/node3.log 2>&1 &
pids+=("$!")

sleep 3

for pid in "${pids[@]}"; do
  if ! kill -0 "$pid" >/dev/null 2>&1; then
    echo "MiniDFS server failed to start. Logs:"
    cat logs/master.log logs/node1.log logs/node2.log logs/node3.log
    exit 1
  fi
done

for i in $(seq 1 6); do
  {
    input="data/stress/debug_input_${i}.txt"
    output="data/stress/debug_output_${i}.txt"
    {
      echo "MiniDFS valgrind concurrency client ${i}"
      seq 1 1500
    } > "$input"

    bin/client upload "$input"
    bin/client download "debug_input_${i}.txt" "$output"
    cmp "$input" "$output"
    bin/client delete "debug_input_${i}.txt"
  } &
  client_pids+=("$!")
done

for pid in "${client_pids[@]}"; do
  wait "$pid"
done

echo "Concurrency debug run completed. Review logs/${TOOL}.*.log for race reports."
