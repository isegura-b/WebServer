#ifndef CONNECT_SOCKET_HPP
#define CONNECT_SOCKET_HPP

#include "SimpleSocket.hpp"

class ConnectSocket : public SimpleSocket
{
    private:
    public:
    ConnectSocket(int domain, int service, int protocol, int port, u_long interface);
    ConnectSocket(const ConnectSocket &other);
    ConnectSocket &operator=(const ConnectSocket &other);
    ~ConnectSocket();

    int NetworkConnection(int sock, struct sockaddr_in address); // Override for connecting(client)

};
#endif