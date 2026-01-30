#include "Cgi.hpp"
#include "Server.hpp"
#include "RequestHandler.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <cerrno>
#include <sstream>
#include <vector>
#include <iostream>

static void setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static std::string trimCrlf(const std::string &s)
{
    if (!s.empty() && s[s.size() - 1] == '\r')
        return s.substr(0, s.size() - 1);
    return s;
}

CgiContext::CgiContext()
    : clientFd(-1), listenPort(0), stdinWrite(-1), stdoutRead(-1), stderrRead(-1), pid(-1),
      state(INIT), startTime(0), lastActivity(0), bodySent(0), exitStatus(0)
{
}

static bool parseCgiOutput(const std::string &out, HttpResponse &res)
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

void CgiModule::start(Connection &c, int port, const CgiJob &job, RequestHandler &handler)
{
    int inPipe[2];
    int outPipe[2];
    int errPipe[2];

    if (pipe(inPipe) < 0 || pipe(outPipe) < 0 || pipe(errPipe) < 0)
    {
        HttpResponse res = handler.errorForPort(500, port);
        c.out = res.serialize();
        c.state = Connection::WRITING_RESPONSE;
        return;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        HttpResponse res = handler.errorForPort(500, port);
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

    _ctx[c.fd] = ctx;
    c.state = Connection::CGI_IN_PROGRESS;
    c.lastActivity = ctx.startTime;
}

void CgiModule::addPollFds(std::vector<struct pollfd> &pfds, std::map<int, CgiFdInfo> &fdMap)
{
    for (std::map<int, CgiContext>::iterator it = _ctx.begin(); it != _ctx.end(); ++it)
    {
        CgiContext &ctx = it->second;
        if (ctx.stdinWrite >= 0 && ctx.state == CgiContext::WRITING_BODY)
        {
            struct pollfd p;
            p.fd = ctx.stdinWrite;
            p.events = POLLOUT;
            p.revents = 0;
            pfds.push_back(p);
            fdMap[p.fd] = CgiFdInfo(ctx.clientFd, CGI_FD_STDIN);
        }
        if (ctx.stdoutRead >= 0)
        {
            struct pollfd p;
            p.fd = ctx.stdoutRead;
            p.events = POLLIN | POLLHUP;
            p.revents = 0;
            pfds.push_back(p);
            fdMap[p.fd] = CgiFdInfo(ctx.clientFd, CGI_FD_STDOUT);
        }
        if (ctx.stderrRead >= 0)
        {
            struct pollfd p;
            p.fd = ctx.stderrRead;
            p.events = POLLIN | POLLHUP;
            p.revents = 0;
            pfds.push_back(p);
            fdMap[p.fd] = CgiFdInfo(ctx.clientFd, CGI_FD_STDERR);
        }
    }
}

void CgiModule::finalizeCgi(int clientFd, bool ok, std::map<int, Connection> &conns, RequestHandler &handler)
{
    std::map<int, CgiContext>::iterator it = _ctx.find(clientFd);
    if (it == _ctx.end())
        return;
    CgiContext &ctx = it->second;

    std::map<int, Connection>::iterator cit = conns.find(clientFd);
    if (cit == conns.end())
    {
        cleanupCgi(clientFd);
        return;
    }
    Connection &conn = cit->second;

    if (!ok)
    {
        HttpResponse res = handler.errorForPort(500, ctx.listenPort);
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
        HttpResponse err = handler.errorForPort(502, ctx.listenPort);
        conn.out = err.serialize();
        conn.state = Connection::WRITING_RESPONSE;
        cleanupCgi(clientFd);
        return;
    }

    conn.out = res.serialize();
    conn.state = Connection::WRITING_RESPONSE;
    cleanupCgi(clientFd);
}

void CgiModule::cleanupCgi(int clientFd)
{
    std::map<int, CgiContext>::iterator it = _ctx.find(clientFd);
    if (it == _ctx.end())
        return;
    CgiContext &ctx = it->second;
    if (ctx.stdinWrite >= 0)
        ::close(ctx.stdinWrite);
    if (ctx.stdoutRead >= 0)
        ::close(ctx.stdoutRead);
    if (ctx.stderrRead >= 0)
        ::close(ctx.stderrRead);
    _ctx.erase(it);
}

void CgiModule::handleEvent(int clientFd, int fdType, short revents, std::map<int, Connection> &conns, RequestHandler &handler)
{
    std::map<int, CgiContext>::iterator it = _ctx.find(clientFd);
    if (it == _ctx.end())
        return;
    CgiContext &ctx = it->second;
    ctx.lastActivity = std::time(NULL);
    std::map<int, Connection>::iterator cit = conns.find(clientFd);
    if (cit != conns.end())
        cit->second.lastActivity = ctx.lastActivity;

    if (revents & (POLLERR | POLLNVAL))
    {
        finalizeCgi(clientFd, false, conns, handler);
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
                    finalizeCgi(clientFd, false, conns, handler);
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
                    finalizeCgi(clientFd, false, conns, handler);
                    return;
                }
            }
        }
    }

    if (ctx.stdoutRead < 0)
    {
        finalizeCgi(clientFd, true, conns, handler);
    }
}

void CgiModule::tick(std::time_t now, std::map<int, Connection> &conns, RequestHandler &handler)
{
    std::map<int, CgiContext>::iterator it = _ctx.begin();
    while (it != _ctx.end())
    {
        CgiContext &ctx = it->second;
        if (now - ctx.startTime > 10)
        {
            if (ctx.pid > 0)
                ::kill(ctx.pid, SIGKILL);
            std::map<int, Connection>::iterator conIt = conns.find(ctx.clientFd);
            if (conIt != conns.end())
            {
                HttpResponse res = handler.errorForPort(504, ctx.listenPort);
                Connection &conn = conIt->second;
                conn.out = res.serialize();
                conn.state = Connection::WRITING_RESPONSE;
            }
            cleanupCgi(ctx.clientFd);
            it = _ctx.begin();
            continue;
        }
        if (ctx.pid > 0)
            ::waitpid(ctx.pid, NULL, WNOHANG);
        ++it;
    }
}

void CgiModule::killForClient(int clientFd)
{
    std::map<int, CgiContext>::iterator it = _ctx.find(clientFd);
    if (it == _ctx.end())
        return;
    if (it->second.pid > 0)
        ::kill(it->second.pid, SIGKILL);
    cleanupCgi(clientFd);
}

bool CgiModule::hasClient(int clientFd) const
{
    return _ctx.find(clientFd) != _ctx.end();
}
