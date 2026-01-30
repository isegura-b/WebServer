#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include "../config/Config.hpp"
#include "../HTTP/HttpRequest.hpp"
#include "../HTTP/HttpResponse.hpp"
#include "../cgi/CgiJob.hpp"
#include <string>
#include <sys/stat.h>

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
	HttpResponse errorForPort(int code, int port);
};

#endif
