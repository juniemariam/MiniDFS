#!/bin/bash
set -euo pipefail

make
mkdir -p data/bench
dd if=/dev/urandom of=data/bench/benchmark.bin bs=64k count=16 status=none

echo "Benchmarking upload with valgrind/callgrind..."
valgrind --tool=callgrind --callgrind-out-file=logs/callgrind.client.out bin/client upload data/bench/benchmark.bin

if command -v perf >/dev/null 2>&1; then
  echo "Benchmarking upload with perf..."
  perf stat bin/client upload data/bench/benchmark.bin
else
  echo "perf not installed; skipped perf run."
fi

