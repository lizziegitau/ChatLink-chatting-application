/* Client-side UDP socket helpers */
#ifndef CLIENT_SOCKET_UDP_H
#define CLIENT_SOCKET_UDP_H

#include <stdbool.h>

#define SERVER_IP_UDP "192.168.100.73" /* Replace with your server IP */
#define SERVER_PORT_UDP 8889
#define BUFFER_SIZE 4096

/* Initialises the UDP socket. Call once at startup */
bool InitUDPSocket(void);

/* Closes the UDP socket. Call on app exit */
void CloseUDPSocket(void);

/* Sends a request and waits for a reply. One shot as there is no connection is made with the server before sending the request */
bool SendRequestUDP(const char *request, char *replyBuf, int replyBufSize);

#endif
