#include "BindSocket.hpp"

BindSocket::BindSocket(int domain, int service, int protocol, int port, u_long interface)
: SimpleSocket(domain, service, protocol, port, interface) 
{
    // Allow quick rebinding after restart and avoid "Address already in use"
    int opt = 1;
    if (setsockopt(getSocket(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR) failed");
    }
    int ret = NetworkConnection(getSocket(), getAddress());
    if (ret < 0)
    {
        perror("Failed to bind");
        exit(EXIT_FAILURE);
    }
    setConnection(ret);
}

BindSocket::BindSocket(const BindSocket &other)
: SimpleSocket(other) {}

BindSocket &BindSocket::operator=(const BindSocket &other)
{
    if (this != &other)
        SimpleSocket::operator=(other);
    return *this;
}

BindSocket::~BindSocket() {}


int BindSocket::NetworkConnection(int sock, struct sockaddr_in address)
{
    return bind(sock, (struct sockaddr *)&address, sizeof(address));
}
