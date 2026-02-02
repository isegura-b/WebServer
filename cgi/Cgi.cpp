//#include "Cgi.hpp"
//#include "../Server/Server.hpp"
//#include "../Server/RequestHandler.hpp"
//#include <unistd.h>
//#include <fcntl.h>
//#include <sys/wait.h>
//#include <signal.h>
//#include <sstream>
//#include <vector>
//#include <iostream>
//
//static void setNonBlocking(int fd)
//{
//    int flags = fcntl(fd, F_GETFL, 0);
//    if (flags >= 0)
//        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
//}
//
//static std::string trimCrlf(const std::string &s)
//{
//    if (!s.empty() && s[s.size() - 1] == '\r')
//        return s.substr(0, s.size() - 1);
//    return s;
//}
//
//CgiContext::CgiContext()
//    : clientFd(-1), listenPort(0), stdinWrite(-1), stdoutRead(-1), stderrRead(-1), pid(-1),
//      state(INIT), startTime(0), lastActivity(0), bodySent(0), exitStatus(0)
//{
//}
//
//static bool parseCgiOutput(const std::string &out, HttpResponse &res)
//{
//    if (out.empty())
//        return false;
//
//    size_t sep = out.find("\r\n\r\n");
//    size_t sepLen = 4;
//    if (sep == std::string::npos)
//    {
//        sep = out.find("\n\n");
//        sepLen = 2;
//    }
//    if (sep == std::string::npos)
//        return false;
//
//    std::string headerPart = out.substr(0, sep);
//    std::string bodyPart = out.substr(sep + sepLen);
//
//    std::istringstream iss(headerPart);
//    std::string line;
//    int statusCode = 200;
//    std::string statusMsg = "OK";
//    bool hasContentType = false;
//
//    while (std::getline(iss, line))
//    {
//        line = trimCrlf(line);
//        if (line.empty())
//            continue;
//        size_t p = line.find(':');
//        if (p == std::string::npos)
//            return false;
//        std::string key = line.substr(0, p);
//        std::string val = line.substr(p + 1);
//        while (!val.empty() && (val[0] == ' ' || val[0] == '\t'))
//            val.erase(0, 1);
//
//        if (key == "Status")
//        {
//            std::istringstream ss(val);
//            ss >> statusCode;
//            std::getline(ss, statusMsg);
//            if (!statusMsg.empty() && statusMsg[0] == ' ')
//                statusMsg.erase(0, 1);
//        }
//        else
//        {
//            if (key == "Content-Type")
//                hasContentType = true;
//            res.setHeader(key, val);
//        }
//    }
//
//    res.setStatus(statusCode, statusMsg.empty() ? "OK" : statusMsg);
//    if (!hasContentType)
//        res.setHeader("Content-Type", "text/html");
//    res.setBody(bodyPart);
//    return true;
//}
//
//void CgiModule::start(Connection &c, int port, const CgiJob &job, RequestHandler &handler)
//{
//    int inPipe[2];
//    int outPipe[2];
//    int errPipe[2];
//
//    if (pipe(inPipe) < 0 || pipe(outPipe) < 0 || pipe(errPipe) < 0)
//    {
//        HttpResponse res = handler.errorForPort(500, port);
//        c.out = res.serialize();
//        c.state = Connection::WRITING_RESPONSE;
//        return;
//    }
//
//    pid_t pid = fork();
//    if (pid < 0)
//    {
//        HttpResponse res = handler.errorForPort(500, port);
//        c.out = res.serialize();
//        c.state = Connection::WRITING_RESPONSE;
//        ::close(inPipe[0]); ::close(inPipe[1]);
//        ::close(outPipe[0]); ::close(outPipe[1]);
//        ::close(errPipe[0]); ::close(errPipe[1]);
//        return;
//    }
//
//    if (pid == 0)
//    {
//        ::dup2(inPipe[0], STDIN_FILENO);
//        ::dup2(outPipe[1], STDOUT_FILENO);
//        ::dup2(errPipe[1], STDERR_FILENO);
//
//        ::close(inPipe[0]); ::close(inPipe[1]);
//        ::close(outPipe[0]); ::close(outPipe[1]);
//        ::close(errPipe[0]); ::close(errPipe[1]);
//
//        if (!job.cwd.empty())
//            ::chdir(job.cwd.c_str());
//
//        std::vector<std::string> envStrings;
//        for (std::map<std::string, std::string>::const_iterator it = job.env.begin(); it != job.env.end(); ++it)
//            envStrings.push_back(it->first + "=" + it->second);
//        std::vector<char *> envp;
//        for (size_t i = 0; i < envStrings.size(); ++i)
//            envp.push_back(const_cast<char *>(envStrings[i].c_str()));
//        envp.push_back(NULL);
//
//        std::vector<char *> argv;
//        argv.push_back(const_cast<char *>(job.interpreter.c_str()));
//        argv.push_back(const_cast<char *>(job.scriptFilename.c_str()));
//        argv.push_back(NULL);
//
//        ::execve(job.interpreter.c_str(), &argv[0], &envp[0]);
//        _exit(127);
//    }
//
//    ::close(inPipe[0]);
//    ::close(outPipe[1]);
//    ::close(errPipe[1]);
//
//    setNonBlocking(inPipe[1]);
//    setNonBlocking(outPipe[0]);
//    setNonBlocking(errPipe[0]);
//
//    CgiContext ctx;
//    ctx.clientFd = c.fd;
//    ctx.listenPort = port;
//    ctx.stdinWrite = inPipe[1];
//    ctx.stdoutRead = outPipe[0];
//    ctx.stderrRead = errPipe[0];
//    ctx.pid = pid;
//    ctx.startTime = std::time(NULL);
//    ctx.lastActivity = ctx.startTime;
//    ctx.body = c.req.body;
//    ctx.bodySent = 0;
//    ctx.state = ctx.body.empty() ? CgiContext::READING_OUTPUT : CgiContext::WRITING_BODY;
//
//    _ctx[c.fd] = ctx;
//    c.state = Connection::CGI_IN_PROGRESS;
//    c.lastActivity = ctx.startTime;
//}
//
//void CgiModule::addPollFds(std::vector<struct pollfd> &pfds, std::map<int, CgiFdInfo> &fdMap)
//{
//    for (std::map<int, CgiContext>::iterator it = _ctx.begin(); it != _ctx.end(); ++it)
//    {
//        CgiContext &ctx = it->second;
//        if (ctx.stdinWrite >= 0 && ctx.state == CgiContext::WRITING_BODY)
//        {
//            struct pollfd p;
//            p.fd = ctx.stdinWrite;
//            p.events = POLLOUT;
//            p.revents = 0;
//            pfds.push_back(p);
//            fdMap[p.fd] = CgiFdInfo(ctx.clientFd, CGI_FD_STDIN);
//        }
//        if (ctx.stdoutRead >= 0)
//        {
//            struct pollfd p;
//            p.fd = ctx.stdoutRead;
//            p.events = POLLIN | POLLHUP;
//            p.revents = 0;
//            pfds.push_back(p);
//            fdMap[p.fd] = CgiFdInfo(ctx.clientFd, CGI_FD_STDOUT);
//        }
//        if (ctx.stderrRead >= 0)
//        {
//            struct pollfd p;
//            p.fd = ctx.stderrRead;
//            p.events = POLLIN | POLLHUP;
//            p.revents = 0;
//            pfds.push_back(p);
//            fdMap[p.fd] = CgiFdInfo(ctx.clientFd, CGI_FD_STDERR);
//        }
//    }
//}
//
//void CgiModule::finalizeCgi(int clientFd, bool ok, std::map<int, Connection> &conns, RequestHandler &handler)
//{
//    std::map<int, CgiContext>::iterator it = _ctx.find(clientFd);
//    if (it == _ctx.end())
//        return;
//    CgiContext &ctx = it->second;
//
//    std::map<int, Connection>::iterator cit = conns.find(clientFd);
//    if (cit == conns.end())
//    {
//        cleanupCgi(clientFd);
//        return;
//    }
//    Connection &conn = cit->second;
//
//    if (!ok)
//    {
//        HttpResponse res = handler.errorForPort(500, ctx.listenPort);
//        conn.out = res.serialize();
//        conn.state = Connection::WRITING_RESPONSE;
//        cleanupCgi(clientFd);
//        return;
//    }
//
//    HttpResponse res;
//    if (!parseCgiOutput(ctx.output, res))
//    {
//        std::cerr << "[cgi] parse failed for client " << clientFd
//                  << " stdout size=" << ctx.output.size()
//                  << " stderr size=" << ctx.err.size() << std::endl;
//        if (!ctx.err.empty())
//            std::cerr << "[cgi] stderr: " << ctx.err << std::endl;
//        HttpResponse err = handler.errorForPort(502, ctx.listenPort);
//        conn.out = err.serialize();
//        conn.state = Connection::WRITING_RESPONSE;
//        cleanupCgi(clientFd);
//        return;
//    }
//
//    conn.out = res.serialize();
//    conn.state = Connection::WRITING_RESPONSE;
//    cleanupCgi(clientFd);
//}
//
//void CgiModule::cleanupCgi(int clientFd)
//{
//    std::map<int, CgiContext>::iterator it = _ctx.find(clientFd);
//    if (it == _ctx.end())
//        return;
//    CgiContext &ctx = it->second;
//
//    // Close pipes
//    if (ctx.stdinWrite >= 0) ::close(ctx.stdinWrite);
//    if (ctx.stdoutRead >= 0) ::close(ctx.stdoutRead);
//    if (ctx.stderrRead >= 0) ::close(ctx.stderrRead);
//
//    if (ctx.pid > 0)
//    {
//        int status;
//        if (waitpid(ctx.pid, &status, WNOHANG) == 0) {
//            ::kill(ctx.pid, SIGKILL);
//            waitpid(ctx.pid, &status, 0);
//        }
//    }
//
//    _ctx.erase(it);
//}
//
////void CgiModule::cleanupCgi(int clientFd)
////{
////    std::map<int, CgiContext>::iterator it = _ctx.find(clientFd);
////    if (it == _ctx.end())
////        return;
////    CgiContext &ctx = it->second;
////    if (ctx.stdinWrite >= 0)
////        ::close(ctx.stdinWrite);
////    if (ctx.stdoutRead >= 0)
////        ::close(ctx.stdoutRead);
////    if (ctx.stderrRead >= 0)
////        ::close(ctx.stderrRead);
////    _ctx.erase(it);
////}
//
//void CgiModule::handleEvent(int clientFd, int fdType, short revents, std::map<int, Connection> &conns, RequestHandler &handler)
//{
//    std::map<int, CgiContext>::iterator it = _ctx.find(clientFd);
//    if (it == _ctx.end())
//        return;
//    CgiContext &ctx = it->second;
//    ctx.lastActivity = std::time(NULL);
//    std::map<int, Connection>::iterator cit = conns.find(clientFd);
//    if (cit != conns.end())
//        cit->second.lastActivity = ctx.lastActivity;
//
//    if (revents & (POLLERR | POLLNVAL))
//    {
//        finalizeCgi(clientFd, false, conns, handler);
//        return;
//    }
//
//    if (fdType == CGI_FD_STDIN && (revents & POLLOUT))
//    {
//        if (ctx.stdinWrite >= 0)
//        {
//            size_t remaining = ctx.body.size() - ctx.bodySent;
//            if (remaining > 0)
//            {
//                int w = ::write(ctx.stdinWrite, ctx.body.data() + ctx.bodySent, remaining);
//                if (w > 0)
//                {
//                    ctx.bodySent += static_cast<size_t>(w);
//                }
//                else if (w < 0)
//                {
//                    finalizeCgi(clientFd, false, conns, handler);
//                    return;
//                }
//            }
//            if (ctx.bodySent >= ctx.body.size())
//            {
//                ::close(ctx.stdinWrite);
//                ctx.stdinWrite = -1;
//                if (ctx.state == CgiContext::WRITING_BODY)
//                    ctx.state = CgiContext::READING_OUTPUT;
//            }
//        }
//    }
//
//    if ((fdType == CGI_FD_STDOUT || fdType == CGI_FD_STDERR) && (revents & (POLLIN | POLLHUP)))
//    {
//        int fd = (fdType == CGI_FD_STDOUT) ? ctx.stdoutRead : ctx.stderrRead;
//        if (fd >= 0)
//        {
//            char buf[4096];
//            while (true)
//            {
//                int r = ::read(fd, buf, sizeof(buf));
//                if (r > 0)
//                {
//                    if (fdType == CGI_FD_STDOUT)
//                        ctx.output.append(buf, r);
//                    else
//                        ctx.err.append(buf, r);
//                }
//                else if (r == 0)
//                {
//                    ::close(fd);
//                    if (fdType == CGI_FD_STDOUT)
//                        ctx.stdoutRead = -1;
//                    else
//                        ctx.stderrRead = -1;
//                    break;
//                }
//                else
//                {
//                    finalizeCgi(clientFd, false, conns, handler);
//                    return;
//                }
//            }
//        }
//    }
//
//    if (ctx.stdoutRead < 0)
//    {
//        finalizeCgi(clientFd, true, conns, handler);
//    }
//}
//
//void CgiModule::tick(std::time_t now, std::map<int, Connection> &conns, RequestHandler &handler)
//{
//    std::map<int, CgiContext>::iterator it = _ctx.begin();
//    while (it != _ctx.end())
//    {
//        CgiContext &ctx = it->second;
//
//        if (now - ctx.startTime > 10)
//        {
//            std::map<int, Connection>::iterator conIt = conns.find(ctx.clientFd);
//            if (conIt != conns.end())
//            {
//                HttpResponse res = handler.errorForPort(504, ctx.listenPort);
//                Connection &conn = conIt->second;
//                conn.out = res.serialize();
//                conn.state = Connection::WRITING_RESPONSE;
//            }
//
//            int fdToClean = ctx.clientFd;
//            ++it; 
//            cleanupCgi(fdToClean);
//            continue;
//        }
//
//        if (ctx.pid > 0)
//        {
//            int status;
//            int ret = waitpid(ctx.pid, &status, WNOHANG);
//            if (ret > 0) {
//                ctx.pid = -1; 
//            }
//        }
//        ++it;
//    }
//}
//
////void CgiModule::tick(std::time_t now, std::map<int, Connection> &conns, RequestHandler &handler)
////{
////    std::map<int, CgiContext>::iterator it = _ctx.begin();
////    while (it != _ctx.end())
////    {
////        CgiContext &ctx = it->second;
////        if (now - ctx.startTime > 10)
////        {
////            if (ctx.pid > 0)
////                ::kill(ctx.pid, SIGKILL);
////            std::map<int, Connection>::iterator conIt = conns.find(ctx.clientFd);
////            if (conIt != conns.end())
////            {
////                HttpResponse res = handler.errorForPort(504, ctx.listenPort);
////                Connection &conn = conIt->second;
////                conn.out = res.serialize();
////                conn.state = Connection::WRITING_RESPONSE;
////            }
////            cleanupCgi(ctx.clientFd);
////            it = _ctx.begin();
////            continue;
////        }
////        if (ctx.pid > 0)
////            ::waitpid(ctx.pid, NULL, WNOHANG);
////        ++it;
////    }
////}
//
//void CgiModule::killForClient(int clientFd)
//{
//    std::map<int, CgiContext>::iterator it = _ctx.find(clientFd);
//    if (it == _ctx.end())
//        return;
//    if (it->second.pid > 0)
//        ::kill(it->second.pid, SIGKILL);
//    cleanupCgi(clientFd);
//}
//
//bool CgiModule::hasClient(int clientFd) const
//{
//    return _ctx.find(clientFd) != _ctx.end();
#include "Cgi.hpp"
#include "../Server/Server.hpp"
#include "../Server/RequestHandler.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <sstream>
#include <vector>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdio>

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

    // Detectar fin de cabeceras (\r\n\r\n o \n\n)
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
        if (line.empty()) continue;
        
        size_t p = line.find(':');
        if (p == std::string::npos) continue; // Ignorar líneas mal formadas
        
        std::string key = line.substr(0, p);
        std::string val = line.substr(p + 1);
        
        // Trim leading spaces
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
            if (key == "Content-Type") hasContentType = true;
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
    int inPipe[2];  // Servidor -> CGI
    int outPipe[2]; // CGI -> Servidor
    int errPipe[2]; // CGI -> Servidor (stderr)

    // Fallo en creación de pipes es error de sistema inmediato
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

    if (pid == 0) // Child
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

    // Parent
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
    
    // Si hay body que enviar al CGI, empezamos escribiendo, sino leyendo
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
        
        // Escribir al stdin del CGI
        if (ctx.stdinWrite >= 0 && ctx.state == CgiContext::WRITING_BODY)
        {
            struct pollfd p;
            p.fd = ctx.stdinWrite;
            p.events = POLLOUT;
            p.revents = 0;
            pfds.push_back(p);
            fdMap[p.fd] = CgiFdInfo(ctx.clientFd, CGI_FD_STDIN);
        }
        
        // Leer del stdout del CGI
        if (ctx.stdoutRead >= 0)
        {
            struct pollfd p;
            p.fd = ctx.stdoutRead;
            // POLLHUP es importante para saber cuando el hijo cerró el pipe (terminó)
            p.events = POLLIN | POLLHUP; 
            p.revents = 0;
            pfds.push_back(p);
            fdMap[p.fd] = CgiFdInfo(ctx.clientFd, CGI_FD_STDOUT);
        }
        
        // Leer del stderr del CGI
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

