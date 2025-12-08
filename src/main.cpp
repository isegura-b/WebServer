#include "Server.hpp"
#include <vector>
#include <cstdlib>
#include <string>
#include <iostream>
#include "../config/ConfigParser.hpp"

int main(int ac, char **av)
{
    std::vector<int> ports;
    Config cfg;

    if (ac > 2)
    {
        std::cerr << "Usage: " << av[0] << " [config_file]" << std::endl;
        return 1;
    }

    std::string confPath;
    if (ac >= 2)
        confPath = av[1];
    else
        confPath = "config/example.conf";

    try
    {
        ConfigParser parser;
        cfg = parser.parse(confPath);

        // std::cout << "[config] servers: " << cfg.servers.size() << std::endl; //n server count
        for (std::size_t i = 0; i < cfg.servers.size(); ++i)
        {
            /* std::cout << "test parse config" << std::endl;
                        const ServerBlock &sb = cfg.servers[i];
                        std::cout << "[config] server #" << i
                                  << " listen=" << (sb.listenHost.empty() ? std::string("0.0.0.0") : sb.listenHost)
                                  << ":" << sb.listenPort
                                  << " root=" << sb.root
                                  << " index=" << sb.index
                                  << " server_name=" << sb.serverName
                                  << " client_max_body_size=" << sb.clientMaxBodySize
                                  << std::endl;

                        if (!sb.errorPages.empty()) {
                            std::cout << "[config]  error_pages:" << std::endl;
                            std::map<int, std::string>::const_iterator it = sb.errorPages.begin();
                            for (; it != sb.errorPages.end(); ++it) {
                                std::cout << "[config]   " << it->first << " => " << it->second << std::endl;
                            }
                        }
                        if (!sb.locations.empty()) {
                            std::cout << "[config]  locations: " << sb.locations.size() << std::endl;
                            for (std::size_t li = 0; li < sb.locations.size(); ++li) {
                                const LocationBlock &lb = sb.locations[li];
                                std::cout << "[config]   location '" << lb.path << "'"
                                          << " root=" << lb.root
                                          << " index=" << lb.index
                                          << " autoindex=" << (lb.autoindex ? "on" : "off")
                                          << std::endl;
                                if (!lb.allowedMethods.empty()) {
                                    std::cout << "[config]    methods:";
                                    for (std::size_t mi = 0; mi < lb.allowedMethods.size(); ++mi) {
                                        std::cout << " " << lb.allowedMethods[mi];
                                    }
                                    std::cout << std::endl;
                                }
                                if (!lb.uploadStore.empty())
                                    std::cout << "[config]    upload_store: " << lb.uploadStore << std::endl;
                                if (!lb.redirect.empty())
                                    std::cout << "[config]    return: " << lb.redirect << std::endl;
                                if (!lb.cgiExtension.empty())
                                    std::cout << "[config]    cgi_extension: " << lb.cgiExtension << std::endl;
                            }
                        }
            */
            // Collect listen ports for server startup
            if (cfg.servers[i].listenPort > 0)
                ports.push_back(cfg.servers[i].listenPort);
        }
        if (ports.empty())
        {
            std::cerr << "Config parsed but no listen ports defined." << std::endl;
            return 1;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Config error: " << e.what() << "\n";
        return 1;
    }

    Server server(ports, cfg); // multi-port
    server.launch();
    return 0;
}