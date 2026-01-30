#include "RequestHandler.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio> 
#include <unistd.h>

static void splitPathQuery(const std::string& rawPath, std::string& pathOnly, std::string& query)
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
		return generateError(400, NULL);
	}

	std::string cleanPath;
	std::string query;
	splitPathQuery(req.path, cleanPath, query);

	const LocationBlock* loc = findBestLocation(server, cleanPath);
	if (!loc) {
		std::cout << "No location found for path: " << req.path << std::endl;
		return generateError(404, server);
	}

	if (!isMethodAllowed(loc, req.method)) {
		return generateError(405, server);
	}

	if (server->clientMaxBodySize > 0 && req.body.size() > server->clientMaxBodySize) {
		return generateError(413, server);
	}
	std::string docRoot = loc->root;
	if (docRoot.empty()) {
		docRoot = server->root; // If loc has no root, use the server root
	}

	std::string fullPath = docRoot + cleanPath;

	if (req.method == "GET") return handleGet(req, loc, fullPath, server);
	if (req.method == "POST") return handlePost(req, loc, fullPath, server);
	if (req.method == "DELETE") return handleDelete(req, loc, fullPath, server);

	return generateError(501, server);
}

// GET

HttpResponse RequestHandler::handleGet(const HttpRequest& req, const LocationBlock* loc, const std::string& path, const ServerBlock* server) {
	HttpResponse res;
	std::string finalPath = path;

	if (isDirectory(finalPath)) {
		if (!loc->index.empty() && fileExists(finalPath + "/" + loc->index)) {
			finalPath += "/" + loc->index;
		}
		else if (loc->autoindex) {
			std::string html = generateAutoindex(finalPath, req.path);

			if (html.empty()) 
				return generateError(500, server); 

			res.setBody(html);
			res.setHeader("Content-Type", "text/html");
			res.setStatus(200, "OK");

			return res;
		}
		else {
			return generateError(403, server); 
		}
	}

	if (!fileExists(finalPath)) 
		return generateError(404, server);

	try {
		std::string content = readFile(finalPath);
		res.setBody(content);
		res.setHeader("Content-Type", getContentType(finalPath));
		res.setStatus(200, "OK");
	}
	catch (...) {
		return generateError(500, server); // Error interno al leer
	}
	return res;
}

// POST

HttpResponse RequestHandler::handlePost(const HttpRequest& req, const LocationBlock* loc, const std::string& path, const ServerBlock* server)
{
	(void)path;
	std::string targetDir = loc->uploadStore.empty() ? loc->root : loc->uploadStore;

	std::string cleanPath;
	std::string query;
	splitPathQuery(req.path, cleanPath, query);
	std::string fileName = cleanPath.substr(cleanPath.find_last_of("/") + 1);
	if (fileName.empty()) 
		fileName = "uploaded_file";

	std::string savePath = targetDir + "/" + fileName;

	std::ofstream file(savePath.c_str(), std::ios::binary);
	if (!file) 
		return generateError(500, server);

	file << req.body;
	file.close();

	HttpResponse res;
	res.setStatus(201, "Created");
	res.setBody("File uploaded successfully");
	return res;
}

HttpResponse RequestHandler::errorForPort(int code, int port)
{
	HttpRequest dummy;
	const ServerBlock* server = findBestServer(dummy, port);
	return generateError(code, server);
}

// DELETE

