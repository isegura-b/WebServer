Webserv - HTTP/1.1 Server  
=
*This project has been created as part of the 42 curriculum
by isegura-, aprenafe, cmanica-*

📖 Overview
-

Webserv is a custom HTTP/1.1 server written in C++98. It is designed to be non-blocking and event-driven, utilizing poll() (or epoll/select) to handle multiple concurrent client connections efficiently without relying on threading.

This project recreates the core functionality of established web servers like NGINX or Apache, focusing on socket manipulation, 
the HTTP request/response cycle, and CGI execution.  

✨ Features
-

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
-

Prerequisites:

    c++ or g++ compiler.
    make.

Build
-

To compile the project, run make.

Start the server by providing a configuration file. If no file is provided, it attempts to load a default path.

./webserv [path/to/config_file.conf]

Example:

./webserv config/default.conf

🏗️ Architecture
-

The server follows a Reactor Pattern architecture:

    Initialization: Parses the config file and sets up listening sockets.
    Event Loop: Uses poll() to monitor file descriptors for events.
    Accepting: New connections are accepted and added to the monitored set.
    Handling:
        Request Parsing: Reads raw bytes from the socket and constructs an HttpRequest object.
        Processing: The Server logic routes the request to static file handling or CGI execution.
        Response Generation: Builds an HttpResponse object (headers + body).
    Sending: The response is written back to the socket in non-blocking chunks.

🧪 Testing
-

Simple Request
-

You can test the server using curl or a web browser:

curl -v http://localhost:8080/index.html

File Upload (POST)
-

curl -X POST -F "file=@/path/to/image.jpg" http://localhost:8080/upload

Stress Testing (Siege)
-

To ensure the server doesn't crash under load and has no memory leaks:

siege -b -t 30S http://localhost:8080/

📂 Project Structure
-
    src/: Main entry point.
    Sockets/: Socket creation, binding, and listening logic.
    Server/: Server loop, connection handling, and request routing.
    HTTP/: Parsing requests (headers, chunked body) and formatting responses.
    cgi/: Environment setup and execve logic for scripts.
    config/: Configuration file tokenizer and parser.
