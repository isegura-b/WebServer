#ifndef CGIDISPATCHER_HPP
#define CGIDISPATCHER_HPP

#include "../config/Config.hpp"
#include "../HTTP/HttpRequest.hpp"
#include "CgiJob.hpp"

class CgiDispatcher
{
public:
    explicit CgiDispatcher(const Config &cfg);
    CgiJob::Decision buildJob(const HttpRequest &req, int port, CgiJob &job, int &errorCode);

private:
    const Config &_cfg;
    const ServerBlock *findBestServer(const HttpRequest &req, int port) const;
    const LocationBlock *findBestLocation(const ServerBlock *server, const std::string &uri) const;
};

#endif
