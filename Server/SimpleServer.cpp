#include "SimpleServer.hpp"

SimpleServer::SimpleServer(int domain, int service, int protocol, int port, u_long interface, int bklg)
{
    ServerSocket = new ListeningSocket(domain, service, protocol, port, interface, bklg);
}//create a ListeningSocket with the parameters given for be a server socket

SimpleServer::~SimpleServer()
{
    delete ServerSocket;
}

ListeningSocket  *SimpleServer::getServerSocket()
{ 
    return ServerSocket;
}