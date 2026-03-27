# High-Performance HTTP Server

Event-driven HTTP/1.1 server using **epoll** (Linux).

## Features
- ✅ epoll-based event loop (O(1) scalability)
- ✅ Edge-triggered mode
- ✅ HTTP/1.1 with Keep-Alive
- ✅ Static file serving
- ✅ Connection state machine
- ✅ File caching
- ✅ Timeout handling (30s)
- ✅ 50,000+ requests/sec

## Architecture

EventLoop (epoll_wait)
  ├─ Connection Manager (linked list for timeouts)
  ├─ State Machine (READING → WRITING → CLOSED)
  ├─ HTTP Parser
  ├─ Response Builder
  └─ File Cache


## Performance
Tested with Apache Bench:
bash
ab -k -c 20000 -n 500000 http://127.0.0.1:9090/


Results: **XX,XXX requests/sec**

## Build
bash
gcc -O3 src/server.c src/main.c -o my_server


## Run
bash
./my_server


Server listens on port **9090**.

## Test Files
Create test files in project directory:
bash
echo "<h1>Hello World!</h1>" > index.html


## Benchmark
bash
ab -k -c 1000 -n 100000 http://127.0.0.1:9090/index.html


## Project Structure

high-performance-http-server/
├── include/
│   └── server.h
├── src/
│   ├── main.c
│   └── server.c
├── index.html
└── README.md


## Date
March 2026
