#ifndef CGI_HPP
#define CGI_HPP

#include <map>
#include <string>
#include <vector>
#include <ctime>
#include <sys/types.h>
#include <poll.h>

class RequestHandler;
class Connection;
struct CgiJob;

enum CgiFdType
{
    CGI_FD_STDIN = 1,
    CGI_FD_STDOUT = 2,
    CGI_FD_STDERR = 3
};

struct CgiFdInfo
{
    int clientFd;
    int type;
    CgiFdInfo() : clientFd(-1), type(0) {}
    CgiFdInfo(int c, int t) : clientFd(c), type(t) {}
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

class CgiModule
{
public:
    void start(Connection &c, int port, const CgiJob &job, RequestHandler &handler);
    void addPollFds(std::vector<struct pollfd> &pfds, std::map<int, CgiFdInfo> &fdMap);
    void handleEvent(int clientFd, int fdType, short revents, std::map<int, Connection> &conns, RequestHandler &handler);
    void tick(std::time_t now, std::map<int, Connection> &conns, RequestHandler &handler);
    void killForClient(int clientFd);
    bool hasClient(int clientFd) const;

private:
    std::map<int, CgiContext> _ctx;
    void finalizeCgi(int clientFd, bool ok, std::map<int, Connection> &conns, RequestHandler &handler);
    void cleanupCgi(int clientFd);
};

#endif
