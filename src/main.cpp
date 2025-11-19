#include "Server.hpp"
#include <vector>
#include <cstdlib>

int main(int argc, char** argv)
{
    std::vector<int> ports;
    for (int i = 1; i < argc; ++i)
    {
        int p = std::atoi(argv[i]);
        if (p > 0 && p < 65536)
            ports.push_back(p);
    }

    if (ports.empty())
    {
        Server server;           // default 8080
        server.launch();
    }
    else ///webserv 8080 8081 9090
    {
        Server server(ports);    // multi-port
        server.launch();
    }
    return 0;
}