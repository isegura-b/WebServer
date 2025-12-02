#include "HttpParser.hpp"
#include <sstream>
#include <cstdlib>

static std::string trim(const std::string &s)
{
    size_t b = s.find_first_not_of(" \t\r\n");
    size_t e = s.find_last_not_of(" \t\r\n");
    if (b == std::string::npos)
        return "";
    return s.substr(b, e - b + 1);
}

bool HttpParser::parse(HttpRequest &req, const std::string &raw)
{
    size_t pos = raw.find("\r\n\r\n");
    if (pos == std::string::npos)
        return false;

    req.headersComplete = true;
    std::string head = raw.substr(0, pos);

    std::istringstream iss(head);
    std::string line;
    bool first = true;

    while (std::getline(iss, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (first)
        {
            first = false;
            std::istringstream sl(line);
            sl >> req.method >> req.path >> req.version;
            continue;
        }

        size_t p = line.find(':');
        if (p == std::string::npos)
            continue;

        std::string key = trim(line.substr(0, p));
        std::string value = trim(line.substr(p + 1));

        req.headers[key] = value;
    }

    if (req.headers.count("Content-Length"))
        req.contentLength = std::atoi(req.headers["Content-Length"].c_str());

    if (raw.size() > pos + 4)
        req.body = raw.substr(pos + 4);

    return true;
}
