#include "HttpServer.hpp"
#include "Logger.hpp"

#include <iostream>
#include <csignal>
#include <cstdlib>

namespace {
    HttpServer* g_server = nullptr;

    void signalHandler(int signum) {
        if (signum == SIGINT || signum == SIGTERM) {
            std::cout << "\n";
            Logger::info("Interrupt signal received. Initiating graceful shutdown...");
            if (g_server) {
                g_server->stop();
            }
        }
    }
}

int main(int argc, char* argv[]) {
    int port = 8080;
    size_t threads = 8;

    // Parse command line arguments: ./server [port] [threads]
    if (argc >= 2) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Usage: " << argv[0] << " [port] [threads]\n";
            return 1;
        }
    }

    if (argc >= 3) {
        try {
            threads = std::stoul(argv[2]);
            if (threads == 0) threads = 1;
        } catch (...) {
            std::cerr << "Usage: " << argv[0] << " [port] [threads]\n";
            return 1;
        }
    }

    // Register signal handlers for clean exit on Ctrl+C (SIGINT)
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    HttpServer server(port, threads, "public");
    g_server = &server;

    // Start accepting connections
    server.start();

    g_server = nullptr;
    return 0;
}
