# MiniDFS - A Distributed File System in C++

MiniDFS is a compact distributed file system that demonstrates the core architecture of systems such as HDFS: a central metadata master, multiple replicated storage nodes, and a client that uploads, downloads, lists, and deletes files over TCP.

## Features

- Central metadata master that tracks files, chunks, and replica locations
- Storage nodes that persist file chunks on disk
- File chunking with configurable chunk size in `common/utils.hpp`
- Replication across three storage nodes
- Consistent hashing for chunk-to-node placement
- Multithreaded TCP servers for concurrent master and storage-node requests
- Parallel client upload, download, and delete operations
- File upload/download/list/delete CLI
- Bash integration test
- Performance benchmarking script using Valgrind callgrind and `perf` when installed

## Architecture

```text
        +-------------+
        |   Client    |
        +------+------+
               |
               v
    +----------+----------+
    |     Metadata Master |
    +----------+----------+
               |
     -----------------------
     |         |          |
     v         v          v
    [Node1]   [Node2]    [Node3]
```

The client asks the master for a chunk placement plan. The master uses consistent hashing to choose replica nodes for each chunk. The client then transfers chunk payloads directly to storage nodes in parallel.

## Build

```bash
make
```

Binaries are written to `bin/`.

## Usage

Start the master and three storage nodes:

```bash
bin/master &
bin/node 5001 &
bin/node 5002 &
bin/node 5003 &
```

Or start the nodes with:

```bash
scripts/start_nodes.sh
```

Run client commands:

```bash
bin/client upload myfile.txt
bin/client download myfile.txt restored.txt
bin/client ls
bin/client delete myfile.txt
```

Runtime chunk data is stored under `data/nodes/`.

## Testing

```bash
make test
```

## Benchmarking

Start the master and nodes first, then run:

```bash
scripts/benchmark.sh
```

