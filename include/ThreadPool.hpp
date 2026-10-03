#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    // Initializes the pool with a fixed number of worker threads
    explicit ThreadPool(size_t numThreads = 8);

    // Destructor stops the pool and joins all threads
    ~ThreadPool();

    // Deleted copy semantics
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Enqueue a new task for workers to execute
    void enqueue(std::function<void()> task);

    // Stops worker threads and shuts down the pool cleanly
    void stop();

    // Statistics queries
    size_t getWorkerCount() const;
    size_t getActiveWorkers() const;
    size_t getQueueSize() const;

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> taskQueue;

    mutable std::mutex queueMutex;
    std::condition_variable cv;

    std::atomic<bool> stopping;
    std::atomic<size_t> activeWorkers;
};

#endif // THREAD_POOL_HPP
