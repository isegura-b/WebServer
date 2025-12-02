#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>


// Struct of what the clients sends before being processed
class HttpRequest
{
public:
    std::string method;
    std::string path;
    std::string version;

    std::map<std::string, std::string> headers;
    std::string body;

    bool headersComplete;
    size_t contentLength;

    HttpRequest();
    bool hasBody();
};

#endif
