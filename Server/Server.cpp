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
#include "../HTTP/HttpRequest.hpp"
#include "../HTTP/HttpParser.hpp"
#include "../HTTP/HttpResponse.hpp"

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

Server::~Server()
{
    for (std::size_t i = 0; i < _extraListeners.size(); ++i)
        delete _extraListeners[i];
}

Server::Server(const std::vector<int> &ports, const Config &cfg)
    : SimpleServer(AF_INET, SOCK_STREAM, 0, ports.empty() ? 8080 : ports[0], INADDR_ANY, 10), _config(cfg)
{
    int sfd = getServerSocket()->getSocket();
    _listenFds.push_back(sfd);
    for (std::size_t i = 1; i < ports.size(); ++i) // extra listeners for remaining ports
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
    fcntl(cfd, F_SETFL, flags | O_NONBLOCK); // set non-blocking
    _connections.insert(std::make_pair(cfd, Connection(cfd)));
    struct sockaddr_in lsaddr;
    socklen_t llen = sizeof(lsaddr);
    int p = 0;
    if (::getsockname(listenFd, (struct sockaddr *)&lsaddr, &llen) == 0)
        p = ntohs(lsaddr.sin_port);
    std::cout << "[+] Nueva conexión fd=" << cfd << " (http://localhost:" << p << "/)" << std::endl;
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
        c.state = Connection::CLOSED;
        return;
    }

    c.in.append(buf, r);
    c.lastActivity = std::time(NULL);

    if (c.state == Connection::READING_HEADERS)
    {
        size_t headerPos = c.in.find("\r\n\r\n");
        if (headerPos == std::string::npos)
            return;

        std::string headerPart = c.in.substr(0, headerPos + 4);

        HttpRequest req;
        if (!HttpParser::parse(req, headerPart))
            return;

        c.req = req;

        long contentLen = -1;
        if (req.headers.count("Content-Length"))
            contentLen = atol(req.headers["Content-Length"].c_str());

        if (contentLen > 0)
        {
            c.expectedBodyLen = (size_t)contentLen;
            c.state = Connection::READING_BODY;
        }
        else
        {
            c.expectedBodyLen = 0;
            c.state = Connection::READY_TO_RESPOND;
        }
    }

    if (c.state == Connection::READING_BODY)
    {
        size_t headerPos = c.in.find("\r\n\r\n");
        size_t bodyStart = headerPos + 4;
        size_t have = 0;
        if (c.in.size() > bodyStart)
            have = c.in.size() - bodyStart;

        if (have < c.expectedBodyLen)
            return;

        c.req.body = c.in.substr(bodyStart, c.expectedBodyLen);

        std::string remainder;
        if (c.in.size() > bodyStart + c.expectedBodyLen)
            remainder = c.in.substr(bodyStart + c.expectedBodyLen);

        c.in = remainder;
        c.state = Connection::READY_TO_RESPOND;
    }

    // Simple response for testing ------HERE-------------
    if (c.state == Connection::READY_TO_RESPOND)
    {
        HttpResponse res;
        res.setStatus(200, "OK");
        res.setHeader("Content-Type", "text/plain");
        res.setBody("Hello from Webserv!");

        c.out = res.serialize();
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
        c.state = Connection::CLOSED;
        return;
    }
    if (w > 0)
    {
        c.out.erase(0, w);
        c.lastActivity = std::time(NULL);
    }
    if (c.out.empty())
    {
        c.state = Connection::READING_HEADERS; // listo para siguiente request
        c.in.clear();
        c.req = HttpRequest();
    }
}

void Server::launch()
{
    struct sockaddr_in addr = getServerSocket()->getAddress();
    int port = ntohs(addr.sin_port);
    std::cout << "Servidor escuchando en puerto " << port << std::endl;
    std::cout << "URL: http://localhost:" << port << "/" << std::endl;
    for (std::size_t i = 0; i < _extraListeners.size(); ++i)
    {
        int p = ntohs(_extraListeners[i]->getAddress().sin_port);
        std::cout << "Servidor escuchando en puerto " << p << std::endl;
        std::cout << "URL: http://localhost:" << p << "/" << std::endl;
    }

    while (1)
    {
        std::vector<struct pollfd> pfds;
        // listeners in pfds vector
        for (std::vector<int>::iterator i = _listenFds.begin(); i != _listenFds.end(); ++i)
        {
            struct pollfd p;
            p.fd = *i;
            p.events = POLLIN;
            p.revents = 0;
            pfds.push_back(p);
        }
        // connections in pfds vector (client)
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
            break;
        }

        // handle events
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

        // Clean up
        std::time_t now = std::time(NULL);
        std::map<int, Connection>::iterator it = _connections.begin();
        while (it != _connections.end())
        {
            Connection &c = it->second;

            if (c.state == Connection::CLOSED || (now - c.lastActivity > 30))
            {
                ::close(c.fd);
                std::map<int, Connection>::iterator tmp = it;
                ++it;
                _connections.erase(tmp);
            }
            else
            {
                ++it;
            }
        }
    }
}

// No multi-port
void Server::accept() {}
void Server::handle() {}
void Server::respond() {}