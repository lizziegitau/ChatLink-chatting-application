#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/server.h"
#include "../include/fileio.h"

/* Sends a string back to the connected client */
void sendReply(int clientSock, const char *reply)
{
    send(clientSock, reply, (int)strlen(reply), 0);
}

/* REGISTER:username:password */
void handleRegister(int clientSock, char *args)
{
    char username[MAX_USERNAME] = {0};
    char password[MAX_PASSWORD] = {0};

    /* Parse username and password from "username:password" */
    char *colon = strchr(args, ':');
    if (!colon)
    {
        sendReply(clientSock, "FAIL:EMPTY_FIELDS\n");
        return;
    }
    *colon = '\0';
    strncpy(username, args, MAX_USERNAME - 1);
    strncpy(password, colon + 1, MAX_PASSWORD - 1);

    /* Remove trailing newline from password if present */
    password[strcspn(password, "\n")] = '\0';

    if (strlen(username) == 0 || strlen(password) == 0)
    {
        sendReply(clientSock, "FAIL:EMPTY_FIELDS\n");
        return;
    }

    if (UserExists(username))
    {
        sendReply(clientSock, "FAIL:USERNAME_TAKEN\n");
        return;
    }

    User newUser;
    strncpy(newUser.username, username, MAX_USERNAME - 1);
    strncpy(newUser.password, password, MAX_PASSWORD - 1);
    SaveUser(&newUser);

    printf("[SERVER] Registered new user: %s\n", username);
    sendReply(clientSock, "OK\n");
}

/* LOGIN:username:password */
void handleLogin(int clientSock, char *args)
{
    char username[MAX_USERNAME] = {0};
    char password[MAX_PASSWORD] = {0};

    char *colon = strchr(args, ':');
    if (!colon)
    {
        sendReply(clientSock, "FAIL:INVALID_CREDENTIALS\n");
        return;
    }
    *colon = '\0';
    strncpy(username, args, MAX_USERNAME - 1);
    strncpy(password, colon + 1, MAX_PASSWORD - 1);
    password[strcspn(password, "\n")] = '\0';

    if (ValidateLogin(username, password))
    {
        printf("[SERVER] Login success: %s\n", username);
        sendReply(clientSock, "OK\n");
    }
    else
    {
        printf("[SERVER] Login failed: %s\n", username);
        sendReply(clientSock, "FAIL:INVALID_CREDENTIALS\n");
    }
}

/* LOADUSERS:currentUser, returns all users except currentUser */
void handleLoadUsers(int clientSock, char *args)
{
    char currentUser[MAX_USERNAME] = {0};
    strncpy(currentUser, args, MAX_USERNAME - 1);
    currentUser[strcspn(currentUser, "\n")] = '\0';

    User users[MAX_USERS];
    int count = 0;
    LoadUsers(users, &count);

    /* Send each user on its own line, skip the requesting user */
    char reply[BUFFER_SIZE] = {0};
    for (int i = 0; i < count; i++)
    {
        if (strcmp(users[i].username, currentUser) != 0)
        {
            char line[MAX_USERNAME + 10];
            snprintf(line, sizeof(line), "USER:%s\n", users[i].username);
            strncat(reply, line, BUFFER_SIZE - strlen(reply) - 1);
        }
    }
    strncat(reply, "END\n", BUFFER_SIZE - strlen(reply) - 1);
    sendReply(clientSock, reply);
}

