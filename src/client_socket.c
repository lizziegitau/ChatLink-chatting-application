/* Implements the client-side socket connection to the ChatLink server */
#include "../include/client_socket.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>

/* The single persistent socket connection to the server */
static SOCKET clientSocket = INVALID_SOCKET;

/* Initialises Winsock, creates a socket and connects to the server */
bool ConnectToServer(void)
{
    WSADATA wsaData;

    /* Initialise Winsock which is required on Windows before any socket call */
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("[CLIENT] WSAStartup failed.\n");
        return false;
    }

    /* Create a TCP socket */
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET)
    {
        printf("[CLIENT] socket() failed.\n");
        WSACleanup();
        return false;
    }

    /* Fill in the server address, localhost on SERVER_PORT */
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    /* Connect to the server */
    if (connect(clientSocket,
                (struct sockaddr *)&serverAddr,
                sizeof(serverAddr)) == SOCKET_ERROR)
    {
        printf("[CLIENT] connect() failed. Is the server running?\n");
        closesocket(clientSocket);
        WSACleanup();
        clientSocket = INVALID_SOCKET;
        return false;
    }

    printf("[CLIENT] Connected to server at %s:%d\n", SERVER_IP, SERVER_PORT);
    return true;
}

/* Closes the socket and cleans up Winsock */
void DisconnectFromServer(void)
{
    if (clientSocket != INVALID_SOCKET)
    {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
    WSACleanup();
}

/* Sends a request to the server and reads the reply */
bool SendRequest(const char *request, char *replyBuf, int replyBufSize)
{
    if (clientSocket == INVALID_SOCKET)
        return false;

    /* Build the request with a newline terminator */
    char msgBuf[BUFFER_SIZE];
    snprintf(msgBuf, sizeof(msgBuf), "%s\n", request);

    /* Send the request */
    if (send(clientSocket, msgBuf, (int)strlen(msgBuf), 0) == SOCKET_ERROR)
    {
        printf("[CLIENT] send() failed.\n");
        return false;
    }

    /* Read the reply */
    memset(replyBuf, 0, replyBufSize);
    int totalReceived = 0;

    /* Keep receiving until we get the full reply ending with END\n or a single-line reply ending with \n */
    while (totalReceived < replyBufSize - 1)
    {
        int bytesReceived = recv(clientSocket,
                                 replyBuf + totalReceived,
                                 replyBufSize - totalReceived - 1,
                                 0);

        if (bytesReceived <= 0)
            break; /* Connection closed or error */
        totalReceived += bytesReceived;
        replyBuf[totalReceived] = '\0';

        /* Multi-line reply — wait for END\n which server always sends last */
        if (strstr(replyBuf, "END\n") != NULL)
            break;

        /* Single-line reply — has a \n and does NOT start with MSG: or USER: */
        if (strchr(replyBuf, '\n') != NULL &&
            strncmp(replyBuf, "MSG:", 4) != 0 &&
            strncmp(replyBuf, "USER:", 5) != 0)
            break;
    }

    replyBuf[totalReceived] = '\0';
    return totalReceived > 0;
}