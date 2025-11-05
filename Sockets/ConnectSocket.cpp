#include "ConnectSocket.hpp"

ConnectSocket::ConnectSocket(int domain, int service, int protocol, int port, u_long interface)
: SimpleSocket(domain, service, protocol, port, interface) 
{
    int ret = NetworkConnection(getSocket(), getAddress());
    if (ret < 0)
    {
        perror("Failed to connect");
        exit(EXIT_FAILURE);
    }
    setConnection(ret);
}

ConnectSocket::ConnectSocket(const ConnectSocket &other)
: SimpleSocket(other) {}

ConnectSocket &ConnectSocket::operator=(const ConnectSocket &other)
{
    if (this != &other)
        SimpleSocket::operator=(other);
    return *this;
}

ConnectSocket::~ConnectSocket() {}


int ConnectSocket::NetworkConnection(int sock, struct sockaddr_in address)
{
    return connect(sock, (struct sockaddr *)&address, sizeof(address));
}
