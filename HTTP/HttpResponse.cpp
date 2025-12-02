#include "HttpResponse.hpp"
#include <sstream>

HttpResponse::HttpResponse()
    : statusCode(200), statusMessage("OK")
{
    headers["Connection"] = "close";
}

void HttpResponse::setStatus(int code, const std::string &msg)
{
    statusCode = code;
    statusMessage = msg;
}

void HttpResponse::setHeader(const std::string &key, const std::string &value)
{
    headers[key] = value;
}

void HttpResponse::setBody(const std::string &b)
{
    body = b;
    std::ostringstream ss;
    ss << b.size();
    headers["Content-Length"] = ss.str();
}

std::string HttpResponse::serialize() const
{
    std::ostringstream out;

    out << "HTTP/1.1 " << statusCode << " " << statusMessage << "\r\n";     // Status line
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)     // Headers
        out << it->first << ": " << it->second << "\r\n";
    out << "\r\n";                                                          // Línea vacía obligatoria
    out << body;                                                            // Body

    return out.str();
}
