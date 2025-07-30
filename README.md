# MiniDFS – A Distributed File System in C++

MiniDFS is a simplified distributed file system that simulates the core architecture of large-scale systems like HDFS. It uses C++ for systems-level operations, TCP sockets for network communication, and multithreading for concurrent processing. Built to demonstrate systems engineering skills for low-level distributed infrastructure roles.

## 🚀 Features

- 🧠 Central Metadata Master
- 📂 Storage Nodes with file chunking and replication
- 🧵 Multi-threaded TCP server for concurrent file operations
- ⚡ File upload/download/list/delete
- 🔄 Consistent hashing for distribution and fault tolerance
- 📈 Performance benchmarking using `valgrind` and `perf`

## 🧱 Architecture

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


## 🛠️ Technologies

- C++11
- POSIX Threads (pthreads) / C++ std::thread
- TCP/IP Sockets
- Linux (tested on Ubuntu)
- Bash for testing
- Valgrind for memory profiling

## 🧪 Usage

```bash
make
./master
./node 5001 &
./node 5002 &
./node 5003 &

# Upload
./client upload myfile.txt

# Download
./client download myfile.txt

# List files
./client ls
```

## Testing
bash/tests/test_client.sh


## ✅ 4. **Getting Started – Week-by-Week Plan**

### 🔹 Week 1
- [ ] Set up repo, directory structure, and `Makefile`
- [ ] Implement basic TCP server (single-threaded)
- [ ] Implement client that can send a file

### 🔹 Week 2
- [ ] Implement metadata server to manage filenames and chunk maps
- [ ] Add multithreading to the nodes for handling concurrent clients

### 🔹 Week 3
- [ ] Add consistent hashing + replication logic
- [ ] Implement fault detection (heartbeat or ping every X sec)
- [ ] CLI commands: `upload`, `download`, `ls`, `delete`

### 🔹 Week 4
- [ ] Write test scripts (Bash) for upload/download
- [ ] Add `valgrind`, benchmark upload latency
- [ ] Polish README, add a demo (video or GIF)
