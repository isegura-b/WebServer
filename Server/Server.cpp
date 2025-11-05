#include "Server.hpp"
#include <fcntl.h>
#include <unistd.h>

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

void Server::accept()
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // receive connection (::accept from sys/socket.h)
    clientSocket = ::accept(getServerSocket()->getSocket(), (struct sockaddr *)&client_addr, &client_len);
    if (clientSocket < 0)
    {
        perror("Error accepting connection");
        exit(EXIT_FAILURE);
    }

    int flags = fcntl(clientSocket, F_GETFL, 0);
    fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK); // Set non-blocking mode(no wait when nothing))

    int r = ::read(clientSocket, buffer, sizeof(buffer) - 1);
    if (r <= 0)
    {
        buffer[0] = '\0';
        return;
    }

    buffer[r] = '\0';
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

void Server::launch()
{
    struct sockaddr_in addr = getServerSocket()->getAddress();
    int port = ntohs(addr.sin_port);

    struct pollfd pfd;
    pfd.fd = getServerSocket()->getSocket();
    pfd.events = POLLIN; //Wait for incoming connections


    while (1)
    {
        std::cout << "\nLink : http://localhost:" << port << "/" << std::endl;

        std::cout << "Waiting" << std::flush;
        int waitedMs = 0;
        const int dotTimer = 1000;
        const int timeoutMs = 5000;

        while (waitedMs < timeoutMs)
        {
            int ret = poll(&pfd, 1, dotTimer);
            if (ret < 0)
            {
                perror("poll error");
                break;
            }
            if (ret == 0)
            {
                std::cout << "." << std::flush;
                waitedMs += dotTimer;
                continue;
            }

            break;
        }

        std::cout << std::endl;


        if (pfd.revents & POLLIN)
        {
            std::cout << "[+] Nueva conexión detectada" << std::endl;
            accept();
            handle();
            respond();
        }
        else
        {
            std::cout << "[-] No hay conexiones nuevas" << std::endl;
        }
    }
}

/*
void Server::launch()
{
    struct sockaddr_in addr = getServerSocket()->getAddress();
    int port = ntohs(addr.sin_port);

    struct pollfd pfd;
    pfd.fd = getServerSocket()->getSocket();
    pfd.events = POLLIN; //Wait for incoming connections

    while (1)
    {
        std::cout << "\nLink : http://localhost:" << port << "/" << std::endl;
        std::cout << "Waiting..." << std::endl;

        int ret = poll(&pfd, 1, 5000); // timeout = 5s

        if (ret < 0)
        {
            perror("poll error");
            continue;
        }
        else if (ret == 0)
        {
            // if no events, continue the loop
            std::cout << "There are no connections..." << std::endl;
            continue;
        }

        if (pfd.revents & POLLIN)
        {
            accept();
            handle();
            respond();
        }
    }
}*/