/* SENDMSG:sender:recipient:content:HH:MM */
void handleSendMessage(int clientSock, char *args)
{
    Message msg;
    memset(&msg, 0, sizeof(msg));

    /* Parse sender */
    char *p = strtok(args, ":");
    if (!p)
    {
        sendReply(clientSock, "FAIL\n");
        return;
    }
    strncpy(msg.sender, p, MAX_USERNAME - 1);

    /* Parse recipient */
    p = strtok(NULL, ":");
    if (!p)
    {
        sendReply(clientSock, "FAIL\n");
        return;
    }
    strncpy(msg.recipient, p, MAX_USERNAME - 1);

    /* Parse content */
    p = strtok(NULL, ":");
    if (!p)
    {
        sendReply(clientSock, "FAIL\n");
        return;
    }
    strncpy(msg.content, p, MAX_MESSAGE - 1);

    /* Parse timestamp (HH:MM which are two parts separated by a colon (:)) */
    char *hh = strtok(NULL, ":");
    char *mm = strtok(NULL, ":\n");
    if (!hh || !mm)
    {
        sendReply(clientSock, "FAIL\n");
        return;
    }
    snprintf(msg.timestamp, sizeof(msg.timestamp), "%s:%s", hh, mm);

    SaveMessage(&msg);
    printf("[SERVER] Message saved: %s -> %s\n", msg.sender, msg.recipient);
    sendReply(clientSock, "OK\n");
}

/* LOADMSGS:userA:userB which returns all messages between the two users */
void handleLoadMessages(int clientSock, char *args)
{
    char userA[MAX_USERNAME] = {0};
    char userB[MAX_USERNAME] = {0};

    char *colon = strchr(args, ':');
    if (!colon)
    {
        sendReply(clientSock, "END\n");
        return;
    }
    *colon = '\0';
    strncpy(userA, args, MAX_USERNAME - 1);
    strncpy(userB, colon + 1, MAX_USERNAME - 1);
    userB[strcspn(userB, "\n")] = '\0';

    Message messages[MAX_MESSAGES];
    int count = 0;
    LoadMessages(messages, &count);

    char reply[BUFFER_SIZE * 4] = {0}; /* A larger buffer for many messages */

    for (int i = 0; i < count; i++)
    {
        bool mine = strcmp(messages[i].sender, userA) == 0 &&
                    strcmp(messages[i].recipient, userB) == 0;
        bool theirs = strcmp(messages[i].sender, userB) == 0 &&
                      strcmp(messages[i].recipient, userA) == 0;

        if (mine || theirs)
        {
            char line[MAX_MESSAGE + 100];
            snprintf(line, sizeof(line), "MSG:%s:%s:%s:%s\n",
                     messages[i].sender,
                     messages[i].recipient,
                     messages[i].content,
                     messages[i].timestamp);
            strncat(reply, line,
                    sizeof(reply) - strlen(reply) - 1);
        }
    }
    strncat(reply, "END\n", sizeof(reply) - strlen(reply) - 1);
    sendReply(clientSock, reply);
}

/* SEARCHUSER:query:currentUser */
void handleSearchUser(int clientSock, char *args)
{
    char query[MAX_USERNAME] = {0};
    char currentUser[MAX_USERNAME] = {0};

    char *colon = strchr(args, ':');
    if (!colon)
    {
        sendReply(clientSock, "NOTFOUND\n");
        return;
    }
    *colon = '\0';
    strncpy(query, args, MAX_USERNAME - 1);
    strncpy(currentUser, colon + 1, MAX_USERNAME - 1);
    currentUser[strcspn(currentUser, "\n")] = '\0';

    /* Cannot search for yourself */
    if (strcmp(query, currentUser) == 0)
    {
        sendReply(clientSock, "NOTFOUND\n");
        return;
    }

    if (UserExists(query))
    {
        char reply[MAX_USERNAME + 10];
        snprintf(reply, sizeof(reply), "FOUND:%s\n", query);
        sendReply(clientSock, reply);
    }
    else
    {
        sendReply(clientSock, "NOTFOUND\n");
    }
}

/* DELETEUSER:username */
void handleDeleteUser(int clientSock, char *args)
{
    char username[MAX_USERNAME] = {0};
    strncpy(username, args, MAX_USERNAME - 1);
    username[strcspn(username, "\n")] = '\0';

    if (DeleteUser(username))
    {
        printf("[SERVER] Deleted user: %s\n", username);
        sendReply(clientSock, "OK\n");
    }
    else
    {
        sendReply(clientSock, "FAIL\n");
    }
}

