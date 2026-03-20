/* Declares the socket connection helpers used by the client side.*/
#ifndef CLIENT_SOCKET_H
#define CLIENT_SOCKET_H

#include <stdbool.h>

#define SERVER_IP "127.0.0.1" /* Server runs on localhost */
#define SERVER_PORT 8888      /* Port both sides must agree on */
#define BUFFER_SIZE 4096      /* Max size of a single message */

/* Connects to the server*/
bool ConnectToServer(void);

/* Disconnects from the server. Call on app exit. */
void DisconnectFromServer(void);

/* Sends a request string to the server and waits for a reply. Writes the server's reply into replyBuf */
bool SendRequest(const char *request, char *replyBuf, int replyBufSize);

#endif