#ifndef SERVER_HPP
#define SERVER_HPP

#include "SimpleServer.hpp"

class Server : public SimpleServer
{
    private:
        int     clientSocket;
        char    buffer[30000];

        void    accept();
        void    handle();
        void    respond();
    public:
        Server();
        ~Server();

        void    launch();
};

#endif