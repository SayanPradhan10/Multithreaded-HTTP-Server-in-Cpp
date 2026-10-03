#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <unordered_map>

class HttpRequest {
public:
    HttpRequest() = default;

    // Parses raw HTTP request data
    bool parse(const std::string& rawRequest);

    // Getters for request components
    const std::string& getMethod() const;
    const std::string& getPath() const;
    const std::string& getVersion() const;
    const std::string& getBody() const;

    // Query parameter lookup
    bool hasQueryParam(const std::string& key) const;
    std::string getQueryParam(const std::string& key, const std::string& defaultValue = "") const;

    // Header lookup
    bool hasHeader(const std::string& key) const;
    std::string getHeader(const std::string& key, const std::string& defaultValue = "") const;

private:
    std::string method;
    std::string path;
    std::string version;
    std::string body;
    std::unordered_map<std::string, std::string> queryParams;
    std::unordered_map<std::string, std::string> headers;

    void parseQueryParams(const std::string& queryString);
};

#endif // HTTP_REQUEST_HPP
