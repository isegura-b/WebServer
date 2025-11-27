#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>

struct LocationBlock
{
    std::string path;                        // ex "/uploads"
    std::string root;                        // ex "/var/www/uploads"
    std::string index;                       // default index file
    bool autoindex;                          // directory listing
    std::vector<std::string> allowedMethods; // HTTP methods: GET, POST, DELETE
    std::string uploadStore;                 // storage path for uploads
    std::string redirect;                    // ex "301 /newpath"
    std::string cgiExtension;                // ex ".php" or ".py"

    LocationBlock() : autoindex(false) {}
};

struct ServerBlock
{
    std::string listenHost;                     // host/interface
    int listenPort;                             // port
    std::string serverName;                     // optional
    std::string root;                           // document root "/var/www/html"
    std::string index;                          // default index
    size_t clientMaxBodySize;                   // bytes
    std::map<int, std::string> errorPages;      // code -> path (HTML Error Pages)
    std::vector<LocationBlock> locations;       // location rules

    ServerBlock() : listenPort(0), clientMaxBodySize(0) {}
};

struct Config
{
    std::vector<ServerBlock> servers;
};

#endif
