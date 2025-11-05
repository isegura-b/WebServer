#ifndef BIND_SOCKET_HPP
#define BIND_SOCKET_HPP

#include "SimpleSocket.hpp"

class BindSocket : public SimpleSocket
{
    private:
    public:
    BindSocket(int domain, int service, int protocol, int port, u_long interface);
    BindSocket(const BindSocket &other);
    BindSocket &operator=(const BindSocket &other);
    ~BindSocket();

    int NetworkConnection(int sock, struct sockaddr_in address); // Override for binding(Server)

};
#endif