void CgiModule::handleEvent(int clientFd, int fdType, short revents, std::map<int, Connection> &conns, RequestHandler &handler)
{
    std::map<int, CgiContext>::iterator it = _ctx.find(clientFd);
    if (it == _ctx.end())
        return;

    CgiContext &ctx = it->second;
    ctx.lastActivity = std::time(NULL);

    // Actualizar actividad del cliente asociado para que no tenga timeout
    std::map<int, Connection>::iterator cit = conns.find(clientFd);
    if (cit != conns.end())
        cit->second.lastActivity = ctx.lastActivity;

    // 1. Errores fatales en el pipe
    if (revents & (POLLERR | POLLNVAL))
    {
        finalizeCgi(clientFd, false, conns, handler);
        return;
    }

    // 2. Escritura al CGI (Stdin)
    if (fdType == CGI_FD_STDIN && (revents & POLLOUT))
    {
        if (ctx.stdinWrite >= 0)
        {
            size_t remaining = ctx.body.size() - ctx.bodySent;
            if (remaining > 0)
            {
                // Intentamos escribir
                ssize_t w = ::write(ctx.stdinWrite, ctx.body.data() + ctx.bodySent, remaining);
                
                if (w > 0)
                {
                    ctx.bodySent += w;
                }
                else if (w < 0)
                {
                    // Lógica OPTIMISTA:
                    // No chequeamos errno. Asumimos EAGAIN (pipe lleno).
                    // Si el pipe estuviera roto, poll nos daría POLLERR en el sig ciclo.
                    // Simplemente retornamos y esperamos.
                    return; 
                }
            }

            // Si terminamos de enviar todo el body
            if (ctx.bodySent >= ctx.body.size())
            {
                ::close(ctx.stdinWrite);
                ctx.stdinWrite = -1;
                ctx.state = CgiContext::READING_OUTPUT;
            }
        }
    }

    // 3. Lectura del CGI (Stdout o Stderr)
    // POLLHUP indica que el escritor cerró, pero puede haber datos pendientes de leer.
    if ((fdType == CGI_FD_STDOUT || fdType == CGI_FD_STDERR) && (revents & (POLLIN | POLLHUP)))
    {
        int fd = (fdType == CGI_FD_STDOUT) ? ctx.stdoutRead : ctx.stderrRead;
        if (fd >= 0)
        {
            char buf[4096];
            while (true)
            {
                ssize_t r = ::read(fd, buf, sizeof(buf));

                if (r > 0)
                {
                    if (fdType == CGI_FD_STDOUT)
                        ctx.output.append(buf, r);
                    else
                        ctx.err.append(buf, r);
                }
                else if (r == 0)
                {
                    // EOF real. El CGI cerró su salida.
                    ::close(fd);
                    if (fdType == CGI_FD_STDOUT)
                        ctx.stdoutRead = -1;
                    else
                        ctx.stderrRead = -1;
                    break; // Salimos del bucle de lectura
                }
                else
                {
                    // r < 0. Lógica OPTIMISTA:
                    // Asumimos EAGAIN (no hay más datos por ahora).
                    // Dejamos de leer y volvemos al loop principal.
                    break;
                }
            }
        }
    }

    // Si ya cerramos el stdout (EOF recibido), terminamos.
    // Ignoramos si stderr sigue abierto (algunos scripts no lo cierran bien).
    if (ctx.stdoutRead < 0)
    {
        finalizeCgi(clientFd, true, conns, handler);
    }
}

