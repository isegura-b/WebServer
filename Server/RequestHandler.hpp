#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include "../config/Config.hpp"
#include "../HTTP/HttpRequest.hpp"
#include "../HTTP/HttpResponse.hpp"
#include <string>
#include <sys/stat.h>

struct CgiJob
{
	enum Decision
	{
		CGI_NO,
		CGI_YES,
		CGI_ERROR
	};
	std::string interpreter;
	std::string scriptFilename;
	std::string scriptName;
	std::string pathInfo;
	std::string queryString;
	std::string cwd;
	std::map<std::string, std::string> env;
	CgiJob() {}
};

class RequestHandler
{
private:
	Config _config;

	const ServerBlock* findBestServer(const HttpRequest& req, int port);
	const LocationBlock* findBestLocation(const ServerBlock* server, const std::string& uri);

	std::string generateAutoindex(const std::string& path, const std::string& requestTarget);
	std::string getContentType(const std::string& path);
	bool isDirectory(const std::string& path);
	bool fileExists(const std::string& path);
	std::string readFile(const std::string& path);

	HttpResponse handleGet(const HttpRequest& req, const LocationBlock* loc, const std::string& fullPath, const ServerBlock* server);
	HttpResponse handlePost(const HttpRequest& req, const LocationBlock* loc, const std::string& fullPath, const ServerBlock* server);
	HttpResponse handleDelete(const HttpRequest& req, const LocationBlock* loc, const std::string& fullPath, const ServerBlock* server);

	HttpResponse generateError(int code, const ServerBlock* server);

public:
	RequestHandler(const Config& cfg);
	HttpResponse handle(const HttpRequest& req, int port);
	CgiJob::Decision buildCgiJob(const HttpRequest& req, int port, CgiJob& job, HttpResponse& errorRes);
	HttpResponse errorForPort(int code, int port);
};

#endif
