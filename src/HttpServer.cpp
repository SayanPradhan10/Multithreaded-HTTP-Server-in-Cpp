#include "HttpServer.hpp"
#include "Logger.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>

HttpServer::HttpServer(
    int port,
    size_t threadCount,
    const std::string& publicDir
)
    : port(port),
      threadCount(threadCount),
      publicDir(publicDir),
      serverFd(-1),
      isRunning(false),
      threadPool(threadCount),
      totalRequests(0) {
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start() {

    // 1. Create socket
    serverFd = socket(AF_INET, SOCK_STREAM, 0);

    if (serverFd < 0) {
        Logger::error(
            "Failed to create socket: " +
            std::string(strerror(errno))
        );
        return;
    }

    // 2. Set socket options
    // Allows immediate re-binding after server restart
    int opt = 1;

    if (setsockopt(
            serverFd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)
        ) < 0) {

        Logger::warn(
            "Failed to set SO_REUSEADDR: " +
            std::string(strerror(errno))
        );
    }

    // 3. Bind socket to IP address and port
    sockaddr_in serverAddr{};

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(
            serverFd,
            reinterpret_cast<struct sockaddr*>(&serverAddr),
            sizeof(serverAddr)
        ) < 0) {

        Logger::error(
            "Failed to bind socket to port " +
            std::to_string(port) +
            ": " +
            std::string(strerror(errno))
        );

        close(serverFd);
        serverFd = -1;
        return;
    }

    // 4. Listen for incoming connections
    if (listen(serverFd, SOMAXCONN) < 0) {

        Logger::error(
            "Failed to listen on socket: " +
            std::string(strerror(errno))
        );

        close(serverFd);
        serverFd = -1;
        return;
    }

    isRunning = true;
    startTime = std::chrono::steady_clock::now();

    Logger::info(
        "Server started on port " +
        std::to_string(port)
    );

    Logger::info(
        "Worker pool started with " +
        std::to_string(threadCount) +
        " workers"
    );

    // 5. Main Accept Loop
    while (isRunning) {

        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        int clientFd = accept(
            serverFd,
            reinterpret_cast<struct sockaddr*>(&clientAddr),
            &clientLen
        );

        if (clientFd < 0) {

            if (!isRunning) {
                // Server shutdown initiated
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            Logger::error(
                "Failed to accept client connection: " +
                std::string(strerror(errno))
            );

            continue;
        }

        // IMPORTANT:
        // Do NOT increment totalRequests here.
        //
        // At this point we only know that a TCP connection
        // was accepted. We don't yet know which HTTP endpoint
        // the client requested.

        // Dispatch connection processing to the thread pool
        threadPool.enqueue([this, clientFd]() {
            this->handleClient(clientFd);
        });
    }
}

void HttpServer::stop() {

    bool expected = true;

    if (!isRunning.compare_exchange_strong(expected, false)) {
        return; // Already stopped or not started
    }

    Logger::info("Shutting down server...");

    // Unblock accept() call if waiting
    if (serverFd >= 0) {

        shutdown(serverFd, SHUT_RDWR);
        close(serverFd);

        serverFd = -1;
    }

    // Stop worker thread pool and join threads
    threadPool.stop();

    Logger::info("Server stopped cleanly.");
}

void HttpServer::handleClient(int clientFd) {

    char buffer[4096];

    ssize_t bytesRead = recv(
        clientFd,
        buffer,
        sizeof(buffer) - 1,
        0
    );

    if (bytesRead > 0) {

        buffer[bytesRead] = '\0';

        std::string rawData(buffer, bytesRead);

        HttpRequest request;

        if (request.parse(rawData)) {

            Logger::info(
                request.getMethod() +
                " " +
                request.getPath()
            );

            /*
             * Count only actual API requests.
             *
             * /api/stats is excluded because the dashboard
             * automatically calls it every 2 seconds.
             *
             * Static files such as:
             * /
             * /style.css
             * /script.js
             *
             * are also not counted.
             */
            const std::string& path = request.getPath();

            if (
                path.rfind("/api/", 0) == 0 &&
                path != "/api/stats"
            ) {
                ++totalRequests;
            }

            HttpResponse response = routeRequest(request);

            std::string responseString = response.toString();

            send(
                clientFd,
                responseString.c_str(),
                responseString.size(),
                0
            );

        } else {

            HttpResponse badReq =
                HttpResponse::badRequest(
                    "{\"error\":\"Bad Request\"}"
                );

            std::string responseString =
                badReq.toString();

            send(
                clientFd,
                responseString.c_str(),
                responseString.size(),
                0
            );
        }
    }

    close(clientFd);
}