void CgiModule::finalizeCgi(int clientFd, bool ok, std::map<int, Connection> &conns, RequestHandler &handler)
{
    std::map<int, CgiContext>::iterator it = _ctx.find(clientFd);
    if (it == _ctx.end()) return;
    
    CgiContext &ctx = it->second;
    std::map<int, Connection>::iterator cit = conns.find(clientFd);

    if (cit == conns.end())
    {
        // Cliente desconectado mientras CGI corría
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
        // Fallo al parsear headers del CGI (o output vacío)
        std::cerr << "[cgi] Error parsing CGI output or empty output." << std::endl;
        if (!ctx.err.empty())
             std::cerr << "[cgi] Stderr content: " << ctx.err << std::endl;
        
        HttpResponse err = handler.errorForPort(502, ctx.listenPort); // 502 Bad Gateway
        conn.out = err.serialize();
        conn.state = Connection::WRITING_RESPONSE;
        cleanupCgi(clientFd);
        return;
    }

    // Éxito
    conn.out = res.serialize();
    conn.state = Connection::WRITING_RESPONSE;
    cleanupCgi(clientFd);
}

void CgiModule::cleanupCgi(int clientFd)
{
    std::map<int, CgiContext>::iterator it = _ctx.find(clientFd);
    if (it == _ctx.end()) return;

    CgiContext &ctx = it->second;

    // Cerrar pipes abiertos
    if (ctx.stdinWrite >= 0) ::close(ctx.stdinWrite);
    if (ctx.stdoutRead >= 0) ::close(ctx.stdoutRead);
    if (ctx.stderrRead >= 0) ::close(ctx.stderrRead);

    // Matar proceso si sigue vivo
    if (ctx.pid > 0)
    {
        int status;
        // Check rápido si ya murió
        if (waitpid(ctx.pid, &status, WNOHANG) == 0) {
            ::kill(ctx.pid, SIGKILL); // Hard kill para asegurar
            waitpid(ctx.pid, &status, 0); // Reap
        }
    }

    _ctx.erase(it);
}

