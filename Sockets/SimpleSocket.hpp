#ifndef SIMPLESOCKET_HPP
#define SIMPLESOCKET_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

class SimpleSocket 
{
private:
    int sock;                   //el fd del socket principal
    int connection;             //el fd del socket de conexion
    struct sockaddr_in address; 

public:
    //Constructor
    SimpleSocket(int domain, int type, int protocol, int port, u_long interface);
    SimpleSocket(const SimpleSocket &other);
    SimpleSocket &operator=(const SimpleSocket &other);
    virtual ~SimpleSocket();


    virtual int NetworkConnection(int sock, struct sockaddr_in address) = 0; // Pure virtual function for connection handling (Server/Client)
    // Getters
    struct sockaddr_in getAddress();
    int getSocket();
    int getConnection();
    // Setters
    void setConnection(int connection);
};

#endif
/* 
Guardar la información de una dirección IPv4
struct sockaddr_in {
    short int          sin_family;  // Address family, AF_INET
    unsigned short int sin_port;    // Port number
    struct in_addr     sin_addr;    // Internet address
    unsigned char      sin_zero[8]; // Same size as struct sockaddr
};
*/