#include "Server.hpp"
#include <vector>
#include <cstdlib>
#include <string>
#include <iostream>
#include "../config/ConfigParser.hpp"

int main(int ac, char** av)
{
    std::vector<int> ports;

    if (ac > 2) {
        std::cerr << "Usage: " << av[0] << " [config_file]" << std::endl;
        return 1;
    }
    
    std::string confPath;
    if (ac >= 2)
        confPath = av[1];
    else
        confPath = "config/example.conf";

    try {
        ConfigParser parser;
        Config cfg = parser.parse(confPath);
        for (std::size_t i = 0; i < cfg.servers.size(); ++i) {
            if (cfg.servers[i].listenPort > 0)
                ports.push_back(cfg.servers[i].listenPort);
        }
        if (ports.empty()) {
            std::cerr << "Config parsed but no listen ports defined." << std::endl;
            return 1;
        }
    } catch (const std::exception &e) {
        std::cerr << "Config error: " << e.what() << "\n";
        return 1;
    }

    Server server(ports);    // multi-port
    server.launch();
    return 0;
}