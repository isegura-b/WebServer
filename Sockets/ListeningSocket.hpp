#ifndef LISTENING_SOCKET_HPP
#define LISTENING_SOCKET_HPP

#include "BindSocket.hpp"

class ListeningSocket : public BindSocket
{
    private:
        int backlog;
        int listening;
    public:
        ListeningSocket(int domain, int type, int protocol, int port, u_long interface, int bklg); //nº serving in line (backlog = 10 if 11->out of line)
        ListeningSocket(const ListeningSocket &other);
        ListeningSocket &operator=(const ListeningSocket &other);
        ~ListeningSocket();
    
    void    startListening();
};

#endif