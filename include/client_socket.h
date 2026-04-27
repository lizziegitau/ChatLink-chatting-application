/* Declares the socket connection helpers used by the client side.*/
#ifndef CLIENT_SOCKET_H
#define CLIENT_SOCKET_H

#include <stdbool.h>

#define SERVER_IP "127.0.0.1" /* localhost — both client and server run in WSL */
#define SERVER_PORT 8888
#define BUFFER_SIZE 4096

bool ConnectToServer(void);
void DisconnectFromServer(void);
bool SendRequest(const char *request, char *replyBuf, int replyBufSize);

#endif