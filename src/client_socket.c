/* Implements the client-side TCP socket connection to the ChatLink server */
#include "../include/client_socket.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

static int clientSocket = -1;

bool ConnectToServer(void)
{
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0)
    {
        printf("[CLIENT] socket() failed.\n");
        return false;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(clientSocket,
                (struct sockaddr *)&serverAddr,
                sizeof(serverAddr)) < 0)
    {
        printf("[CLIENT] connect() failed. Is the server running?\n");
        close(clientSocket);
        clientSocket = -1;
        return false;
    }

    printf("[CLIENT] Connected to server at %s:%d\n", SERVER_IP, SERVER_PORT);
    return true;
}

void DisconnectFromServer(void)
{
    if (clientSocket != -1)
    {
        close(clientSocket);
        clientSocket = -1;
    }
}

bool SendRequest(const char *request, char *replyBuf, int replyBufSize)
{
    if (clientSocket == -1)
        return false;

    char msgBuf[BUFFER_SIZE];
    snprintf(msgBuf, sizeof(msgBuf), "%s\n", request);

    if (send(clientSocket, msgBuf, strlen(msgBuf), 0) < 0)
    {
        printf("[CLIENT] send() failed.\n");
        return false;
    }

    memset(replyBuf, 0, replyBufSize);
    int totalReceived = 0;

    while (totalReceived < replyBufSize - 1)
    {
        int bytesReceived = recv(clientSocket,
                                 replyBuf + totalReceived,
                                 replyBufSize - totalReceived - 1,
                                 0);
        if (bytesReceived <= 0)
            break;

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