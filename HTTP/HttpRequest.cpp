#include "HttpRequest.hpp"

HttpRequest::HttpRequest()
    : headersComplete(false), contentLength(0)
{
}

bool HttpRequest::hasBody()
{
    return contentLength > 0;
}