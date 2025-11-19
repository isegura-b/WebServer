#include "Server.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <algorithm>
#include "../Sockets/ListeningSocket.hpp"

// Connection init struct

Connection::Connection()
    : fd(-1), state(READING_HEADERS), lastActivity(std::time(NULL))
{
}

Connection::Connection(int f)
    : fd(f), state(READING_HEADERS), lastActivity(std::time(NULL))
{
}

// SimpleServer
Server::Server()
    : SimpleServer(AF_INET, SOCK_STREAM, 0, 8080, INADDR_ANY, 10)
{
    int sfd = getServerSocket()->getSocket();
    _listenFds.push_back(sfd);
}

Server::~Server() {
    for (std::size_t i = 0; i < _extraListeners.size(); ++i)
        delete _extraListeners[i];
}

Server::Server(const std::vector<int> &ports)
    : SimpleServer(AF_INET, SOCK_STREAM, 0, ports.empty() ? 8080 : ports[0], INADDR_ANY, 10)
{
    int sfd = getServerSocket()->getSocket();
    _listenFds.push_back(sfd);
    for (std::size_t i = 1; i < ports.size(); ++i)      //extra listeners for remaining ports
    {
        ListeningSocket *ls = new ListeningSocket(AF_INET, SOCK_STREAM, 0, ports[i], INADDR_ANY, 10);
        _extraListeners.push_back(ls);
        _listenFds.push_back(ls->getSocket());
    }
}

void Server::acceptNew(int listenFd)
{
    int cfd = ::accept(listenFd, NULL, NULL);
    if (cfd < 0)
    {
        perror("accept");
        return;
    }
    int flags = fcntl(cfd, F_GETFL, 0);
    fcntl(cfd, F_SETFL, flags | O_NONBLOCK);
    _connections.insert(std::make_pair(cfd, Connection(cfd)));
    struct sockaddr_in lsaddr; socklen_t llen = sizeof(lsaddr);
    int p = 0;
    if (::getsockname(listenFd, (struct sockaddr*)&lsaddr, &llen) == 0)
        p = ntohs(lsaddr.sin_port);
    std::cout << "[+] Nueva conexión fd=" << cfd << " (http://localhost:" << p << "/)" << std::endl;
}

static bool endsWithHeaders(const std::string &s)
{
    if (s.size() < 4)
        return false;
    return s.find("\r\n\r\n") != std::string::npos;
}

void Server::processReadable(Connection &c)
{
    char buf[5000];
    int r = ::read(c.fd, buf, sizeof(buf));
    if (r == 0)
    {
        c.state = Connection::CLOSED;
        return;
    }
    if (r < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        c.state = Connection::CLOSED;
        return;
    }
    c.in.append(buf, r);
    c.lastActivity = std::time(NULL);
    if (c.state == Connection::READING_HEADERS && endsWithHeaders(c.in))
    {
        c.state = Connection::READY_TO_RESPOND;
        const char *body = "Hello, World!";
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Content-Length: ";
        char lenBuf[32];
        std::sprintf(lenBuf, "%zu", std::strlen(body));
        response += lenBuf;
        response += "\r\nConnection: close\r\n\r\n";
        response += body;
        c.out = response;
        c.state = Connection::WRITING_RESPONSE;
    }
}

void Server::processWritable(Connection &c)
{
    if (c.out.empty())
    {
        if (c.state == Connection::WRITING_RESPONSE)
            c.state = Connection::CLOSED;
        return;
    }
    int w = ::write(c.fd, c.out.c_str(), c.out.size());
    if (w < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        c.state = Connection::CLOSED;
        return;
    }
    if (w > 0)
    {
        c.out.erase(0, w);
        c.lastActivity = std::time(NULL);
    }
    if (c.out.empty())
        c.state = Connection::CLOSED;
}

void Server::launch()
{
    struct sockaddr_in addr = getServerSocket()->getAddress();
    int port = ntohs(addr.sin_port);
    std::cout << "Servidor escuchando en puerto " << port << std::endl;
    std::cout << "URL: http://localhost:" << port << "/" << std::endl;
    // Print extra listener URLs if any
    for (std::size_t i = 0; i < _extraListeners.size(); ++i)
    {
        int p = ntohs(_extraListeners[i]->getAddress().sin_port);
        std::cout << "Servidor escuchando en puerto " << p << std::endl;
        std::cout << "URL: http://localhost:" << p << "/" << std::endl;
    }

    while (1)
    {
        std::vector<struct pollfd> pfds;
        for (std::vector<int>::iterator i = _listenFds.begin(); i != _listenFds.end(); ++i)
        {
            struct pollfd p;
            p.fd = *i;
            p.events = POLLIN;
            p.revents = 0;
            pfds.push_back(p);
        }
        for (std::map<int, Connection>::iterator i = _connections.begin(); i != _connections.end(); ++i)
        {
            struct pollfd p;
            p.fd = i->first;
            if (i->second.state == Connection::WRITING_RESPONSE)
                p.events = POLLOUT;
            else
                p.events = POLLIN;
            p.revents = 0;
            pfds.push_back(p);
        }

        int ret = poll(&pfds[0], pfds.size(), 1000);
        if (ret < 0)
        {
            perror("poll");
            continue;
        }

        for (std::size_t i = 0; i < pfds.size(); ++i)
        {
            struct pollfd &pfd = pfds[i];
            bool isListener = std::find(_listenFds.begin(), _listenFds.end(), pfd.fd) != _listenFds.end();
            if (isListener)
            {
                if (pfd.revents & POLLIN)
                    acceptNew(pfd.fd);
                continue;
            }
            std::map<int, Connection>::iterator cit = _connections.find(pfd.fd);
            if (cit == _connections.end())
                continue;
            Connection &conn = cit->second;
            if (pfd.revents & POLLIN)
                processReadable(conn);
            if (pfd.revents & POLLOUT)
                processWritable(conn);
        }

        std::time_t now = std::time(NULL);
        for (std::map<int, Connection>::iterator i = _connections.begin(); i != _connections.end();)
        {
            Connection &c = i->second;
            bool erase = false;
            if (c.state == Connection::CLOSED)
                erase = true;
            else if (now - c.lastActivity > 30)
                erase = true;
            if (erase)
            {
                ::close(c.fd);
                std::map<int, Connection>::iterator toErase = i;
                ++i;
                _connections.erase(toErase);
            }
            else
            {
                ++i;
            }
        }
    }
}


// No multi-port
void Server::accept() {}
void Server::handle() {}
void Server::respond() {}