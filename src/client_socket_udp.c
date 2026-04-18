/* Implements the client-side UDP socket for the ChatLink UDP version */
#include "../include/client_socket_udp.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>

/* The single UDP socket that is reused for every request */
static SOCKET udpSocket = INVALID_SOCKET;
static struct sockaddr_in serverAddr;

/* Initialises Winsock and creates the UDP socket. No connect() as it is connectionless meaning no handshake */
bool InitUDPSocket(void)
{
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("[UDP CLIENT] WSAStartup failed.\n");
        return false;
    }

    /* Create a UDP socket */
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0); /* SOCK_DGRAM = UDP */
    if (udpSocket == INVALID_SOCKET)
    {
        printf("[UDP CLIENT] socket() failed.\n");
        WSACleanup();
        return false;
    }

    /* Set a receive timeout of 5 seconds so recvfrom() does not block forever if the server is unreachable */
    DWORD timeout = 5000;
    setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO,
               (char *)&timeout, sizeof(timeout));

    /* Fill in server address which is used by every sendto() call */
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT_UDP);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP_UDP);

    printf("[UDP CLIENT] UDP socket ready. Server: %s:%d\n",
           SERVER_IP_UDP, SERVER_PORT_UDP);
    return true;
}

/* Closes the UDP socket and cleans up Winsock */
void CloseUDPSocket(void)
{
    if (udpSocket != INVALID_SOCKET)
    {
        closesocket(udpSocket);
        udpSocket = INVALID_SOCKET;
    }
    WSACleanup();
}

/* Sends one request packet and waits for one reply packet. This is the core of connectionless communication where sendto() fires the request at the server and recvfrom() waitsfor the reply to come back from the server */
bool SendRequestUDP(const char *request, char *replyBuf, int replyBufSize)
{
    if (udpSocket == INVALID_SOCKET)
        return false;

    /* Build request with newline terminator */
    char msgBuf[BUFFER_SIZE];
    snprintf(msgBuf, sizeof(msgBuf), "%s\n", request);

    /* sendto(): send request packet to server  */
    int sent = sendto(udpSocket, msgBuf, (int)strlen(msgBuf), 0,
                      (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (sent == SOCKET_ERROR)
    {
        printf("[UDP CLIENT] sendto() failed.\n");
        return false;
    }

    /* recvfrom(): wait for reply packet from server */
    memset(replyBuf, 0, replyBufSize);
    int totalReceived = 0;

    /* UDP can fragment large replies into multiple packets. So keep receiving until we get END\n or a single-line reply */
    while (totalReceived < replyBufSize - 1)
    {
        struct sockaddr_in fromAddr;
        int fromLen = sizeof(fromAddr);

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

        /* For a multi-line reply, wait for END\n */
        if (strstr(replyBuf, "END\n") != NULL)
            break;

        /* For a single-line reply, wait for \n */
        if (strchr(replyBuf, '\n') != NULL &&
            strncmp(replyBuf, "MSG:", 4) != 0 &&
            strncmp(replyBuf, "USER:", 5) != 0)
            break;
    }

    replyBuf[totalReceived] = '\0';
    return totalReceived > 0;
}

/* Wrapper so auth.c, chat.c and search.c work is unchanged. SendRequest() calls now go through UDP instead of TCP */
bool SendRequest(const char *request, char *replyBuf, int replyBufSize)
{
    return SendRequestUDP(request, replyBuf, replyBufSize);
}