HttpResponse HttpServer::routeRequest(
    const HttpRequest& request
) {

    const std::string& path = request.getPath();
    const std::string& method = request.getMethod();

    // Only GET is supported
    if (method != "GET") {

        return HttpResponse::badRequest(
            "{\"error\":\"Only GET method is supported\"}"
        );
    }

    // Static dashboard and assets
    if (path == "/" || path == "/index.html") {

        return handleStaticFile("index.html");

    } else if (path == "/style.css") {

        return handleStaticFile("style.css");

    } else if (path == "/script.js") {

        return handleStaticFile("script.js");
    }

    // API Routes
    if (path == "/api/hello") {

        return handleHello();

    } else if (path == "/api/stats") {

        return handleStats();

    } else if (path == "/api/compute") {

        return handleCompute(request);
    }

    // Unknown Route -> 404
    std::string notFoundJson =
        "{\n"
        "  \"error\": \"Not Found\",\n"
        "  \"path\": \"" +
        path +
        "\"\n"
        "}";

    return HttpResponse::notFound(notFoundJson);
}

HttpResponse HttpServer::handleStaticFile(
    const std::string& fileName
) {

    // Basic path security check
    // Prevent directory traversal
    if (fileName.find("..") != std::string::npos) {

        return HttpResponse::notFound(
            "{\"error\":\"Access Denied\"}"
        );
    }

    std::string fullPath =
        publicDir + "/" + fileName;

    std::ifstream file(
        fullPath,
        std::ios::in | std::ios::binary
    );

    if (!file.is_open()) {

        return HttpResponse::notFound(
            "{\"error\":\"File Not Found\"}"
        );
    }

    std::ostringstream ss;

    ss << file.rdbuf();

    std::string content = ss.str();

    std::string mimeType =
        getMimeType(fileName);

    return HttpResponse::file(
        mimeType,
        content
    );
}

HttpResponse HttpServer::handleHello() {

    std::string jsonBody =
        "{\n"
        "  \"message\": \"Hello from C++ HTTP Server\"\n"
        "}";

    return HttpResponse::json(
        200,
        jsonBody
    );
}

HttpResponse HttpServer::handleStats() {

    auto now =
        std::chrono::steady_clock::now();

    auto uptimeSeconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            now - startTime
        ).count();

    std::ostringstream json;

    json
        << "{\n"
        << "  \"total_requests\": "
        << totalRequests.load()
        << ",\n"

        << "  \"worker_threads\": "
        << threadPool.getWorkerCount()
        << ",\n"

        << "  \"active_workers\": "
        << threadPool.getActiveWorkers()
        << ",\n"

        << "  \"queued_tasks\": "
        << threadPool.getQueueSize()
        << ",\n"

        << "  \"uptime_seconds\": "
        << uptimeSeconds
        << "\n"

        << "}";

    return HttpResponse::json(
        200,
        json.str()
    );
}

HttpResponse HttpServer::handleCompute(
    const HttpRequest& request
) {

    int n = 30; // default value

    if (request.hasQueryParam("n")) {

        try {

            n = std::stoi(
                request.getQueryParam("n")
            );

        } catch (...) {

            return HttpResponse::badRequest(
                "{\"error\":\"Invalid parameter 'n'. Must be an integer.\"}"
            );
        }
    }

    if (n < 0 || n > 90) {

        return HttpResponse::badRequest(
            "{\"error\":\"Parameter 'n' must be between 0 and 90.\"}"
        );
    }

    uint64_t result =
        computeFibonacci(n);

    std::ostringstream json;

    json
        << "{\n"
        << "  \"input\": "
        << n
        << ",\n"

        << "  \"result\": "
        << result
        << "\n"

        << "}";

    return HttpResponse::json(
        200,
        json.str()
    );
}

uint64_t HttpServer::computeFibonacci(int n) {

    if (n <= 0) {
        return 0;
    }

    if (n == 1) {
        return 1;
    }

    uint64_t a = 0;
    uint64_t b = 1;

    for (int i = 2; i <= n; ++i) {

        uint64_t c = a + b;

        a = b;
        b = c;
    }

    return b;
}

std::string HttpServer::getMimeType(
    const std::string& path
) {

    size_t dotPos =
        path.rfind('.');

    if (dotPos == std::string::npos) {
        return "text/plain";
    }

    std::string ext =
        path.substr(dotPos);

    if (ext == ".html") {
        return "text/html; charset=utf-8";
    }

    if (ext == ".css") {
        return "text/css; charset=utf-8";
    }

    if (ext == ".js") {
        return "application/javascript; charset=utf-8";
    }

    if (ext == ".json") {
        return "application/json";
    }

    if (ext == ".png") {
        return "image/png";
    }

    if (ext == ".ico") {
        return "image/x-icon";
    }

    return "text/plain";
}