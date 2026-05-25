# MiniDFS Debugging Notes

MiniDFS has two debugging paths:

- `scripts/benchmark.sh` uses Valgrind callgrind and `perf stat` to profile upload cost.
- `scripts/debug_concurrency.sh` runs the master and storage nodes under Valgrind Helgrind or DRD while concurrent clients upload, download, compare, and delete files.

## Concurrency Issue Addressed

The storage nodes are multithreaded: every accepted TCP connection is handled by a detached worker thread. That means two client operations can touch the same chunk file at the same time, for example:

- one client downloads a chunk while another client deletes it
- repeated uploads overwrite the same chunk while another client reads it

To prevent races around on-disk chunk access, `StorageHandler` protects `store_chunk`, `retrieve_chunk`, and `delete_chunk` with a mutex. The metadata master also protects its file metadata map with a mutex.

## Running Thread Debugging

On Linux with Valgrind installed:

```bash
scripts/debug_concurrency.sh helgrind
scripts/debug_concurrency.sh drd
```

Review the generated logs:

```bash
less logs/helgrind.master.log
less logs/helgrind.node1.log
less logs/drd.master.log
less logs/drd.node1.log
```

The concurrent stress workload can also be run without Valgrind:

```bash
bash bash/tests/test_concurrent_clients.sh
```

