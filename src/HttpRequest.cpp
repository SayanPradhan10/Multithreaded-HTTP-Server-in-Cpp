#include "HttpRequest.hpp"
#include <sstream>
#include <algorithm>

namespace {
    // Helper to trim leading and trailing spaces/tabs/newlines
    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
}

bool HttpRequest::parse(const std::string& rawRequest) {
    if (rawRequest.empty()) {
        return false;
    }

    std::istringstream stream(rawRequest);
    std::string line;

    // 1. Parse the Request-Line: METHOD URI HTTP_VERSION
    // Example: GET /api/compute?n=30 HTTP/1.1
    if (!std::getline(stream, line)) {
        return false;
    }

    std::istringstream requestLineStream(line);
    std::string rawUri;
    if (!(requestLineStream >> method >> rawUri >> version)) {
        return false;
    }

    // 2. Parse URI into path and optional query parameters
    size_t queryPos = rawUri.find('?');
    if (queryPos != std::string::npos) {
        path = rawUri.substr(0, queryPos);
        parseQueryParams(rawUri.substr(queryPos + 1));
    } else {
        path = rawUri;
    }

    // 3. Parse Headers until an empty line is encountered
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty()) {
            break; // Header section ends with an empty line
        }

        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string headerName = trim(line.substr(0, colonPos));
            std::string headerValue = trim(line.substr(colonPos + 1));
            // Store header
            headers[headerName] = headerValue;
        }
    }

    // 4. Capture remainder as body (if present)
    std::string bodyLine;
    bool first = true;
    while (std::getline(stream, bodyLine)) {
        if (!first) {
            body += "\n";
        }
        body += bodyLine;
        first = false;
    }

    return true;
}

void HttpRequest::parseQueryParams(const std::string& queryString) {
    std::istringstream qsStream(queryString);
    std::string pair;

    // Split query string by '&'
    while (std::getline(qsStream, pair, '&')) {
        if (pair.empty()) continue;

        size_t eqPos = pair.find('=');
        if (eqPos != std::string::npos) {
            std::string key = pair.substr(0, eqPos);
            std::string val = pair.substr(eqPos + 1);
            queryParams[key] = val;
        } else {
            queryParams[pair] = "";
        }
    }
}

const std::string& HttpRequest::getMethod() const {
    return method;
}

const std::string& HttpRequest::getPath() const {
    return path;
}

const std::string& HttpRequest::getVersion() const {
    return version;
}

const std::string& HttpRequest::getBody() const {
    return body;
}

bool HttpRequest::hasQueryParam(const std::string& key) const {
    return queryParams.find(key) != queryParams.end();
}

std::string HttpRequest::getQueryParam(const std::string& key, const std::string& defaultValue) const {
    auto it = queryParams.find(key);
    if (it != queryParams.end()) {
        return it->second;
    }
    return defaultValue;
}

bool HttpRequest::hasHeader(const std::string& key) const {
    return headers.find(key) != headers.end();
}

std::string HttpRequest::getHeader(const std::string& key, const std::string& defaultValue) const {
    auto it = headers.find(key);
    if (it != headers.end()) {
        return it->second;
    }
    return defaultValue;
}
