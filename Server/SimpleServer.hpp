#ifndef SIMPLESERVER_HPP
#define SIMPLESERVER_HPP

#include "../Sockets/ListeningSocket.hpp"


class SimpleServer
{
private:
    ListeningSocket *ServerSocket; //Save the parameters for the socket
    virtual void    accept() = 0; //pure virtual function to be implemented in derived classes(gets the info from child classes)
    virtual void    handle() = 0;
    virtual void    respond() = 0;
public:
    SimpleServer(int domain, int service, int protocol, int port, u_long interface, int bklg);
    ~SimpleServer();

    virtual void    launch() = 0;
    ListeningSocket  *getServerSocket();
};

#endif