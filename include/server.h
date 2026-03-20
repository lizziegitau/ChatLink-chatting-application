/* Defines constants and declares handler functions for the ChatLink server */
#ifndef SERVER_H
#define SERVER_H

#define SERVER_PORT 8888 /* Port the server listens on */
#define BUFFER_SIZE 4096 /* Max request/reply buffer size */
#define BACKLOG 5        /* Max pending connections in listen queue */

/* Request handler functions where each handles one type of client request */
void handleRegister(int clientSock, char *args);
void handleLogin(int clientSock, char *args);
void handleLoadUsers(int clientSock, char *args);
void handleSendMessage(int clientSock, char *args);
void handleLoadMessages(int clientSock, char *args);
void handleSearchUser(int clientSock, char *args);
void handleDeleteUser(int clientSock, char *args);

/* Sends a simple string reply back to the client */
void sendReply(int clientSock, const char *reply);

#endif