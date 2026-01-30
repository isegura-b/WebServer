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

class ServerDirectiveFlags
{
public:
    bool listenSet;
    bool rootSet;
    bool indexSet;
    bool serverNameSet;
    bool maxBodySet;
    ServerDirectiveFlags() : listenSet(false), rootSet(false), indexSet(false), serverNameSet(false), maxBodySet(false) {}
    void reset() { listenSet = rootSet = indexSet = serverNameSet = maxBodySet = false; }
};

class LocationDirectiveFlags
{
public:
    bool rootSet;
    bool indexSet;
    bool autoindexSet;
    bool methodsSet;
    bool uploadStoreSet;
    bool redirectSet;
    LocationDirectiveFlags() : rootSet(false), indexSet(false), autoindexSet(false), methodsSet(false), uploadStoreSet(false), redirectSet(false) {}
    void reset() { rootSet = indexSet = autoindexSet = methodsSet = uploadStoreSet = redirectSet = false; }
};

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
    ServerDirectiveFlags sFlags;
    LocationDirectiveFlags lFlags;

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
                sFlags.reset();
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
                lFlags.reset();
                continue;
            }

            // Parse server directives (inside {})
            if (starts_with(line, "listen "))
            {
                std::string s = trim(line.substr(7));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                if (sFlags.listenSet)
                    throw std::runtime_error("duplicate directive 'listen' in server block");
                size_t colonPos = s.find(':');
                if (colonPos == std::string::npos)
                {
                    current.listenHost = "0.0.0.0";
                    std::string p = s;
                    current.listenPort = std::atoi(p.c_str());
                    if (current.listenPort <= 0 || current.listenPort >= 65536)
                        throw std::runtime_error("invalid listen port: " + p);
                    sFlags.listenSet = true;
                    continue;
                }
                current.listenHost = s.substr(0, colonPos);
                std::string p = s.substr(colonPos + 1);
                current.listenPort = std::atoi(p.c_str());
                if (current.listenPort <= 0 || current.listenPort >= 65536)
                    throw std::runtime_error("invalid listen port: " + p);
                sFlags.listenSet = true;
            }
            else if (starts_with(line, "root "))
            {
                std::string s = trim(line.substr(5));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                if (sFlags.rootSet)
                    throw std::runtime_error("duplicate directive 'root' in server block");
                current.root = s;
                sFlags.rootSet = true;
            }
            else if (starts_with(line, "index "))
            {
                std::string s = trim(line.substr(6));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                if (sFlags.indexSet)
                    throw std::runtime_error("duplicate directive 'index' in server block");
                current.index = s;
                sFlags.indexSet = true;
            }
            else if (starts_with(line, "server_name "))
            {
                std::string s = trim(line.substr(12));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                if (sFlags.serverNameSet)
                    throw std::runtime_error("duplicate directive 'server_name' in server block");
                current.serverName = s;
                sFlags.serverNameSet = true;
            }
            else if (starts_with(line, "client_max_body_size "))
            {
                std::string s = trim(line.substr(21));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                if (sFlags.maxBodySet)
                    throw std::runtime_error("duplicate directive 'client_max_body_size' in server block");
                current.clientMaxBodySize = parse_size_with_suffix(s);
                sFlags.maxBodySet = true;
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
                    if (current.errorPages.count(code))
                        throw std::runtime_error("duplicate error_page for code " + std::string(toks[0]));
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
                for (size_t i = 0; i < current.locations.size(); ++i)
                {
                    if (current.locations[i].path == currentLoc.path)
                        throw std::runtime_error("duplicate location path: " + currentLoc.path);
                }
                current.locations.push_back(currentLoc);
                state = IN_SERVER;
                continue;
            }
            if (starts_with(line, "root "))
            {
                std::string s = trim(line.substr(5));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                if (lFlags.rootSet)
                    throw std::runtime_error("duplicate directive 'root' in location block");
                currentLoc.root = s;
                lFlags.rootSet = true;
            }
            else if (starts_with(line, "index "))
            {
                std::string s = trim(line.substr(6));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                if (lFlags.indexSet)
                    throw std::runtime_error("duplicate directive 'index' in location block");
                currentLoc.index = s;
                lFlags.indexSet = true;
            }
            else if (starts_with(line, "autoindex "))
            {
                std::string s = trim(line.substr(10));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                if (lFlags.autoindexSet)
                    throw std::runtime_error("duplicate directive 'autoindex' in location block");
                currentLoc.autoindex = (s == "on" || s == "ON" || s == "1");
                lFlags.autoindexSet = true;
            }
            else if (starts_with(line, "methods "))
            {
                std::string s = trim(line.substr(8));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                if (lFlags.methodsSet)
                    throw std::runtime_error("duplicate directive 'methods' in location block");
                currentLoc.allowedMethods = split_ws(s);
                lFlags.methodsSet = true;
            }
            else if (starts_with(line, "upload_store "))
            {
                std::string s = trim(line.substr(13));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                if (lFlags.uploadStoreSet)
                    throw std::runtime_error("duplicate directive 'upload_store' in location block");
                currentLoc.uploadStore = s;
                lFlags.uploadStoreSet = true;
            }
            else if (starts_with(line, "return "))
            {
                std::string s = trim(line.substr(7));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                if (lFlags.redirectSet)
                    throw std::runtime_error("duplicate directive 'return' in location block");
                currentLoc.redirect = s;
                lFlags.redirectSet = true;
            }
            else if (starts_with(line, "cgi_extension "))
            {
                std::string s = trim(line.substr(14));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                std::vector<std::string> toks = split_ws(s);
                if (toks.size() >= 2)
                {
                    if (currentLoc.cgiPass.count(toks[0]))
                        throw std::runtime_error("duplicate cgi mapping for extension " + toks[0]);
                    currentLoc.cgiPass[toks[0]] = toks[1];
                }
                else
                {
                    throw std::runtime_error("cgi_extension requires: <ext> <interpreter>");
                }
            }
            else if (starts_with(line, "cgi_pass "))
            {
                std::string s = trim(line.substr(9));
                if (!s.empty() && s[s.size() - 1] == ';')
                    s.erase(s.size() - 1);
                std::vector<std::string> toks = split_ws(s);
                if (toks.size() >= 2)
                {
                    if (currentLoc.cgiPass.count(toks[0]))
                        throw std::runtime_error("duplicate cgi mapping for extension " + toks[0]);
                    currentLoc.cgiPass[toks[0]] = toks[1];
                }
                else
                {
                    throw std::runtime_error("cgi_pass requires: <ext> <interpreter>");
                }
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
