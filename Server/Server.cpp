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
#include <cstdlib>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <signal.h>
#include "../Sockets/ListeningSocket.hpp"
#include "../HTTP/HttpRequest.hpp"
#include "../HTTP/HttpParser.hpp"
#include "../HTTP/HttpResponse.hpp"

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

static void setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static std::string trimCrlf(const std::string& s)
{
    if (!s.empty() && s[s.size() - 1] == '\r')
        return s.substr(0, s.size() - 1);
    return s;
}

// Connection init struct

Connection::Connection()
    : fd(-1), state(READING_HEADERS), lastActivity(std::time(NULL)), expectedBodyLen(0),
      isChunked(false), badRequest(false), chunkSize(0), chunkState(CHUNK_SIZE)
{
}

Connection::Connection(int f)
    : fd(f), state(READING_HEADERS), lastActivity(std::time(NULL)), expectedBodyLen(0),
      isChunked(false), badRequest(false), chunkSize(0), chunkState(CHUNK_SIZE)
{
}

CgiContext::CgiContext()
    : clientFd(-1), listenPort(0), stdinWrite(-1), stdoutRead(-1), stderrRead(-1), pid(-1),
      state(INIT), startTime(0), lastActivity(0), bodySent(0), exitStatus(0)
{
}

// SimpleServer
//Server::Server()
//    : SimpleServer(AF_INET, SOCK_STREAM, 0, 8080, INADDR_ANY, 10)
//{
//    int sfd = getServerSocket()->getSocket();
//    _listenFds.push_back(sfd);
//}

Server::~Server()
{
    for (std::size_t i = 0; i < _extraListeners.size(); ++i)
        delete _extraListeners[i];
}

Server::Server(const std::vector<int> &ports, const Config &cfg)
    : SimpleServer(AF_INET, SOCK_STREAM, 0, ports.empty() ? 8080 : ports[0], INADDR_ANY, 10), _handler(cfg), _config(cfg)
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
        c.isChunked = false;
        c.badRequest = false;
        c.bodyBuffer.clear();
        c.chunkSize = 0;
        c.chunkState = Connection::CHUNK_SIZE;

        long contentLen = -1;
        if (req.headers.count("Content-Length"))
            contentLen = atol(req.headers["Content-Length"].c_str());

        if (req.headers.count("Transfer-Encoding"))
        {
            std::string te = req.headers["Transfer-Encoding"];
            if (te.find("chunked") != std::string::npos || te.find("Chunked") != std::string::npos)
            {
                c.isChunked = true;
                c.state = Connection::READING_BODY;
            }
        }

        if (!c.isChunked && contentLen > 0)
        {
            c.expectedBodyLen = (size_t)contentLen;
            c.state = Connection::READING_BODY;
        }
        else if (!c.isChunked)
        {
            c.expectedBodyLen = 0;
            c.state = Connection::READY_TO_RESPOND;
        }
    }

    if (c.state == Connection::READING_BODY)
    {
        size_t headerPos = c.in.find("\r\n\r\n");
        size_t bodyStart = 0;
        if (headerPos != std::string::npos)
        {
            bodyStart = headerPos + 4;
            if (c.in.size() < bodyStart)
                return;
            std::string bodyPart = c.in.substr(bodyStart);
            c.in = bodyPart;
        }

        if (c.isChunked)
        {
            if (!parseChunkedBody(c))
                return;
            if (c.badRequest)
            {
                c.state = Connection::READY_TO_RESPOND;
            }
            else if (c.chunkState == Connection::CHUNK_DONE)
            {
                c.req.body = c.bodyBuffer;
                c.bodyBuffer.clear();
                c.state = Connection::READY_TO_RESPOND;
            }
            else
            {
                return;
            }
        }
        else
        {
            size_t have = c.in.size();
            if (have < c.expectedBodyLen)
                return;
            c.req.body = c.in.substr(0, c.expectedBodyLen);
            std::string remainder;
            if (c.in.size() > c.expectedBodyLen)
                remainder = c.in.substr(c.expectedBodyLen);
            c.in = remainder;
            c.state = Connection::READY_TO_RESPOND;
        }
    }

    // Conecition with request handler
    if (c.state == Connection::READY_TO_RESPOND)
    {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        if (::getsockname(c.fd, (struct sockaddr *)&addr, &len) == -1)
        {
            perror("getsockname");
        }
        int port = ntohs(addr.sin_port);

        if (c.badRequest)
        {
            HttpResponse res = _handler.errorForPort(400, port);
            c.out = res.serialize();
            c.state = Connection::WRITING_RESPONSE;
            return;
        }

        CgiJob job;
        HttpResponse cgiErr;
        CgiJob::Decision decision = _handler.buildCgiJob(c.req, port, job, cgiErr);
        if (decision == CgiJob::CGI_ERROR)
        {
            c.out = cgiErr.serialize();
            c.state = Connection::WRITING_RESPONSE;
            return;
        }
        if (decision == CgiJob::CGI_YES)
        {
            startCgi(c, port, job);
            return;
        }

        HttpResponse res = _handler.handle(c.req, port);
        c.out = res.serialize();
        c.state = Connection::WRITING_RESPONSE;
    }
}

