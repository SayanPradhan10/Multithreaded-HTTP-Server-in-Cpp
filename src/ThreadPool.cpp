#include "ThreadPool.hpp"
#include <iostream>

ThreadPool::ThreadPool(size_t numThreads)
    : stopping(false), activeWorkers(0) {
    // Spawn worker threads
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this]() {
            while (true) {
                std::function<void()> task;

                {
                    // Acquire lock to inspect and pop from task queue
                    std::unique_lock<std::mutex> lock(this->queueMutex);

                    // Wait until tasks are available or stop has been requested
                    this->cv.wait(lock, [this]() {
                        return this->stopping || !this->taskQueue.empty();
                    });

                    // If stopping and no remaining tasks, terminate thread
                    if (this->stopping && this->taskQueue.empty()) {
                        return;
                    }

                    // Retrieve the next task from the FIFO queue
                    task = std::move(this->taskQueue.front());
                    this->taskQueue.pop();
                }

                // Execute the task outside the lock
                if (task) {
                    ++activeWorkers;
                    try {
                        task();
                    } catch (...) {
                        // Catch exceptions to prevent worker thread termination
                    }
                    --activeWorkers;
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stopping) {
            return;
        }
        taskQueue.push(std::move(task));
    }
    // Wake up one available worker thread
    cv.notify_one();
}

void ThreadPool::stop() {
    bool expected = false;
    if (!stopping.compare_exchange_strong(expected, true)) {
        return; // Already stopped
    }

    // Wake up all worker threads so they can exit their loop
    cv.notify_all();

    // Join all worker threads
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers.clear();
}

size_t ThreadPool::getWorkerCount() const {
    return workers.size();
}

size_t ThreadPool::getActiveWorkers() const {
    return activeWorkers.load();
}

size_t ThreadPool::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return taskQueue.size();
}