/* Called for each accepted connection. Reads requests in a loop until the client disconnects */
void handleClient(SOCKET clientSock)
{
    char buffer[BUFFER_SIZE];

    printf("[SERVER] Client connected. Waiting for requests...\n");

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));

        /* Block until a request arrives */
        int bytesReceived = recv(clientSock, buffer,
                                 sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0)
        {
            /* Client disconnected */
            printf("[SERVER] Client disconnected.\n");
            break;
        }

        buffer[bytesReceived] = '\0';
        printf("[SERVER] Received: %s", buffer);

        /* Parse the command, everything before the first ':' */
        char *colon = strchr(buffer, ':');
        char command[32] = {0};
        char *args = NULL;

        if (colon)
        {
            int cmdLen = (int)(colon - buffer);
            if (cmdLen < 32)
                strncpy(command, buffer, cmdLen);
            args = colon + 1;
        }
        else
        {
            /* Command with no arguments */
            strncpy(command, buffer, 31);
            command[strcspn(command, "\n")] = '\0';
            args = buffer + strlen(command);
        }

        /* Route to the correct handler */
        if (strcmp(command, "REGISTER") == 0)
            handleRegister(clientSock, args);
        else if (strcmp(command, "LOGIN") == 0)
            handleLogin(clientSock, args);
        else if (strcmp(command, "LOADUSERS") == 0)
            handleLoadUsers(clientSock, args);
        else if (strcmp(command, "SENDMSG") == 0)
            handleSendMessage(clientSock, args);
        else if (strcmp(command, "LOADMSGS") == 0)
            handleLoadMessages(clientSock, args);
        else if (strcmp(command, "SEARCHUSER") == 0)
            handleSearchUser(clientSock, args);
        else if (strcmp(command, "DELETEUSER") == 0)
            handleDeleteUser(clientSock, args);
        else
        {
            printf("[SERVER] Unknown command: %s\n", command);
            sendReply(clientSock, "FAIL:UNKNOWN_COMMAND\n");
        }
    }

    closesocket(clientSock);
}

/* Server entry point */
int main(void)
{
    WSADATA wsaData;

    /* Initialise Winsock */
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("[SERVER] WSAStartup failed.\n");
        return 1;
    }

    /* Create the listening socket */
    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock == INVALID_SOCKET)
    {
        printf("[SERVER] socket() failed.\n");
        WSACleanup();
        return 1;
    }

    /* Bind to all interfaces on SERVER_PORT */
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(serverSock,
             (struct sockaddr *)&serverAddr,
             sizeof(serverAddr)) == SOCKET_ERROR)
    {
        printf("[SERVER] bind() failed. Port %d may be in use.\n",
               SERVER_PORT);
        closesocket(serverSock);
        WSACleanup();
        return 1;
    }

    /* Start listening and queue up to BACKLOG pending connections */
    if (listen(serverSock, BACKLOG) == SOCKET_ERROR)
    {
        printf("[SERVER] listen() failed.\n");
        closesocket(serverSock);
        WSACleanup();
        return 1;
    }

    printf("[SERVER] ChatLink Server started on port %d\n", SERVER_PORT);
    printf("[SERVER] Waiting for connections...\n");
    printf("[SERVER] Press Ctrl+C to stop.\n\n");

    /* An iterative accept loop that accepts one client at a time, handles all their requests then loop back to accept the next client */
    while (1)
    {
        struct sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);

        /* Block here until a client connects, ACCEPT REQUEST */
        SOCKET clientSock = accept(serverSock,
                                   (struct sockaddr *)&clientAddr,
                                   &clientAddrLen);
        if (clientSock == INVALID_SOCKET)
        {
            printf("[SERVER] accept() failed.\n");
            continue;
        }

        printf("[SERVER] Accepted connection from %s\n",
               inet_ntoa(clientAddr.sin_addr));

        /* Handle all requests from this client, PROCESS REQUEST */
        handleClient(clientSock);

        /* Client done so loop back to accept next connection */
        printf("[SERVER] Ready for next client.\n\n");
    }

    closesocket(serverSock);
    WSACleanup();
    return 0;
}