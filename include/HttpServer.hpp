#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <string>
#include <atomic>
#include <chrono>
#include <memory>

#include "ThreadPool.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

class HttpServer {
public:

    HttpServer(
        int port = 8080,
        size_t threadCount = 8,
        const std::string& publicDir = "public"
    );

    ~HttpServer();

    // Start accepting client connections.
    // This function runs the main blocking accept loop.
    void start();

    // Stop the server and release resources gracefully.
    void stop();

    // Process one client connection.
    void handleClient(int clientFd);

private:

    // Server configuration
    int port;
    size_t threadCount;
    std::string publicDir;

    // Listening socket
    int serverFd;

    // Indicates whether the server is currently running.
    std::atomic<bool> isRunning;

    // Fixed-size worker thread pool.
    ThreadPool threadPool;

    // Server statistics
    //
    // Counts API requests handled by the server,
    // excluding /api/stats so dashboard auto-refresh
    // does not continuously increase the counter.
    std::atomic<uint64_t> totalRequests;

    // Time when the server started.
    std::chrono::steady_clock::time_point startTime;

    // Request routing
    HttpResponse routeRequest(const HttpRequest& request);

    // Static file handling
    HttpResponse handleStaticFile(const std::string& path);

    // API handlers
    HttpResponse handleHello();

    HttpResponse handleStats();

    HttpResponse handleCompute(
        const HttpRequest& request
    );

    // Utility functions
    static uint64_t computeFibonacci(int n);

    static std::string getMimeType(
        const std::string& path
    );
};

#endif // HTTP_SERVER_HPP