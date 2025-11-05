#include "SimpleSocket.hpp"

// Constructor
SimpleSocket::SimpleSocket(int domain, int type, int protocol, int port, u_long interface)
    : sock(-1), connection(-1)
{
    // Address initialization
    address.sin_family = domain;                // domain 
    address.sin_port = htons(port);             // port number  (htons means host to network short)
    address.sin_addr.s_addr = htonl(interface); // ip address   (htonl means host to network long)

    // Establish socket
    sock = socket(domain, type, protocol);
    if (sock == -1)
    {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }
}

// Copy constructor
SimpleSocket::SimpleSocket(const SimpleSocket &other)
    : sock(other.sock), connection(other.connection), address(other.address)
{
}

// Assignment operator
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

// Destructor
SimpleSocket::~SimpleSocket()
{
    if (sock >= 0)
        close(sock);
    if (connection >= 0)
        close(connection);
}

// Getters
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

// Setters
void SimpleSocket::setConnection(int connection)
{
    this->connection = connection;
}