#include "RequestHandler.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio> 
#include <unistd.h>

static bool isMethodAllowed(const LocationBlock* loc, const std::string& method) {
	for (size_t i = 0; i < loc->allowedMethods.size(); ++i) {
		if (loc->allowedMethods[i] == method)
			return true;
	}
	return false;
}

RequestHandler::RequestHandler(const Config& cfg) : _config(cfg) {
}

HttpResponse RequestHandler::handle(const HttpRequest& req, int port) {
	const ServerBlock* server = findBestServer(req, port);

	if (!server) {
		std::cout << "No server found for port " << port << std::endl;
		return generateError(400);
	}

	const LocationBlock* loc = findBestLocation(server, req.path);
	if (!loc) {
		std::cout << "No location found for path: " << req.path << std::endl;
		return generateError(404);
	}

	if (!isMethodAllowed(loc, req.method)) {
		return generateError(405);
	}

	if (server->clientMaxBodySize > 0 && req.body.size() > server->clientMaxBodySize) {
		return generateError(413);
	}
	std::string docRoot = loc->root;
	if (docRoot.empty()) {
		docRoot = server->root; // If loc has no root, use the server root
	}

	std::string fullPath = docRoot + req.path;

	if (req.method == "GET") return handleGet(req, loc, fullPath);
	if (req.method == "POST") return handlePost(req, loc, fullPath);
	if (req.method == "DELETE") return handleDelete(req, loc, fullPath);

	return generateError(501);
}

// GET

HttpResponse RequestHandler::handleGet(const HttpRequest& req, const LocationBlock* loc, const std::string& path) {
	HttpResponse res;
	std::string finalPath = path;

	if (isDirectory(finalPath)) {
		if (!loc->index.empty() && fileExists(finalPath + "/" + loc->index)) {
			finalPath += "/" + loc->index;
		}
		else if (loc->autoindex) {
			std::string html = generateAutoindex(finalPath, req.path);

			if (html.empty()) 
				return generateError(500); 

			res.setBody(html);
			res.setHeader("Content-Type", "text/html");
			res.setStatus(200, "OK");

			return res;
		}
		else {
			return generateError(403); 
		}
	}

	if (!fileExists(finalPath)) 
		return generateError(404);

	try {
		std::string content = readFile(finalPath);
		res.setBody(content);
		res.setHeader("Content-Type", getContentType(finalPath));
		res.setStatus(200, "OK");
	}
	catch (...) {
		return generateError(500); // Error interno al leer
	}
	return res;
}

// POST

HttpResponse RequestHandler::handlePost(const HttpRequest& req, const LocationBlock* loc, const std::string& path)
{
	(void)path;
	std::string targetDir = loc->uploadStore.empty() ? loc->root : loc->uploadStore;

	std::string fileName = req.path.substr(req.path.find_last_of("/") + 1);
	if (fileName.empty()) 
		fileName = "uploaded_file";

	std::string savePath = targetDir + "/" + fileName;

	std::ofstream file(savePath.c_str(), std::ios::binary);
	if (!file) 
		return generateError(500);

	file << req.body;
	file.close();

	HttpResponse res;
	res.setStatus(201, "Created");
	res.setBody("File uploaded successfully");
	return res;
}

// DELETE

HttpResponse RequestHandler::handleDelete(const HttpRequest& req, const LocationBlock* loc, const std::string& path)
{
	(void)req; (void)loc; 
	HttpResponse res;

	if (!fileExists(path))
		return generateError(404);

	if (std::remove(path.c_str()) != 0) 
		return generateError(500); 

	res.setStatus(204, "No Content"); 
	return res;
}

const ServerBlock* RequestHandler::findBestServer(const HttpRequest& req, int port) {
	(void)req; 

	for (size_t i = 0; i < _config.servers.size(); ++i) {
		if (_config.servers[i].listenPort == port)
			return &_config.servers[i];
	}
	return NULL;
}

const LocationBlock* RequestHandler::findBestLocation(const ServerBlock* server, const std::string& uri) {
    // Longest prefix match
	const LocationBlock* best = NULL;
	size_t bestLen = 0;

	for (size_t i = 0; i < server->locations.size(); ++i) {
		const std::string& route = server->locations[i].path;
		if (uri.find(route) == 0) { 
			if (route.size() > bestLen) {
				best = &server->locations[i];
				bestLen = route.size();
			}
		}
	}
	return best;
}

std::string RequestHandler::readFile(const std::string& path) {
	std::ifstream f(path.c_str(), std::ios::in | std::ios::binary);

	if (!f)
		throw std::runtime_error("Error");

	std::stringstream ss;
	ss << f.rdbuf();

	return ss.str();
}

bool RequestHandler::fileExists(const std::string& path) {
	struct stat buffer;

	return (stat(path.c_str(), &buffer) == 0);
}

bool RequestHandler::isDirectory(const std::string& path) {
	struct stat buffer;

	if (stat(path.c_str(), &buffer) != 0)
		return false;

	return S_ISDIR(buffer.st_mode);
}

// Checks the extension and returns the aproppiate string

std::string RequestHandler::getContentType(const std::string& path) {
	if (path.find(".html") != std::string::npos) return "text/html";
	if (path.find(".css") != std::string::npos) return "text/css";
	if (path.find(".xml") != std::string::npos) return "text/xml";
	if (path.find(".gif") != std::string::npos) return "image/gif";
	if (path.find(".jpeg") != std::string::npos) return "image/jpeg";
	if (path.find(".jpg") != std::string::npos) return "image/jpeg";
	if (path.find(".js") != std::string::npos) return "application/javascript";
	if (path.find(".txt") != std::string::npos) return "text/plain";
	if (path.find(".png") != std::string::npos) return "image/png";
	if (path.find(".ico") != std::string::npos) return "image/x-icon";
	if (path.find(".json") != std::string::npos) return "application/json";
	if (path.find(".pdf") != std::string::npos) return "application/pdf";
	if (path.find(".zip") != std::string::npos) return "application/zip";
	if (path.find(".mp3") != std::string::npos) return "audio/mpeg";
	if (path.find(".mp4") != std::string::npos) return "video/mp4";
	return "text/plain"; // Default 
}

HttpResponse RequestHandler::generateError(int code) {
	HttpResponse res;
	res.setStatus(code, "Error");

	std::stringstream ss;
	ss << code;

	res.setBody("<html><body><h1>Error " + ss.str() + "</h1></body></html>");
	res.setHeader("Content-Type", "text/html");
	return res;
}

std::string RequestHandler::generateAutoindex(const std::string& path, const std::string& requestTarget) {
	std::string html = "<html><body><h1>Index of " + requestTarget + "</h1><ul>";

	DIR* dir = opendir(path.c_str());
	if (dir == NULL) {
		return ""; 
	}

	struct dirent* entry;

	while ((entry = readdir(dir)) != NULL) {
		std::string name = entry->d_name;
		if (name == ".") 
			continue;

		std::string href = requestTarget;
		if (href.empty() || href[href.size() - 1] != '/') 
			href += "/";
		href += name;

		html += "<li><a href=\"" + href + "\">" + name + "</a></li>";
	}

	closedir(dir);

	html += "</ul></body></html>";
	return html;
}