HttpResponse RequestHandler::handleDelete(const HttpRequest& req, const LocationBlock* loc, const std::string& path, const ServerBlock* server)
{
	(void)req; (void)loc; 
	HttpResponse res;

	if (!fileExists(path))
		return generateError(404, server);

	if (std::remove(path.c_str()) != 0) 
		return generateError(500, server); 

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

HttpResponse RequestHandler::generateError(int code, const ServerBlock* server) {
	HttpResponse res;
	res.setStatus(code, "Error");

	if (server) {
		std::map<int, std::string>::const_iterator it = server->errorPages.find(code);
		if (it != server->errorPages.end()) {
			const std::string& errorPath = it->second;
			if (fileExists(errorPath)) {
				try {
					std::string content = readFile(errorPath);
					res.setBody(content);
					res.setHeader("Content-Type", getContentType(errorPath));
					return res;
				}
				catch (...) {
				}
			}
		}
	}

	std::stringstream ss;
	ss << code;

	res.setBody("<html><body><h1>Error " + ss.str() + "</h1></body></html>");
	res.setHeader("Content-Type", "text/html");
	return res;
}

std::string RequestHandler::generateAutoindex(const std::string& path,
                                             const std::string& requestTarget)
{
    std::string html;

    html += "<!DOCTYPE html><html lang=\"es\"><head>";
    html += "<meta charset=\"utf-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>Index of " + requestTarget + "</title>";

    html += "<style>";
    html += "html,body{height:100%;}";
    html += "body{margin:0;min-height:100vh;padding:24px;"
            "font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;"
            "color:rgba(255,255,255,.92);"
            "background-color:#0b1020;"
            "background-image:"
            "radial-gradient(900px 500px at 10% 10%, rgba(91,195,255,.25), transparent 55%),"
            "radial-gradient(700px 500px at 90% 80%, rgba(176,91,255,.2), transparent 55%);"
            "background-repeat:no-repeat;"
            "background-attachment:fixed;"
            "background-size:cover;"
            "}";

    html += ".card{max-width:900px;margin:24px auto 0 auto;padding:20px;"
            "background:linear-gradient(180deg, rgba(255,255,255,.08), rgba(255,255,255,.04));"
            "border:1px solid rgba(255,255,255,.12);border-radius:18px;"
            "box-shadow:0 20px 60px rgba(0,0,0,.35);}";

    html += "h1{margin:0 0 6px;font-size:28px;}";
    html += "p{margin:0 0 16px;color:rgba(255,255,255,.65);}";

    html += "ul{list-style:none;margin:0;padding:0;display:grid;gap:12px;}";

    html += "a.item{display:flex;justify-content:space-between;align-items:center;"
            "padding:12px 14px;border-radius:14px;text-decoration:none;"
            "color:inherit;border:1px solid rgba(255,255,255,.12);"
            "background:rgba(255,255,255,.04);"
            "transition:transform .15s ease,background .15s ease,border-color .15s ease;}";

    html += "a.item:hover{transform:translateY(-1px);"
            "background:rgba(255,255,255,.08);"
            "border-color:rgba(255,255,255,.2);}";

    html += ".left{display:flex;gap:10px;align-items:center;}";

    html += ".tag{font-size:12px;color:rgba(255,255,255,.65);"
            "border:1px solid rgba(255,255,255,.12);"
            "padding:4px 8px;border-radius:999px;}";

    html += "</style></head><body>";

    html += "<div class=\"card\">";
    html += "<h1>📂 " + requestTarget + "</h1>";
    html += "<ul>";

    DIR* dir = opendir(path.c_str());
    if (!dir)
        return "";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string name = entry->d_name;

        if (name == "." || name == "..")
            continue;

        std::string href = requestTarget;
        if (href.empty() || href[href.size() - 1] != '/')
            href += "/";
        href += name;

        std::string diskPath = path;
        if (!diskPath.empty() && diskPath[diskPath.size() - 1] != '/')
            diskPath += "/";
        diskPath += name;

        bool isDir = isDirectory(diskPath);

        std::string icon = "📄";
        if (isDir)
            icon = "📁";
        else if (name.size() >= 4 &&
                (name.rfind(".png") != std::string::npos ||
                 name.rfind(".jpg") != std::string::npos ||
                 name.rfind(".jpeg") != std::string::npos ||
                 name.rfind(".gif") != std::string::npos))
            icon = "🖼️";

        std::string tag = isDir ? "DIR" : "FILE";

        html += "<li>";
        html += "<a class=\"item\" href=\"" + href + "\">";
        html += "<span class=\"left\">" + icon + " " + name + "</span>";
        html += "<span class=\"tag\">" + tag + "</span>";
        html += "</a>";
        html += "</li>";
    }

    closedir(dir);

    html += "</ul>";
    html += "</div></body></html>";

    return html;
}
