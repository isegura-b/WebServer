#include "SimpleSocket.hpp"

SimpleSocket::SimpleSocket(int domain, int type, int protocol, int port, u_long interface)
    : sock(-1), connection(-1)
{
    address.sin_family = domain;                // domain 
    address.sin_port = htons(port);             // port number  (htons means host to network short)
    address.sin_addr.s_addr = htonl(interface); // ip address   (htonl means host to network long)

    sock = socket(domain, type, protocol);
    if (sock == -1)
    {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }
}

SimpleSocket::SimpleSocket(const SimpleSocket &other)
    : sock(other.sock), connection(other.connection), address(other.address)
{
}

SimpleSocket &SimpleSocket::operator=(const SimpleSocket &other)
{
    if (this != &other)
    {
        sock = other.sock;
        connection = other.connection;
        address = other.address;
    }
    return *this;
}

SimpleSocket::~SimpleSocket()
{
    if (sock >= 0)
        close(sock);
    if (connection >= 0)
        close(connection);
}

struct sockaddr_in SimpleSocket::getAddress()
{ 
    return address; 
}

int SimpleSocket::getSocket()
{ 
    return sock; 
}

int SimpleSocket::getConnection()
{ 
    return connection; 
}

void SimpleSocket::setConnection(int connection)
{
    this->connection = connection;
}