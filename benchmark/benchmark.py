#!/usr/bin/env python3
"""
Simple HTTP Benchmark Tool for Multithreaded HTTP Server
Usage:
    python3 benchmark/benchmark.py <url> <total_requests> <concurrency>
Example:
    python3 benchmark/benchmark.py http://localhost:8080/api/hello 1000 20
"""

import sys
import time
import urllib.request
import urllib.error
from concurrent.futures import ThreadPoolExecutor, as_completed

def calculate_percentile(sorted_data, percentile):
    """Calculate percentile from a sorted list of floats."""
    if not sorted_data:
        return 0.0
    k = (len(sorted_data) - 1) * (percentile / 100.0)
    f = int(k)
    c = f + 1
    if c < len(sorted_data):
        d0 = sorted_data[f] * (c - k)
        d1 = sorted_data[c] * (k - f)
        return d0 + d1
    else:
        return sorted_data[f]

def send_request(url):
    """Sends a single GET request and returns (success: bool, latency_ms: float)."""
    start_time = time.perf_counter()
    try:
        req = urllib.request.Request(
            url,
            headers={'User-Agent': 'BenchmarkClient/1.0', 'Connection': 'close'}
        )
        with urllib.request.urlopen(req, timeout=10) as response:
            _ = response.read()
            status = response.getcode()
            elapsed_ms = (time.perf_counter() - start_time) * 1000.0
            return (status == 200, elapsed_ms)
    except Exception:
        elapsed_ms = (time.perf_counter() - start_time) * 1000.0
        return (False, elapsed_ms)

def run_benchmark(url, total_requests, concurrency):
    print("=" * 55)
    print("       MULTITHREADED HTTP SERVER BENCHMARK")
    print("=" * 55)
    print(f"Target URL:        {url}")
    print(f"Total Requests:    {total_requests}")
    print(f"Concurrency Level: {concurrency}")
    print("-" * 55)
    print("Running benchmark... Please wait.")

    latencies = []
    success_count = 0
    failure_count = 0

    benchmark_start = time.perf_counter()

    with ThreadPoolExecutor(max_workers=concurrency) as executor:
        futures = [executor.submit(send_request, url) for _ in range(total_requests)]
        for future in as_completed(futures):
            success, latency = future.result()
            latencies.append(latency)
            if success:
                success_count += 1
            else:
                failure_count += 1

    total_time = time.perf_counter() - benchmark_start
    latencies.sort()

    req_per_sec = total_requests / total_time if total_time > 0 else 0
    avg_latency = sum(latencies) / len(latencies) if latencies else 0
    p50 = calculate_percentile(latencies, 50)
    p95 = calculate_percentile(latencies, 95)
    p99 = calculate_percentile(latencies, 99)

    print("-" * 55)
    print("BENCHMARK RESULTS")
    print("-" * 55)
    print(f"Total Requests:      {total_requests}")
    print(f"Successful Requests: {success_count}")
    print(f"Failed Requests:     {failure_count}")
    print(f"Total Time:          {total_time:.3f} s")
    print(f"Requests/sec:        {req_per_sec:.2f} req/s")
    print(f"Average Latency:     {avg_latency:.2f} ms")
    print(f"p50 Latency:         {p50:.2f} ms")
    print(f"p95 Latency:         {p95:.2f} ms")
    print(f"p99 Latency:         {p99:.2f} ms")
    print("=" * 55)

def main():
    if len(sys.argv) < 4:
        print("Usage: python3 benchmark/benchmark.py <url> <total_requests> <concurrency>")
        print("Example: python3 benchmark/benchmark.py http://localhost:8080/api/hello 1000 20")
        sys.exit(1)

    url = sys.argv[1]
    try:
        total_requests = int(sys.argv[2])
        concurrency = int(sys.argv[3])
    except ValueError:
        print("Error: total_requests and concurrency must be positive integers.")
        sys.exit(1)

    if total_requests <= 0 or concurrency <= 0:
        print("Error: total_requests and concurrency must be greater than 0.")
        sys.exit(1)

    run_benchmark(url, total_requests, concurrency)

if __name__ == '__main__':
    main()
