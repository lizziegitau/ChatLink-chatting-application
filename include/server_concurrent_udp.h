/* Defines constants and declares handler functions
   for the ChatLink Concurrent UDP server */
#ifndef SERVER_CONCURRENT_UDP_H
#define SERVER_CONCURRENT_UDP_H

#include <netinet/in.h>

#define SERVER_PORT_UDP 8889
#define BUFFER_SIZE 4096
#define MAX_REPLY 16384

/* Request handler functions */
void handleRegister_UDP(int sock, struct sockaddr_in *clientAddr,
                        socklen_t addrLen, char *args);
void handleLogin_UDP(int sock, struct sockaddr_in *clientAddr,
                     socklen_t addrLen, char *args);
void handleLoadUsers_UDP(int sock, struct sockaddr_in *clientAddr,
                         socklen_t addrLen, char *args);
void handleSendMessage_UDP(int sock, struct sockaddr_in *clientAddr,
                           socklen_t addrLen, char *args);
void handleLoadMessages_UDP(int sock, struct sockaddr_in *clientAddr,
                            socklen_t addrLen, char *args);
void handleSearchUser_UDP(int sock, struct sockaddr_in *clientAddr,
                          socklen_t addrLen, char *args);
void handleDeleteUser_UDP(int sock, struct sockaddr_in *clientAddr,
                          socklen_t addrLen, char *args);

/* Sends reply back to client using sendto() */
void sendReply_UDP(int sock, struct sockaddr_in *clientAddr,
                   socklen_t addrLen, const char *reply);

#endif