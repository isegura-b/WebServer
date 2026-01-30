#ifndef CGIJOB_HPP
#define CGIJOB_HPP

#include <string>
#include <map>

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

#endif
