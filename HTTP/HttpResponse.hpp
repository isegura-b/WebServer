#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>

class HttpResponse
{
public:
    int statusCode;
    std::string statusMessage;
    std::map<std::string, std::string> headers;
    std::string body;

    HttpResponse();

    void setStatus(int code, const std::string &msg);
    void setHeader(const std::string &key, const std::string &value);
    void setBody(const std::string &b);

    std::string serialize() const;
};

#endif
