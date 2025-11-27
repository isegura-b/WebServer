#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>

struct LocationBlock {
    std::string path;           // e.g. "/uploads"
    std::string root;           // override root
    std::string index;          // default index file
    bool        autoindex;      // directory listing
    std::vector<std::string> allowedMethods; // GET, POST, DELETE
    std::string uploadStore;    // storage path for uploads
    std::string redirect;       // e.g. "301 /new"
    std::string cgiExtension;   // e.g. ".php" or ".py"

    LocationBlock() : autoindex(false) {}
};

struct ServerBlock {
    std::string listenHost;     // host/interface
    int         listenPort;     // port
    std::string serverName;     // optional
    std::string root;           // document root
    std::string index;          // default index
    size_t      clientMaxBodySize; // bytes
    std::vector<std::string> defaultErrorPages; // paths by code
    std::vector<LocationBlock> locations;       // location rules

    ServerBlock() : listenPort(0), clientMaxBodySize(0) {}
};

struct Config {
    std::vector<ServerBlock> servers;
};

#endif // CONFIG_HPP
