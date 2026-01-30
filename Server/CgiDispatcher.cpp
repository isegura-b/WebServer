#include "CgiDispatcher.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>

static bool isMethodAllowed(const LocationBlock *loc, const std::string &method)
{
    for (size_t i = 0; i < loc->allowedMethods.size(); ++i)
    {
        if (loc->allowedMethods[i] == method)
            return true;
    }
    return false;
}

static bool fileExists(const std::string &path)
{
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

static bool isDirectory(const std::string &path)
{
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0)
        return false;
    return S_ISDIR(buffer.st_mode);
}

static std::string toUpperUnderscore(const std::string &s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
        if (c == '-')
            c = '_';
        out.push_back(c);
    }
    return out;
}

static std::string toStringSize(size_t v)
{
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

static void splitPathQuery(const std::string &rawPath, std::string &pathOnly, std::string &query)
{
    size_t qpos = rawPath.find('?');
    if (qpos == std::string::npos)
    {
        pathOnly = rawPath;
        query.clear();
    }
    else
    {
        pathOnly = rawPath.substr(0, qpos);
        query = rawPath.substr(qpos + 1);
    }
}

static std::string dirnameOf(const std::string &path)
{
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos)
        return ".";
    if (pos == 0)
        return "/";
    return path.substr(0, pos);
}

static std::string makeAbsolutePath(const std::string &path)
{
    if (!path.empty() && path[0] == '/')
        return path;
    char buf[4096];
    if (::getcwd(buf, sizeof(buf)) == NULL)
        return path;
    std::string base(buf);
    if (!base.empty() && base[base.size() - 1] != '/')
        base += "/";
    return base + path;
}

static bool matchCgiExtension(const std::map<std::string, std::string> &cgiPass,
                              const std::string &uriPath,
                              std::string &ext,
                              size_t &extPos)
{
    size_t lastSlash = uriPath.find_last_of('/');
    if (lastSlash == std::string::npos)
        lastSlash = 0;
    else
        lastSlash += 1;
    size_t bestLen = 0;
    size_t bestPos = std::string::npos;
    std::string bestExt;
    for (std::map<std::string, std::string>::const_iterator it = cgiPass.begin(); it != cgiPass.end(); ++it)
    {
        const std::string &e = it->first;
        size_t pos = uriPath.find(e, lastSlash);
        if (pos == std::string::npos)
            continue;
        if (pos < lastSlash)
            continue;
        if (e.size() > bestLen)
        {
            bestLen = e.size();
            bestPos = pos;
            bestExt = e;
        }
    }
    if (bestPos == std::string::npos)
        return false;
    ext = bestExt;
    extPos = bestPos;
    return true;
}

CgiDispatcher::CgiDispatcher(const Config &cfg) : _cfg(cfg)
{
}

const ServerBlock *CgiDispatcher::findBestServer(const HttpRequest &req, int port) const
{
    (void)req;
    for (size_t i = 0; i < _cfg.servers.size(); ++i)
    {
        if (_cfg.servers[i].listenPort == port)
            return &_cfg.servers[i];
    }
    return NULL;
}

const LocationBlock *CgiDispatcher::findBestLocation(const ServerBlock *server, const std::string &uri) const
{
    const LocationBlock *best = NULL;
    size_t bestLen = 0;
    for (size_t i = 0; i < server->locations.size(); ++i)
    {
        const std::string &route = server->locations[i].path;
        if (uri.find(route) == 0)
        {
            if (route.size() > bestLen)
            {
                best = &server->locations[i];
                bestLen = route.size();
            }
        }
    }
    return best;
}

CgiJob::Decision CgiDispatcher::buildJob(const HttpRequest &req, int port, CgiJob &job, int &errorCode)
{
    errorCode = 0;
    const ServerBlock *server = findBestServer(req, port);
    if (!server)
    {
        errorCode = 400;
        return CgiJob::CGI_ERROR;
    }

    std::string cleanPath;
    std::string query;
    splitPathQuery(req.path, cleanPath, query);

    const LocationBlock *loc = findBestLocation(server, cleanPath);
    if (!loc)
    {
        errorCode = 404;
        return CgiJob::CGI_ERROR;
    }

    if (!isMethodAllowed(loc, req.method))
    {
        errorCode = 405;
        return CgiJob::CGI_ERROR;
    }

    if (server->clientMaxBodySize > 0 && req.body.size() > server->clientMaxBodySize)
    {
        errorCode = 413;
        return CgiJob::CGI_ERROR;
    }

    if (loc->cgiPass.empty())
        return CgiJob::CGI_NO;

    std::string ext;
    size_t extPos = std::string::npos;
    if (!matchCgiExtension(loc->cgiPass, cleanPath, ext, extPos))
        return CgiJob::CGI_NO;

    std::string scriptName = cleanPath.substr(0, extPos + ext.size());
    std::string pathInfo = "";
    if (cleanPath.size() > extPos + ext.size())
        pathInfo = cleanPath.substr(extPos + ext.size());

    std::string docRoot = loc->root.empty() ? server->root : loc->root;
    std::string scriptFilename = docRoot + scriptName;
    scriptFilename = makeAbsolutePath(scriptFilename);

    if (!fileExists(scriptFilename))
    {
        errorCode = 404;
        return CgiJob::CGI_ERROR;
    }
    if (isDirectory(scriptFilename))
    {
        errorCode = 403;
        return CgiJob::CGI_ERROR;
    }

    std::map<std::string, std::string>::const_iterator it = loc->cgiPass.find(ext);
    if (it == loc->cgiPass.end() || it->second.empty())
    {
        errorCode = 500;
        return CgiJob::CGI_ERROR;
    }

    job.interpreter = it->second;
    job.scriptFilename = scriptFilename;
    job.scriptName = scriptName;
    job.pathInfo = pathInfo;
    job.queryString = query;
    job.cwd = dirnameOf(scriptFilename);

    job.env.clear();
    job.env["REQUEST_METHOD"] = req.method;
    job.env["QUERY_STRING"] = query;
    job.env["SCRIPT_NAME"] = scriptName;
    job.env["SCRIPT_FILENAME"] = scriptFilename;
    if (!pathInfo.empty())
        job.env["PATH_INFO"] = pathInfo;
    job.env["SERVER_PROTOCOL"] = req.version;
    job.env["GATEWAY_INTERFACE"] = "CGI/1.1";
    if (!server->serverName.empty())
        job.env["SERVER_NAME"] = server->serverName;
    else
        job.env["SERVER_NAME"] = "localhost";
    job.env["SERVER_PORT"] = toStringSize(static_cast<size_t>(server->listenPort));

    if (req.headers.count("Content-Type"))
        job.env["CONTENT_TYPE"] = req.headers.find("Content-Type")->second;
    if (!req.body.empty())
        job.env["CONTENT_LENGTH"] = toStringSize(req.body.size());

    for (std::map<std::string, std::string>::const_iterator hit = req.headers.begin(); hit != req.headers.end(); ++hit)
    {
        std::string key = toUpperUnderscore(hit->first);
        job.env["HTTP_" + key] = hit->second;
    }

    return CgiJob::CGI_YES;
}
