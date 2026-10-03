#include "HttpResponse.hpp"
#include <sstream>

HttpResponse::HttpResponse()
    : statusCode(200), statusMessage("OK") {
    headers["Connection"] = "close";
}

HttpResponse::HttpResponse(int statusCode, const std::string& statusMessage)
    : statusCode(statusCode) {
    if (statusMessage.empty()) {
        this->statusMessage = getDefaultStatusMessage(statusCode);
    } else {
        this->statusMessage = statusMessage;
    }
    headers["Connection"] = "close";
}

void HttpResponse::setStatusCode(int code, const std::string& message) {
    statusCode = code;
    if (message.empty()) {
        statusMessage = getDefaultStatusMessage(code);
    } else {
        statusMessage = message;
    }
}

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
    headers[key] = value;
}

void HttpResponse::setBody(const std::string& bodyContent, const std::string& contentType) {
    body = bodyContent;
    headers["Content-Type"] = contentType;
    headers["Content-Length"] = std::to_string(body.size());
}

int HttpResponse::getStatusCode() const {
    return statusCode;
}

const std::string& HttpResponse::getStatusMessage() const {
    return statusMessage;
}

const std::string& HttpResponse::getBody() const {
    return body;
}

std::string HttpResponse::toString() const {
    std::ostringstream response;
    // Status line: HTTP/1.1 200 OK\r\n
    response << "HTTP/1.1 " << statusCode << " " << statusMessage << "\r\n";

    // Headers
    for (const auto& [key, value] : headers) {
        response << key << ": " << value << "\r\n";
    }

    // End of header section
    response << "\r\n";

    // Body
    response << body;

    return response.str();
}

std::string HttpResponse::getDefaultStatusMessage(int code) {
    switch (code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        default:  return "Unknown Status";
    }
}

HttpResponse HttpResponse::ok() {
    return HttpResponse(200, "OK");
}

HttpResponse HttpResponse::json(int statusCode, const std::string& jsonString) {
    HttpResponse res(statusCode);
    res.setBody(jsonString, "application/json");
    return res;
}

HttpResponse HttpResponse::html(int statusCode, const std::string& htmlString) {
    HttpResponse res(statusCode);
    res.setBody(htmlString, "text/html; charset=utf-8");
    return res;
}

HttpResponse HttpResponse::file(const std::string& contentType, const std::string& content) {
    HttpResponse res(200, "OK");
    res.setBody(content, contentType);
    return res;
}

HttpResponse HttpResponse::badRequest(const std::string& message) {
    return HttpResponse::json(400, message);
}

HttpResponse HttpResponse::notFound(const std::string& message) {
    return HttpResponse::json(404, message);
}

HttpResponse HttpResponse::internalServerError(const std::string& message) {
    return HttpResponse::json(500, message);
}
