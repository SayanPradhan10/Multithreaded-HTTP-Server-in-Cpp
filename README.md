#
Multithreaded-HTTP-Server-in-C++
A lightweight, high-performance HTTP/1.1 web server built from scratch in C++17 using POSIX/BSD TCP sockets, a custom thread pool with a thread-safe FIFO task queue, and basic HTTP routing. Designed for educational clarity and technical interview demonstration.

---

## Table of Contents
1. [Project Overview](#project-overview)
2. [Architecture Diagram](#architecture-diagram)
3. [TCP Socket Lifecycle](#tcp-socket-lifecycle)
4. [Thread Pool & Task Queue Workflow](#thread-pool--task-queue-workflow)
5. [HTTP Request Lifecycle](#http-request-lifecycle)
6. [Project Structure](#project-structure)
7. [Build Instructions](#build-instructions)
8. [Run Instructions](#run-instructions)
9. [API Endpoints](#api-endpoints)
10. [Dashboard](#dashboard)
11. [Benchmark Instructions](#benchmark-instructions)
12. [Graceful Shutdown](#graceful-shutdown)
13. [Key Technical Interview Questions](#key-technical-interview-questions)
14. [Design Decisions & Limitations](#design-decisions--limitations)

---

## 1. Project Overview

This project implements a concurrent HTTP server without external networking or threading libraries (e.g., no Boost.Asio, no libuv). It demonstrates fundamental systems programming concepts:
- Low-level network socket programming (`socket`, `bind`, `listen`, `accept`, `recv`, `send`)
- Multithreading and synchronization (`std::thread`, `std::mutex`, `std::condition_variable`, `std::queue`)
- Producer-consumer concurrency pattern (fixed worker thread pool)
- HTTP/1.1 protocol parsing (request lines, headers, query parameters)
- Thread-safe synchronized console logging
- Dynamic JSON API responses and static file serving (HTML/CSS/JS)
- Real-time server performance metrics and benchmarking

---

## 2. Architecture Diagram

```
                         ┌──────────────────────┐
                         │      Web Browser     │
                         │  localhost:8080      │
                         └──────────┬───────────┘
                                    │
                              HTTP Request
                                    │
                                    ▼
                         ┌──────────────────────┐
                         │     HTTP Server      │
                         │    Main Thread       │
                         │                      │
                         │ socket()             │
                         │ bind()               │
                         │ listen()             │
                         │ accept()             │
                         └──────────┬───────────┘
                                    │
                              Client Socket FD
                                    │
                                    ▼
                         ┌──────────────────────┐
                         │      Task Queue      │
                         │                      │
                         │  Thread-safe FIFO    │
                         │  mutex + CV          │
                         └──────────┬───────────┘
                                    │
                              Worker takes task
                                    │
                                    ▼
               ┌─────────────────────────────────────────┐
               │              THREAD POOL                │
               │                                         │
               │ Worker 1  Worker 2  ...  Worker 8       │
               │                                         │
               └────────────────────┬────────────────────┘
                                    │
                                    ▼
                         ┌──────────────────────┐
                         │   HTTP Request       │
                         │      Parser          │
                         │                      │
                         │ Method               │
                         │ Path                 │
                         │ Query Parameters     │
                         │ Headers              │
                         └──────────┬───────────┘
                                    │
                                    ▼
                         ┌──────────────────────┐
                         │       Router         │
                         └──────────┬───────────┘
                                    │
                 ┌──────────────────┼──────────────────┐
                 │                  │                  │
                 ▼                  ▼                  ▼
          /api/hello          /api/stats       /api/compute
                                                     ?n=30
                 │                  │                  │
                 └──────────────────┼──────────────────┘
                                    │
                                    ▼
                         ┌──────────────────────┐
                         │   HTTP Response      │
                         │                      │
                         │ Status Code          │
                         │ Headers              │
                         │ Body                 │
                         └──────────┬───────────┘
                                    │
                                  send()
                                    │
                                    ▼
                         ┌──────────────────────┐
                         │      Browser         │
                         └──────────────────────┘
```

---

## 3. TCP Socket Lifecycle

The server utilizes standard POSIX/BSD socket system calls:

1. **`socket()`**: Creates an endpoint for communication (`AF_INET` for IPv4, `SOCK_STREAM` for reliable TCP byte streams).
2. **`setsockopt(SO_REUSEADDR)`**: Allows immediate re-binding of the port after server restarts, avoiding `TIME_WAIT` socket binding errors (`EADDRINUSE`).
3. **`bind()`**: Associates the socket descriptor with a network address (IP `INADDR_ANY`) and port number (e.g., `8080`).
4. **`listen()`**: Marks the socket as passive to accept incoming connection requests with a backlog queue (`SOMAXCONN`).
5. **`accept()`**: Blocks the main thread until a new TCP handshake completes, returning a dedicated client socket file descriptor (`clientFd`).
6. **`recv()`**: Worker threads read raw request bytes sent across the TCP stream.
7. **`send()`**: Worker threads transmit formatted HTTP/1.1 response bytes back across the TCP stream.
8. **`close()`**: Closes the client socket descriptor once the response is delivered (`Connection: close`).

---

## 4. Thread Pool & Task Queue Workflow

To prevent the overhead of creating and destroying OS threads per connection (which causes thread thrashing and memory exhaustion under load), the server uses a **Fixed Thread Pool** with a **Producer-Consumer Task Queue**:

```
Main Server Thread (Producer)
        │
        │ 1. accept() connection
        │ 2. lock(queueMutex)
        ▼
┌──────────────────┐
│    Task Queue    │  <--- std::queue<std::function<void()>>
└────────┬─────────┘
         │
         │ 3. cv.notify_one()
         ▼
Worker Thread (Consumer)
         │
         │ 4. cv.wait(lock, condition)
         │ 5. pop() task
         │ 6. unlock(queueMutex)
         │ 7. execute task() -> handleClient(clientFd)
```

- **Thread-safe FIFO**: Backed by `std::queue<std::function<void()>>`, protected by `std::mutex`.
- **Condition Variable (`std::condition_variable`)**: Worker threads go to sleep when the queue is empty, consuming zero CPU cycles. When a client socket task arrives, `notify_one()` awakens an available worker.
- **Worker Isolation**: The queue lock is held only briefly to push/pop tasks. Request parsing, CPU execution, and socket I/O take place completely outside the lock.

---

## 5. HTTP Request Lifecycle

1. **Raw Ingestion**: Read from socket into buffer using `recv()`.
2. **Parsing**:
   - **Request Line**: Extracted into `METHOD` (e.g., `GET`), `URI` (e.g., `/api/compute?n=30`), and `VERSION` (e.g., `HTTP/1.1`).
   - **URI Parsing**: Separates root path (`/api/compute`) from query string (`n=30`). Query key-value pairs are stored in an unordered map.
   - **Headers**: Key-value pairs (`Header-Name: Value`) read until an empty line (`\r\n\r\n`).
3. **Routing**: Evaluates method and path to invoke the appropriate handler.
4. **Response Serialization**: Formats status line, headers (`Content-Type`, `Content-Length`, `Connection: close`), and payload body into a valid HTTP/1.1 wire string.
5. **Transmission & Closure**: Sends data via `send()` and closes `clientFd`.

---

## 6. Project Structure

```
multithreaded-http-server/
│
├── include/
│   ├── HttpServer.hpp      # Main socket lifecycle and routing
│   ├── HttpRequest.hpp     # HTTP request line, header & query parser
│   ├── HttpResponse.hpp    # HTTP response builder & serializer
│   ├── ThreadPool.hpp      # Fixed thread pool with CV & mutex
│   └── Logger.hpp          # Synchronized thread-safe console logging
│
├── src/
│   ├── main.cpp            # Entry point & signal handling
│   ├── HttpServer.cpp      # Server loop & endpoint handlers
│   ├── HttpRequest.cpp     # Request parsing implementation
│   ├── HttpResponse.cpp    # Response formatting implementation
│   ├── ThreadPool.cpp      # Worker thread loop & synchronization
│   └── Logger.cpp          # Log level formatting and output
│
├── public/                 # Static web dashboard
│   ├── index.html          # Dashboard UI structure
│   ├── style.css           # Dark theme styling
│   └── script.js           # API tester & live polling
│
├── benchmark/
│   └── benchmark.py        # Concurrency & latency benchmark tool
│
├── Makefile                # Build configuration (g++ -std=c++17)
└── README.md               # Documentation
```

---

## 7. Build Instructions

### Prerequisites
- Modern C++ compiler supporting C++17 (`g++` >= 7.0 or `clang++` >= 5.0)
- POSIX-compliant operating system (Linux or macOS)
- Python 3.7+ (for running the benchmark script)

### Compilation
Run `make` from the project root:
```bash
make
```

To clean build artifacts:
```bash
make clean
```

---

## 8. Run Instructions

Start the server specifying the port and worker thread count:
```bash
./server [port] [worker_threads]
```

### Examples:
Run on default port 8080 with 8 worker threads:
```bash
./server
```

Run on port 9000 with 16 worker threads:
```bash
./server 9000 16
```

---

## 9. API Endpoints

| Method | Endpoint | Description | Sample Response |
|---|---|---|---|
| `GET` | `/` | Serves static dashboard UI | `public/index.html` |
| `GET` | `/style.css` | Serves dashboard stylesheet | `public/style.css` |
| `GET` | `/script.js` | Serves dashboard client script | `public/script.js` |
| `GET` | `/api/hello` | Returns simple JSON greeting | `{"message": "Hello from C++ HTTP Server"}` |
| `GET` | `/api/stats` | Returns real-time server runtime metrics | `{"total_requests": 1008, "worker_threads": 8, "active_workers": 1, "queued_tasks": 0, "uptime_seconds": 83}` |
| `GET` | `/api/compute?n=30` | Iteratively computes Fibonacci(n) | `{"input": 30, "result": 832040}` |
| `GET` | `/unknown` | Unmatched path returns 404 Not Found | `{"error": "Not Found", "path": "/unknown"}` |

---

## 10. Dashboard

When the server is running, open your browser to:
```
http://localhost:8080
```

The interactive dashboard features:
- **Server Status**: Live indicator (`ONLINE` / `OFFLINE`).
- **Real-Time Metrics Cards**: Port, Total Requests, Worker Threads, Active Workers, Queued Tasks, and Uptime.
- **Interactive API Tester**: Test `/api/hello`, `/api/stats`, `/api/compute?n=X`, and 404 endpoints with formatted JSON responses.
- **Auto-Refresh**: Polls `/api/stats` every 2 seconds without full-page reloading.

---

## 11. Benchmark Instructions

A standalone Python 3 benchmark tool is provided in `benchmark/benchmark.py`.

### Usage:
```bash
python3 benchmark/benchmark.py <URL> <TOTAL_REQUESTS> <CONCURRENCY>
```

### Example:
Benchmark `/api/hello` with 1,000 total requests across 20 concurrent threads:
```bash
python3 benchmark/benchmark.py http://localhost:8080/api/hello 1000 20
```

### Sample Benchmark Output:
```
=======================================================
       MULTITHREADED HTTP SERVER BENCHMARK
=======================================================
Target URL:        http://localhost:8080/api/hello
Total Requests:    1000
Concurrency Level: 20
-------------------------------------------------------
Running benchmark... Please wait.
-------------------------------------------------------
BENCHMARK RESULTS
-------------------------------------------------------
Total Requests:      1000
Successful Requests: 1000
Failed Requests:     0
Total Time:          0.300 s
Requests/sec:        3337.52 req/s
Average Latency:     5.93 ms
p50 Latency:         5.06 ms
p95 Latency:         10.84 ms
p99 Latency:         23.41 ms
=======================================================
```

---

## 12. Graceful Shutdown

When `Ctrl + C` (`SIGINT`) or `SIGTERM` is sent:
1. Signal handler catches the interrupt and signals `server.stop()`.
2. Server socket calls `shutdown(serverFd, SHUT_RDWR)` and `close(serverFd)`, which unblocks the listening `accept()` call on the main thread.
3. The thread pool sets its stopping flag to `true` and broadcasts `cv.notify_all()`.
4. Any currently executing requests complete their execution.
5. All worker threads are cleanly joined (`thread.join()`).
6. The process exits with code 0 without leaking descriptors or threads.

---