bool Server::parseChunkedBody(Connection &c)
{
    while (true)
    {
        if (c.chunkState == Connection::CHUNK_SIZE)
        {
            size_t pos = c.in.find("\r\n");
            if (pos == std::string::npos)
                return false;
            std::string line = c.in.substr(0, pos);
            c.in.erase(0, pos + 2);

            size_t sc = line.find(';');
            if (sc != std::string::npos)
                line = line.substr(0, sc);

            char *endptr = NULL;
            long sz = std::strtol(line.c_str(), &endptr, 16);
            if (endptr == line.c_str() || sz < 0)
            {
                c.badRequest = true;
                c.chunkState = Connection::CHUNK_DONE;
                return true;
            }
            if (sz == 0)
            {
                c.chunkState = Connection::CHUNK_DONE;
                continue;
            }
            c.chunkSize = static_cast<size_t>(sz);
            c.chunkState = Connection::CHUNK_DATA;
        }
        if (c.chunkState == Connection::CHUNK_DATA)
        {
            if (c.in.size() < c.chunkSize)
                return false;
            c.bodyBuffer.append(c.in, 0, c.chunkSize);
            c.in.erase(0, c.chunkSize);
            c.chunkState = Connection::CHUNK_CRLF;
        }
        if (c.chunkState == Connection::CHUNK_CRLF)
        {
            if (c.in.size() < 2)
                return false;
            if (c.in.substr(0, 2) != "\r\n")
            {
                c.badRequest = true;
                c.chunkState = Connection::CHUNK_DONE;
                return true;
            }
            c.in.erase(0, 2);
            c.chunkState = Connection::CHUNK_SIZE;
        }
        if (c.chunkState == Connection::CHUNK_DONE)
        {
            size_t end = c.in.find("\r\n\r\n");
            if (end != std::string::npos)
            {
                c.in.erase(0, end + 4);
                return true;
            }
            if (c.in.size() >= 2 && c.in.substr(0, 2) == "\r\n")
            {
                c.in.erase(0, 2);
                return true;
            }
            return false;
        }
    }
    return false;
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

void Server::startCgi(Connection &c, int port, const CgiJob& job)
{
    int inPipe[2];
    int outPipe[2];
    int errPipe[2];

    if (pipe(inPipe) < 0 || pipe(outPipe) < 0 || pipe(errPipe) < 0)
    {
        HttpResponse res = _handler.errorForPort(500, port);
        c.out = res.serialize();
        c.state = Connection::WRITING_RESPONSE;
        return;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        HttpResponse res = _handler.errorForPort(500, port);
        c.out = res.serialize();
        c.state = Connection::WRITING_RESPONSE;
        ::close(inPipe[0]); ::close(inPipe[1]);
        ::close(outPipe[0]); ::close(outPipe[1]);
        ::close(errPipe[0]); ::close(errPipe[1]);
        return;
    }

    if (pid == 0)
    {
        ::dup2(inPipe[0], STDIN_FILENO);
        ::dup2(outPipe[1], STDOUT_FILENO);
        ::dup2(errPipe[1], STDERR_FILENO);

        ::close(inPipe[0]); ::close(inPipe[1]);
        ::close(outPipe[0]); ::close(outPipe[1]);
        ::close(errPipe[0]); ::close(errPipe[1]);

        if (!job.cwd.empty())
            ::chdir(job.cwd.c_str());

        std::vector<std::string> envStrings;
        for (std::map<std::string, std::string>::const_iterator it = job.env.begin(); it != job.env.end(); ++it)
            envStrings.push_back(it->first + "=" + it->second);
        std::vector<char *> envp;
        for (size_t i = 0; i < envStrings.size(); ++i)
            envp.push_back(const_cast<char *>(envStrings[i].c_str()));
        envp.push_back(NULL);

        std::vector<char *> argv;
        argv.push_back(const_cast<char *>(job.interpreter.c_str()));
        argv.push_back(const_cast<char *>(job.scriptFilename.c_str()));
        argv.push_back(NULL);

        ::execve(job.interpreter.c_str(), &argv[0], &envp[0]);
        _exit(127);
    }

    ::close(inPipe[0]);
    ::close(outPipe[1]);
    ::close(errPipe[1]);

    setNonBlocking(inPipe[1]);
    setNonBlocking(outPipe[0]);
    setNonBlocking(errPipe[0]);

    CgiContext ctx;
    ctx.clientFd = c.fd;
    ctx.listenPort = port;
    ctx.stdinWrite = inPipe[1];
    ctx.stdoutRead = outPipe[0];
    ctx.stderrRead = errPipe[0];
    ctx.pid = pid;
    ctx.startTime = std::time(NULL);
    ctx.lastActivity = ctx.startTime;
    ctx.body = c.req.body;
    ctx.bodySent = 0;
    ctx.state = ctx.body.empty() ? CgiContext::READING_OUTPUT : CgiContext::WRITING_BODY;

    _cgi[c.fd] = ctx;
    c.state = Connection::CGI_IN_PROGRESS;
    c.lastActivity = ctx.startTime;
}

static bool parseCgiOutput(const std::string& out, HttpResponse& res)
{
    if (out.empty())
        return false;

    size_t sep = out.find("\r\n\r\n");
    size_t sepLen = 4;
    if (sep == std::string::npos)
    {
        sep = out.find("\n\n");
        sepLen = 2;
    }
    if (sep == std::string::npos)
        return false;

    std::string headerPart = out.substr(0, sep);
    std::string bodyPart = out.substr(sep + sepLen);

    std::istringstream iss(headerPart);
    std::string line;
    int statusCode = 200;
    std::string statusMsg = "OK";
    bool hasContentType = false;

    while (std::getline(iss, line))
    {
        line = trimCrlf(line);
        if (line.empty())
            continue;
        size_t p = line.find(':');
        if (p == std::string::npos)
            return false;
        std::string key = line.substr(0, p);
        std::string val = line.substr(p + 1);
        while (!val.empty() && (val[0] == ' ' || val[0] == '\t'))
            val.erase(0, 1);

        if (key == "Status")
        {
            std::istringstream ss(val);
            ss >> statusCode;
            std::getline(ss, statusMsg);
            if (!statusMsg.empty() && statusMsg[0] == ' ')
                statusMsg.erase(0, 1);
        }
        else
        {
            if (key == "Content-Type")
                hasContentType = true;
            res.setHeader(key, val);
        }
    }

    res.setStatus(statusCode, statusMsg.empty() ? "OK" : statusMsg);
    if (!hasContentType)
        res.setHeader("Content-Type", "text/html");
    res.setBody(bodyPart);
    return true;
}

void Server::finalizeCgi(int clientFd, bool ok)
{
    std::map<int, CgiContext>::iterator it = _cgi.find(clientFd);
    if (it == _cgi.end())
        return;

    std::map<int, Connection>::iterator cit = _connections.find(clientFd);
    if (cit == _connections.end())
    {
        cleanupCgi(clientFd);
        return;
    }
    Connection &conn = cit->second;
    CgiContext &ctx = it->second;

    if (!ok)
    {
        HttpResponse res = _handler.errorForPort(500, ctx.listenPort);
        conn.out = res.serialize();
        conn.state = Connection::WRITING_RESPONSE;
        cleanupCgi(clientFd);
        return;
    }

    HttpResponse res;
    if (!parseCgiOutput(ctx.output, res))
    {
        std::cerr << "[cgi] parse failed for client " << clientFd
                  << " stdout size=" << ctx.output.size()
                  << " stderr size=" << ctx.err.size() << std::endl;
        if (!ctx.err.empty())
            std::cerr << "[cgi] stderr: " << ctx.err << std::endl;
        HttpResponse err = _handler.errorForPort(502, ctx.listenPort);
        conn.out = err.serialize();
        conn.state = Connection::WRITING_RESPONSE;
        cleanupCgi(clientFd);
        return;
    }

    conn.out = res.serialize();
    conn.state = Connection::WRITING_RESPONSE;
    cleanupCgi(clientFd);
}

void Server::cleanupCgi(int clientFd)
{
    std::map<int, CgiContext>::iterator it = _cgi.find(clientFd);
    if (it == _cgi.end())
        return;
    CgiContext &ctx = it->second;
    if (ctx.stdinWrite >= 0)
        ::close(ctx.stdinWrite);
    if (ctx.stdoutRead >= 0)
        ::close(ctx.stdoutRead);
    if (ctx.stderrRead >= 0)
        ::close(ctx.stderrRead);
    _cgi.erase(it);
}

void Server::handleCgiEvent(int clientFd, int fdType, short revents)
{
    std::map<int, CgiContext>::iterator it = _cgi.find(clientFd);
    if (it == _cgi.end())
        return;
    CgiContext &ctx = it->second;
    ctx.lastActivity = std::time(NULL);
    std::map<int, Connection>::iterator cit = _connections.find(clientFd);
    if (cit != _connections.end())
        cit->second.lastActivity = ctx.lastActivity;

    if (revents & (POLLERR | POLLNVAL))
    {
        finalizeCgi(clientFd, false);
        return;
    }

    if (fdType == CGI_FD_STDIN && (revents & POLLOUT))
    {
        if (ctx.stdinWrite >= 0)
        {
            size_t remaining = ctx.body.size() - ctx.bodySent;
            if (remaining > 0)
            {
                int w = ::write(ctx.stdinWrite, ctx.body.data() + ctx.bodySent, remaining);
                if (w > 0)
                {
                    ctx.bodySent += static_cast<size_t>(w);
                }
                else if (w < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    finalizeCgi(clientFd, false);
                    return;
                }
            }
            if (ctx.bodySent >= ctx.body.size())
            {
                ::close(ctx.stdinWrite);
                ctx.stdinWrite = -1;
                if (ctx.state == CgiContext::WRITING_BODY)
                    ctx.state = CgiContext::READING_OUTPUT;
            }
        }
    }

    if ((fdType == CGI_FD_STDOUT || fdType == CGI_FD_STDERR) && (revents & (POLLIN | POLLHUP)))
    {
        int fd = (fdType == CGI_FD_STDOUT) ? ctx.stdoutRead : ctx.stderrRead;
        if (fd >= 0)
        {
            char buf[4096];
            while (true)
            {
                int r = ::read(fd, buf, sizeof(buf));
                if (r > 0)
                {
                    if (fdType == CGI_FD_STDOUT)
                        ctx.output.append(buf, r);
                    else
                        ctx.err.append(buf, r);
                }
                else if (r == 0)
                {
                    ::close(fd);
                    if (fdType == CGI_FD_STDOUT)
                        ctx.stdoutRead = -1;
                    else
                        ctx.stderrRead = -1;
                    break;
                }
                else
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;
                    finalizeCgi(clientFd, false);
                    return;
                }
            }
        }
    }

    if (ctx.stdoutRead < 0)
    {
        finalizeCgi(clientFd, true);
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
        std::map<int, CgiFdInfo> cgiFdMap;
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
            else if (i->second.state == Connection::CGI_IN_PROGRESS)
                p.events = 0;
            else
                p.events = POLLIN;
            p.revents = 0;
            pfds.push_back(p);
        }
        // CGI pipes
        for (std::map<int, CgiContext>::iterator it = _cgi.begin(); it != _cgi.end(); ++it)
        {
            CgiContext &ctx = it->second;
            if (ctx.stdinWrite >= 0 && ctx.state == CgiContext::WRITING_BODY)
            {
                struct pollfd p;
                p.fd = ctx.stdinWrite;
                p.events = POLLOUT;
                p.revents = 0;
                pfds.push_back(p);
                cgiFdMap[p.fd] = CgiFdInfo(ctx.clientFd, CGI_FD_STDIN);
            }
            if (ctx.stdoutRead >= 0)
            {
                struct pollfd p;
                p.fd = ctx.stdoutRead;
                p.events = POLLIN | POLLHUP;
                p.revents = 0;
                pfds.push_back(p);
                cgiFdMap[p.fd] = CgiFdInfo(ctx.clientFd, CGI_FD_STDOUT);
            }
            if (ctx.stderrRead >= 0)
            {
                struct pollfd p;
                p.fd = ctx.stderrRead;
                p.events = POLLIN | POLLHUP;
                p.revents = 0;
                pfds.push_back(p);
                cgiFdMap[p.fd] = CgiFdInfo(ctx.clientFd, CGI_FD_STDERR);
            }
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
            std::map<int, CgiFdInfo>::iterator cgiIt = cgiFdMap.find(pfd.fd);
            if (cgiIt != cgiFdMap.end())
            {
                handleCgiEvent(cgiIt->second.clientFd, cgiIt->second.type, pfd.revents);
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

            if (c.state == Connection::CLOSED || (c.state != Connection::CGI_IN_PROGRESS && (now - c.lastActivity > 30)))
            {
                std::map<int, CgiContext>::iterator cg = _cgi.find(c.fd);
                if (cg != _cgi.end())
                {
                    if (cg->second.pid > 0)
                        ::kill(cg->second.pid, SIGKILL);
                    cleanupCgi(c.fd);
                }
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
        // CGI timeouts and reap
        std::map<int, CgiContext>::iterator cit = _cgi.begin();
        while (cit != _cgi.end())
        {
            CgiContext &ctx = cit->second;
            if (now - ctx.startTime > 10)
            {
                if (ctx.pid > 0)
                    ::kill(ctx.pid, SIGKILL);
                HttpResponse res = _handler.errorForPort(504, ctx.listenPort);
                std::map<int, Connection>::iterator conIt = _connections.find(ctx.clientFd);
                if (conIt != _connections.end())
                {
                    Connection &conn = conIt->second;
                    conn.out = res.serialize();
                    conn.state = Connection::WRITING_RESPONSE;
                }
                cleanupCgi(ctx.clientFd);
                cit = _cgi.begin();
                continue;
            }
            if (ctx.pid > 0)
                ::waitpid(ctx.pid, NULL, WNOHANG);
            ++cit;
        }
    }
}

// No multi-port
