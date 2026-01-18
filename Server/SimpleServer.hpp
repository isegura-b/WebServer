#ifndef SIMPLESERVER_HPP
#define SIMPLESERVER_HPP

#include "../Sockets/ListeningSocket.hpp"


class SimpleServer
{
private:
    ListeningSocket *ServerSocket; //Save the parameters for the socket
public:
    SimpleServer(int domain, int service, int protocol, int port, u_long interface, int bklg);
    ~SimpleServer();

    virtual void    launch() = 0;
    ListeningSocket  *getServerSocket();
};

#endif