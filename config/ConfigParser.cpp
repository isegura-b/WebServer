#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <cstdlib>

static bool starts_with(const std::string &s, const std::string &p) {
    return s.compare(0, p.size(), p) == 0;
}

std::string ConfigParser::trim(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && ::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && ::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    return s.substr(a, b - a);
}

Config ConfigParser::parse(const std::string &path) {
    std::ifstream in(path.c_str());
    if (!in)
        throw std::runtime_error("Cannot open config: " + path);

    Config cfg;
    std::string line;
    ServerBlock current;
    enum { OUTSIDE, IN_SERVER } state = OUTSIDE;

    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || starts_with(line, "#")) continue;

        if (state == OUTSIDE) {
            if (line == "server {") {
                current = ServerBlock();
                state = IN_SERVER;
            } else {
                throw std::runtime_error("Unexpected directive outside server block: " + line);
            }
        } else if (state == IN_SERVER) {
            if (line == "}") {
                if (current.listenPort == 0)
                    throw std::runtime_error("server block missing listen directive");
                cfg.servers.push_back(current);
                state = OUTSIDE;
                continue;
            }
            // handle simple directives: listen host:port; root ...; index ...;
            if (starts_with(line, "listen ")) {
                std::string v = trim(line.substr(7));
                if (!v.empty() && v[v.size()-1] == ';') v.erase(v.size()-1);
                size_t colon = v.find(':');
                if (colon == std::string::npos)
                    throw std::runtime_error("listen must be host:port, got: " + v);
                current.listenHost = v.substr(0, colon);
                std::string p = v.substr(colon+1);
                current.listenPort = std::atoi(p.c_str());
                if (current.listenPort <= 0 || current.listenPort >= 65536)
                    throw std::runtime_error("invalid listen port: " + p);
            } else if (starts_with(line, "root ")) {
                std::string v = trim(line.substr(5));
                if (!v.empty() && v[v.size()-1] == ';') v.erase(v.size()-1);
                current.root = v;
            } else if (starts_with(line, "index ")) {
                std::string v = trim(line.substr(6));
                if (!v.empty() && v[v.size()-1] == ';') v.erase(v.size()-1);
                current.index = v;
            } else {
                // For now, ignore unknown directives in MVP
                // Later: client_max_body_size, error_page, location, etc.
            }
        }
    }
    if (state != OUTSIDE)
        throw std::runtime_error("Unclosed server block");

    if (cfg.servers.empty())
        throw std::runtime_error("No servers defined in config");

    return cfg;
}
