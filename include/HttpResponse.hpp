#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <unordered_map>

class HttpResponse {
public:
    HttpResponse();
    explicit HttpResponse(int statusCode, const std::string& statusMessage = "");

    // Setters
    void setStatusCode(int code, const std::string& message = "");
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& bodyContent, const std::string& contentType = "text/plain");

    // Getters
    int getStatusCode() const;
    const std::string& getStatusMessage() const;
    const std::string& getBody() const;

    // Serialize to HTTP/1.1 response wire format
    std::string toString() const;

    // Convenient factory helpers
    static HttpResponse ok();
    static HttpResponse json(int statusCode, const std::string& jsonString);
    static HttpResponse html(int statusCode, const std::string& htmlString);
    static HttpResponse file(const std::string& contentType, const std::string& content);
    static HttpResponse badRequest(const std::string& message = "{\"error\":\"Bad Request\"}");
    static HttpResponse notFound(const std::string& message = "{\"error\":\"Not Found\"}");
    static HttpResponse internalServerError(const std::string& message = "{\"error\":\"Internal Server Error\"}");

private:
    int statusCode;
    std::string statusMessage;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    static std::string getDefaultStatusMessage(int code);
};

#endif // HTTP_RESPONSE_HPP
