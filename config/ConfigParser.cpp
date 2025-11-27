#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <cstdlib>
#include <vector>

static bool starts_with(const std::string &s, const std::string &p)
{
    return s.compare(0, p.size(), p) == 0;
}

std::string ConfigParser::trim(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && ::isspace(static_cast<unsigned char>(s[a])))
        ++a;
    while (b > a && ::isspace(static_cast<unsigned char>(s[b - 1])))
        --b;
    return s.substr(a, b - a);
}

static std::vector<std::string> split_ws(const std::string &s)
{
    std::vector<std::string> out;
    std::string cur;
    for (std::string::const_iterator it = s.begin(); it != s.end(); ++it)
    {
        unsigned char ch = static_cast<unsigned char>(*it);
        if (::isspace(ch))
        {
            if (!cur.empty())
            {
                out.push_back(cur);
                cur.clear();
            }
        }
        else
        {
            cur.push_back(*it);
        }
    }
    if (!cur.empty())
        out.push_back(cur);
    return out;
}

/*
    suffixes for size
    K -> 1024
    M -> 1024*1024
    G -> 1024*1024*1024
*/
static size_t parse_size_with_suffix(const std::string &v)
{
    if (v.empty())
        return 0;
    char last = v[v.size() - 1];
    size_t mult = 1;
    std::string num = v;
    if (last == 'K' || last == 'k')
    {
        mult = 1024;
        num = v.substr(0, v.size() - 1);
    }
    else if (last == 'M' || last == 'm')
    {
        mult = 1024ULL * 1024ULL;
        num = v.substr(0, v.size() - 1);
    }
    else if (last == 'G' || last == 'g')
    {
        mult = 1024ULL * 1024ULL * 1024ULL;
        num = v.substr(0, v.size() - 1);
    }
    long base = std::atol(num.c_str());
    if (base < 0)
        base = 0;
    return static_cast<size_t>(base) * mult;
}

Config ConfigParser::parse(const std::string &path)
{
    std::ifstream in(path.c_str()); // open file
    if (!in)
        throw std::runtime_error("Cannot open config: " + path);

    Config cfg;
    std::string line;
    ServerBlock current;
    enum State
    {
        OUTSIDE,
        IN_SERVER,
        IN_LOCATION
    } state = OUTSIDE;
    LocationBlock currentLoc;

    while (std::getline(in, line))
    {
        line = trim(line);
        if (line.empty() || starts_with(line, "#"))
            continue;

        if (state == OUTSIDE)
        {
            if (line == "server {")
            {
                current = ServerBlock();
                state = IN_SERVER;
            }
            else
            {
                throw std::runtime_error("Unexpected directive outside server block: " + line);
            }
        }
        else if (state == IN_SERVER)
        {
            if (line == "}")
            {
                if (current.listenPort == 0)
                    throw std::runtime_error("server block missing listen directive"); // exception
                cfg.servers.push_back(current);
                state = OUTSIDE;
                continue;
            }
            if (starts_with(line, "location "))
            {
                std::string rest = trim(line.substr(9));
                if (!rest.empty() && rest[rest.size() - 1] == '{')
                {
                    rest.erase(rest.size() - 1);
                    rest = trim(rest);
                }
                else if (rest == "{")
                {
                    rest.clear();
                }
                if (rest.empty())
                    throw std::runtime_error("location requires a path");
                currentLoc = LocationBlock();
                currentLoc.path = rest;
                state = IN_LOCATION;
                continue;
            }

            // Parse server directives (inside {})
            if (starts_with(line, "listen "))
            {
                std::string s = trim(line.substr(7));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                size_t colon = s.find(':');
                if (colon == std::string::npos)
                {
                    current.listenHost = "0.0.0.0";
                    std::string p = s;
                    current.listenPort = std::atoi(p.c_str());
                    if (current.listenPort <= 0 || current.listenPort >= 65536)
                        throw std::runtime_error("invalid listen port: " + p);
                    continue;
                }
                current.listenHost = s.substr(0, colon);
                std::string p = s.substr(colon + 1);
                current.listenPort = std::atoi(p.c_str());
                if (current.listenPort <= 0 || current.listenPort >= 65536)
                    throw std::runtime_error("invalid listen port: " + p);
            }
            else if (starts_with(line, "root "))
            {
                std::string s = trim(line.substr(5));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                current.root = s;
            }
            else if (starts_with(line, "index "))
            {
                std::string s = trim(line.substr(6));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                current.index = s;
            }
            else if (starts_with(line, "server_name "))
            {
                std::string s = trim(line.substr(12));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                current.serverName = s;
            }
            else if (starts_with(line, "client_max_body_size "))
            {
                std::string s = trim(line.substr(22));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                current.clientMaxBodySize = parse_size_with_suffix(s);
            }
            else if (starts_with(line, "error_page "))
            {
                std::string s = trim(line.substr(11));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                std::vector<std::string> toks = split_ws(s);
                if (toks.size() >= 2)
                {
                    int code = std::atoi(toks[0].c_str());
                    std::string pathv = toks[1];
                    current.errorPages[code] = pathv;
                }
            }
            else
            {
            }
        }
        else if (state == IN_LOCATION)
        {
            if (line == "}")
            {
                if (currentLoc.path.empty())
                    throw std::runtime_error("location without path");
                current.locations.push_back(currentLoc);
                state = IN_SERVER;
                continue;
            }
            if (starts_with(line, "root "))
            {
                std::string s = trim(line.substr(5));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                currentLoc.root = s;
            }
            else if (starts_with(line, "index "))
            {
                std::string s = trim(line.substr(6));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                currentLoc.index = s;
            }
            else if (starts_with(line, "autoindex "))
            {
                std::string s = trim(line.substr(10));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                currentLoc.autoindex = (s == "on" || s == "ON" || s == "1");
            }
            else if (starts_with(line, "methods "))
            {
                std::string s = trim(line.substr(8));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                currentLoc.allowedMethods = split_ws(s);
            }
            else if (starts_with(line, "upload_store "))
            {
                std::string s = trim(line.substr(13));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                currentLoc.uploadStore = s;
            }
            else if (starts_with(line, "return "))
            {
                std::string s = trim(line.substr(7));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                currentLoc.redirect = s;
            }
            else if (starts_with(line, "cgi_extension "))
            {
                std::string s = trim(line.substr(14));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                currentLoc.cgiExtension = s;
            }
            else
            {
            }
        }
    }
    if (state != OUTSIDE)
        throw std::runtime_error("Unclosed server block");

    if (cfg.servers.empty())
        throw std::runtime_error("No servers defined in config");

    return cfg;
}
