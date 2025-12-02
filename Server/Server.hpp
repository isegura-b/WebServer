#ifndef SERVER_HPP
#define SERVER_HPP

#include "SimpleServer.hpp"
#include "../HTTP/HttpRequest.hpp"
#include <map>
#include <vector>
#include <string>
#include <ctime>

class ListeningSocket;

struct Connection
{
    enum State
    {
        READING_HEADERS,
        READING_BODY,
        READY_TO_RESPOND,
        WRITING_RESPONSE,
        CLOSED
    };
    int fd; // client socket file descriptor
    std::string in;
    std::string out;
    State state;
    std::time_t lastActivity;
    HttpRequest req;
    size_t expectedBodyLen;
    Connection();
    Connection(int f);
};

class Server : public SimpleServer
{
private:
    std::map<int, Connection> _connections;         // active client connections multiport (key(fd) -> value(Connection struct))
    std::vector<int> _listenFds;                    // listening sockets (saves fds for multi-port in a list)
    std::vector<ListeningSocket *> _extraListeners; // additional listeners we create (ownership)

    void acceptNew(int listenFd);
    void processReadable(Connection &c);
    void processWritable(Connection &c);
    void accept();
    void handle();
    void respond();

public:
    Server();
    Server(const std::vector<int> &ports);
    ~Server();

    void launch();
};

#endif