void CgiModule::tick(std::time_t now, std::map<int, Connection> &conns, RequestHandler &handler)
{
    std::map<int, CgiContext>::iterator it = _ctx.begin();
    while (it != _ctx.end())
    {
        CgiContext &ctx = it->second;

        // Timeout del CGI (ej: 10 segundos)
        if (now - ctx.startTime > 10)
        {
            std::cout << "[cgi] Timeout for client " << ctx.clientFd << std::endl;
            
            std::map<int, Connection>::iterator conIt = conns.find(ctx.clientFd);
            if (conIt != conns.end())
            {
                HttpResponse res = handler.errorForPort(504, ctx.listenPort); // 504 Gateway Timeout
                conIt->second.out = res.serialize();
                conIt->second.state = Connection::WRITING_RESPONSE;
            }

            // Guardamos ID y avanzamos iterador antes de borrar
            int fdToClean = ctx.clientFd;
            ++it; 
            cleanupCgi(fdToClean);
            continue;
        }

        // Reap de zombies (por si terminaron antes de tiempo)
        if (ctx.pid > 0)
        {
            int status;
            int ret = waitpid(ctx.pid, &status, WNOHANG);
            if (ret > 0) {
                // Proceso terminó.
                ctx.pid = -1; 
                // No finalizamos aquí, esperamos a leer todo el pipe (EOF) en handleEvent
            }
        }
        ++it;
    }
}

void CgiModule::killForClient(int clientFd)
{
    std::map<int, CgiContext>::iterator it = _ctx.find(clientFd);
    if (it == _ctx.end()) return;
    
    // Matar proceso hijo
    if (it->second.pid > 0) {
        ::kill(it->second.pid, SIGKILL);
        waitpid(it->second.pid, NULL, 0); // Evitar zombie
    }
    
    // Cerrar pipes
    if (it->second.stdinWrite >= 0) ::close(it->second.stdinWrite);
    if (it->second.stdoutRead >= 0) ::close(it->second.stdoutRead);
    if (it->second.stderrRead >= 0) ::close(it->second.stderrRead);

    _ctx.erase(it);
}

bool CgiModule::hasClient(int clientFd) const
{
    return _ctx.find(clientFd) != _ctx.end();
}
