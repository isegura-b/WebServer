#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include "Config.hpp"
#include <string>

class ConfigParser
{
public:
    Config parse(const std::string &path);

private:
    static std::string trim(const std::string &s);
};

#endif
