#include "HttpRequest.hpp"

HttpRequest::HttpRequest()
    : headersComplete(false), contentLength(0)
{
}

bool HttpRequest::hasBody()
{
    return (headers.count("Content-Length") && contentLength > 0);
}
