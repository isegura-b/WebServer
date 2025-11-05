#include "Server.hpp"

// _______________________________NO SE SI ES NECESARIO CAMBIAR EL PUERT
//_______________________________ESTA QUI

Server::Server()
    : SimpleServer(AF_INET, SOCK_STREAM, 0, 8080, INADDR_ANY, 10), clientSocket(-1)
{
    launch();
}

Server::~Server() 
{
}

void    Server::accept()
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // esperar a una conexión de cliente
    clientSocket = ::accept(getServerSocket()->getSocket(), (struct sockaddr *)&client_addr, &client_len);
    if (clientSocket < 0)
    {
        perror("Error accepting connection");
        exit(EXIT_FAILURE);
    }

    ssize_t r = ::read(clientSocket, buffer, sizeof(buffer) - 1); // leer datos del cliente
    if (r < 0)
    {
        perror("Error reading from client");
        ::close(clientSocket);
        return;
    }
    buffer[r] = '\0'; // asegurar terminación
}

void    Server::handle()
{
    std::cout << buffer << std::endl; //print the buffer received from client
}

void   Server::respond()
{
    const char *httpResponse = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello, World!";
    write(clientSocket, httpResponse, strlen(httpResponse)); //send a simple HTTP response
    close(clientSocket); //close the client socket after responding
}

void   Server::launch()
{
    // Mostrar URL/IP de acceso
    struct sockaddr_in addr = getServerSocket()->getAddress();
    int port = ntohs(addr.sin_port);
    std::cout << "Accede en: http://localhost:" << port << "/" << std::endl;

    while (1)
    {
        std::cout << "=======Waiting for connections...=======" << std::endl;
        accept();
        handle();
        respond();
        std::cout << "=======Response sent, connection closed.=======" << std::endl;
    }
}