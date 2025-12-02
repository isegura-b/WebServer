#ifndef HTTPPARSER_HPP
#define HTTPPARSER_HPP

#include "HttpRequest.hpp"
#include <string>

class HttpParser
{
public:
    static bool parse(HttpRequest &req, const std::string &raw);
};

#endif
