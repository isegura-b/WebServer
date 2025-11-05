#include "ListeningSocket.hpp"

ListeningSocket::ListeningSocket(int domain, int type, int protocol, int port, u_long interface, int bklg)
:BindSocket(domain, type, protocol, port, interface)
{
    this->backlog = bklg;
    this->startListening();
}

ListeningSocket::ListeningSocket(const ListeningSocket &other)
: BindSocket(other), backlog(other.backlog), listening(other.listening)
{
}

ListeningSocket &ListeningSocket::operator=(const ListeningSocket &other)
{
    if (this != &other)
    {
        BindSocket::operator=(other);
        this->backlog = other.backlog;
        this->listening = other.listening;
    }
    return *this;
}

ListeningSocket::~ListeningSocket()
{
}

void    ListeningSocket::startListening()
{
    listening = listen(getSocket(), this->backlog);
    if (listening < 0)
    {
        perror("Error in listen");
        exit(EXIT_FAILURE);
    }
}