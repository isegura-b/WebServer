Webserv - HTTP/1.1 Server
📖 Overview

Webserv is a custom HTTP/1.1 server written in C++98. It is designed to be non-blocking and event-driven, utilizing poll() (or epoll/select) to handle multiple concurrent client connections efficiently without relying on threading.

This project recreates the core functionality of established web servers like NGINX or Apache, focusing on socket manipulation, the HTTP request/response cycle, and CGI execution.
✨ Features

    Non-blocking I/O: Uses a single thread with multiplexing to handle multiple connections simultaneously.

    HTTP/1.1 Support: Parses and processes standard HTTP requests.

    Supported Methods:

        GET: Retrieve resources.

        POST: Upload data/files.

        DELETE: Remove resources.

    CGI Support: Executes scripts (Python, PHP, etc.) via Common Gateway Interface.

    Configuration: Highly configurable via a .conf file (inspired by NGINX).

        Multi-port listening.

        Virtual server hosting (Host header routing).

        Custom error pages.

        Client body size limits.

        HTTP Redirection.

        Directory listing (Auto-index).

    Robustness: Handles large file uploads (chunked transfer encoding) and stress testing (Siege).

🛠️ Compilation & Usage
Prerequisites

    c++ or g++ compiler.

    make.

Build

To compile the project, run:
Bash

make

Run

Start the server by providing a configuration file. If no file is provided, it attempts to load a default path.
Bash

./webserv [path/to/config_file.conf]

Example:
Bash

./webserv config/default.conf
