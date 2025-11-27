#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include "Config.hpp"
#include <string>

class ConfigParser {
public:
    // Parse config file into Config. Throws std::runtime_error on errors.
    Config parse(const std::string &path);
private:
    static std::string trim(const std::string &s);
};

#endif // CONFIG_PARSER_HPP
