/* Defines constants and declares handler functions
   for the ChatLink Concurrent TCP server */
#ifndef SERVER_CONCURRENT_TCP_H
#define SERVER_CONCURRENT_TCP_H

#define SERVER_PORT 8888
#define BUFFER_SIZE 4096
#define BACKLOG 5
#define MAX_REPLY 16384

/* Request handler functions */
void handleRegister(int clientSock, char *args);
void handleLogin(int clientSock, char *args);
void handleLoadUsers(int clientSock, char *args);
void handleSendMessage(int clientSock, char *args);
void handleLoadMessages(int clientSock, char *args);
void handleSearchUser(int clientSock, char *args);
void handleDeleteUser(int clientSock, char *args);

/* Sends a reply back to the client */
void sendReply(int clientSock, const char *reply);

/* Slave process function — handles all requests from one client */
void handleClient(int clientSock);

#endif