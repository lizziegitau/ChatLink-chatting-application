/* Implements the client-side UDP socket for the ChatLink UDP version */
#include "../include/client_socket_udp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

/* The single UDP socket reused for every request */
static int udpSocket = -1;
static struct sockaddr_in serverAddr;

/* Creates the UDP socket. No connect() — connectionless, no handshake */
bool InitUDPSocket(void)
{
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket < 0)
    {
        printf("[UDP CLIENT] socket() failed.\n");
        return false;
    }

    /* 5-second receive timeout using struct timeval (Linux style) */
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO,
               &timeout, sizeof(timeout));

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT_UDP);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP_UDP);

    printf("[UDP CLIENT] UDP socket ready. Server: %s:%d\n",
           SERVER_IP_UDP, SERVER_PORT_UDP);
    return true;
}

/* Closes the UDP socket */
void CloseUDPSocket(void)
{
    if (udpSocket != -1)
    {
        close(udpSocket);
        udpSocket = -1;
    }
}

/* Sends one request packet and waits for the reply */
bool SendRequestUDP(const char *request, char *replyBuf, int replyBufSize)
{
    if (udpSocket == -1)
        return false;

    char msgBuf[BUFFER_SIZE];
    snprintf(msgBuf, sizeof(msgBuf), "%s\n", request);

    int sent = sendto(udpSocket, msgBuf, strlen(msgBuf), 0,
                      (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (sent < 0)
    {
        printf("[UDP CLIENT] sendto() failed.\n");
        return false;
    }

    memset(replyBuf, 0, replyBufSize);
    int totalReceived = 0;

    while (totalReceived < replyBufSize - 1)
    {
        struct sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr); /* socklen_t, not int */

        int bytesReceived = recvfrom(udpSocket,
                                     replyBuf + totalReceived,
                                     replyBufSize - totalReceived - 1,
                                     0,
                                     (struct sockaddr *)&fromAddr,
                                     &fromLen);
        if (bytesReceived <= 0)
        {
            printf("[UDP CLIENT] recvfrom() timed out or failed.\n");
            break;
        }

        totalReceived += bytesReceived;
        replyBuf[totalReceived] = '\0';

        if (strstr(replyBuf, "END\n") != NULL)
            break;

        if (strchr(replyBuf, '\n') != NULL &&
            strncmp(replyBuf, "MSG:", 4) != 0 &&
            strncmp(replyBuf, "USER:", 5) != 0)
            break;
    }

    replyBuf[totalReceived] = '\0';
    return totalReceived > 0;
}

/* Wrapper so auth.c, chat.c and search.c are unchanged */
bool SendRequest(const char *request, char *replyBuf, int replyBufSize)
{
    return SendRequestUDP(request, replyBuf, replyBufSize);
}