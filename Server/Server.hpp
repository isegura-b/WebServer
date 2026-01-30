#ifndef SERVER_HPP
#define SERVER_HPP

#include "SimpleServer.hpp"
#include "RequestHandler.hpp"
#include "../HTTP/HttpRequest.hpp"
#include "../config/Config.hpp"
#include <map>
#include <vector>
#include <string>
#include <ctime>
#include <sys/types.h>

class ListeningSocket;

struct Connection
{
    enum State
    {
        READING_HEADERS,
        READING_BODY,
        READY_TO_RESPOND,
        CGI_IN_PROGRESS,
        WRITING_RESPONSE,
        CLOSED
    };
    enum ChunkState
    {
        CHUNK_SIZE,
        CHUNK_DATA,
        CHUNK_CRLF,
        CHUNK_DONE
    };
    int fd; // client socket file descriptor
    std::string in;
    std::string out;
    State state;
    std::time_t lastActivity;
    HttpRequest req;
    size_t expectedBodyLen;
    bool isChunked;
    bool badRequest;
    std::string bodyBuffer;
    size_t chunkSize;
    ChunkState chunkState;
    Connection();
    Connection(int f);
};

struct CgiContext
{
    enum State
    {
        INIT,
        WRITING_BODY,
        READING_OUTPUT,
        DONE,
        ERROR,
        TIMEOUT
    };
    int clientFd;
    int listenPort;
    int stdinWrite;
    int stdoutRead;
    int stderrRead;
    pid_t pid;
    State state;
    std::time_t startTime;
    std::time_t lastActivity;
    std::string body;
    size_t bodySent;
    std::string output;
    std::string err;
    int exitStatus;
    CgiContext();
};

class Server : public SimpleServer
{
private:
    std::map<int, Connection> _connections;         // active client connections multiport (key(fd) -> value(Connection struct))
    std::vector<int> _listenFds;                    // listening sockets (saves fds for multi-port in a list)
    std::vector<ListeningSocket *> _extraListeners; // additional listeners we create (ownership)
	RequestHandler _handler;
    std::map<int, CgiContext> _cgi;


    void acceptNew(int listenFd);
    void processReadable(Connection &c);
    void processWritable(Connection &c);
    void startCgi(Connection &c, int port, const CgiJob& job);
    void handleCgiEvent(int clientFd, int fdType, short revents);
    void finalizeCgi(int clientFd, bool ok);
    bool parseChunkedBody(Connection &c);
    void cleanupCgi(int clientFd);

    Config _config;

public:
    //Server();
    Server(const std::vector<int> &ports, const Config &cfg);
    ~Server();

    void launch();
};

#endif
