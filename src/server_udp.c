#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/server_udp.h"
#include "../include/fileio.h"

/* sendReply_UDP function sends a string back to the client using sendto() — the UDP equivalent of send() */
void sendReply_UDP(SOCKET sock, struct sockaddr_in *clientAddr,
                   int addrLen, const char *reply)
{
    sendto(sock, reply, (int)strlen(reply), 0,
           (struct sockaddr *)clientAddr, addrLen);
}

/* Handler for registration requests */
void handleRegister_UDP(SOCKET sock, struct sockaddr_in *clientAddr,
                        int addrLen, char *args)
{
    char username[MAX_USERNAME] = {0};
    char password[MAX_PASSWORD] = {0};

    char *colon = strchr(args, ':');
    if (!colon)
    {
        sendReply_UDP(sock, clientAddr, addrLen, "FAIL:EMPTY_FIELDS\n");
        return;
    }
    *colon = '\0';
    strncpy(username, args, MAX_USERNAME - 1);
    strncpy(password, colon + 1, MAX_PASSWORD - 1);
    password[strcspn(password, "\n")] = '\0';

    if (strlen(username) == 0 || strlen(password) == 0)
    {
        sendReply_UDP(sock, clientAddr, addrLen, "FAIL:EMPTY_FIELDS\n");
        return;
    }

    if (UserExists(username))
    {
        sendReply_UDP(sock, clientAddr, addrLen, "FAIL:USERNAME_TAKEN\n");
        return;
    }

    User newUser;
    strncpy(newUser.username, username, MAX_USERNAME - 1);
    strncpy(newUser.password, password, MAX_PASSWORD - 1);
    SaveUser(&newUser);

    printf("[UDP SERVER] Registered new user: %s\n", username);
    sendReply_UDP(sock, clientAddr, addrLen, "OK\n");
}

/* Handler for login requests */
void handleLogin_UDP(SOCKET sock, struct sockaddr_in *clientAddr,
                     int addrLen, char *args)
{
    char username[MAX_USERNAME] = {0};
    char password[MAX_PASSWORD] = {0};

    char *colon = strchr(args, ':');
    if (!colon)
    {
        sendReply_UDP(sock, clientAddr, addrLen, "FAIL:INVALID_CREDENTIALS\n");
        return;
    }
    *colon = '\0';
    strncpy(username, args, MAX_USERNAME - 1);
    strncpy(password, colon + 1, MAX_PASSWORD - 1);
    password[strcspn(password, "\n")] = '\0';

    if (ValidateLogin(username, password))
    {
        printf("[UDP SERVER] Login success: %s\n", username);
        sendReply_UDP(sock, clientAddr, addrLen, "OK\n");
    }
    else
    {
        printf("[UDP SERVER] Login failed: %s\n", username);
        sendReply_UDP(sock, clientAddr, addrLen, "FAIL:INVALID_CREDENTIALS\n");
    }
}

/* Handler for loading users except the current user */
void handleLoadUsers_UDP(SOCKET sock, struct sockaddr_in *clientAddr,
                         int addrLen, char *args)
{
    char currentUser[MAX_USERNAME] = {0};
    strncpy(currentUser, args, MAX_USERNAME - 1);
    currentUser[strcspn(currentUser, "\n")] = '\0';

    User users[MAX_USERS];
    int count = 0;
    LoadUsers(users, &count);

    char reply[MAX_REPLY] = {0};
    for (int i = 0; i < count; i++)
    {
        if (strcmp(users[i].username, currentUser) != 0)
        {
            char line[MAX_USERNAME + 10];
            snprintf(line, sizeof(line), "USER:%s\n", users[i].username);
            strncat(reply, line, MAX_REPLY - strlen(reply) - 1);
        }
    }
    strncat(reply, "END\n", MAX_REPLY - strlen(reply) - 1);
    sendReply_UDP(sock, clientAddr, addrLen, reply);
}

/* Handler for sending messages */
void handleSendMessage_UDP(SOCKET sock, struct sockaddr_in *clientAddr,
                           int addrLen, char *args)
{
    Message msg;
    memset(&msg, 0, sizeof(msg));

    char *p = strtok(args, ":");
    if (!p)
    {
        sendReply_UDP(sock, clientAddr, addrLen, "FAIL\n");
        return;
    }
    strncpy(msg.sender, p, MAX_USERNAME - 1);

    p = strtok(NULL, ":");
    if (!p)
    {
        sendReply_UDP(sock, clientAddr, addrLen, "FAIL\n");
        return;
    }
    strncpy(msg.recipient, p, MAX_USERNAME - 1);

    p = strtok(NULL, ":");
    if (!p)
    {
        sendReply_UDP(sock, clientAddr, addrLen, "FAIL\n");
        return;
    }
    strncpy(msg.content, p, MAX_MESSAGE - 1);

    char *hh = strtok(NULL, ":");
    char *mm = strtok(NULL, ":\n");
    if (!hh || !mm)
    {
        sendReply_UDP(sock, clientAddr, addrLen, "FAIL\n");
        return;
    }
    snprintf(msg.timestamp, sizeof(msg.timestamp), "%s:%s", hh, mm);

    SaveMessage(&msg);
    printf("[UDP SERVER] Message saved: %s -> %s\n", msg.sender, msg.recipient);
    sendReply_UDP(sock, clientAddr, addrLen, "OK\n");
}

/* Handler for loading messages between two users */
void handleLoadMessages_UDP(SOCKET sock, struct sockaddr_in *clientAddr,
                            int addrLen, char *args)
{
    char userA[MAX_USERNAME] = {0};
    char userB[MAX_USERNAME] = {0};

    char *colon = strchr(args, ':');
    if (!colon)
    {
        sendReply_UDP(sock, clientAddr, addrLen, "END\n");
        return;
    }
    *colon = '\0';
    strncpy(userA, args, MAX_USERNAME - 1);
    strncpy(userB, colon + 1, MAX_USERNAME - 1);
    userB[strcspn(userB, "\n")] = '\0';

    Message messages[MAX_MESSAGES];
    int count = 0;
    LoadMessages(messages, &count);

    char reply[MAX_REPLY * 4] = {0};
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
            strncat(reply, line, sizeof(reply) - strlen(reply) - 1);
        }
    }
    strncat(reply, "END\n", sizeof(reply) - strlen(reply) - 1);
    sendReply_UDP(sock, clientAddr, addrLen, reply);
}

/* Handler for searching for users by username. A user cannot search for themselves */
void handleSearchUser_UDP(SOCKET sock, struct sockaddr_in *clientAddr,
                          int addrLen, char *args)
{
    char query[MAX_USERNAME] = {0};
    char currentUser[MAX_USERNAME] = {0};

    char *colon = strchr(args, ':');
    if (!colon)
    {
        sendReply_UDP(sock, clientAddr, addrLen, "NOTFOUND\n");
        return;
    }
    *colon = '\0';
    strncpy(query, args, MAX_USERNAME - 1);
    strncpy(currentUser, colon + 1, MAX_USERNAME - 1);
    currentUser[strcspn(currentUser, "\n")] = '\0';

    if (strcmp(query, currentUser) == 0)
    {
        sendReply_UDP(sock, clientAddr, addrLen, "NOTFOUND\n");
        return;
    }

    if (UserExists(query))
    {
        char reply[MAX_USERNAME + 10];
        snprintf(reply, sizeof(reply), "FOUND:%s\n", query);
        sendReply_UDP(sock, clientAddr, addrLen, reply);
    }
    else
    {
        sendReply_UDP(sock, clientAddr, addrLen, "NOTFOUND\n");
    }
}

/* Handler for deleting a user */
void handleDeleteUser_UDP(SOCKET sock, struct sockaddr_in *clientAddr,
                          int addrLen, char *args)
{
    char username[MAX_USERNAME] = {0};
    strncpy(username, args, MAX_USERNAME - 1);
    username[strcspn(username, "\n")] = '\0';

    if (DeleteUser(username))
    {
        printf("[UDP SERVER] Deleted user: %s\n", username);
        sendReply_UDP(sock, clientAddr, addrLen, "OK\n");
    }
    else
    {
        sendReply_UDP(sock, clientAddr, addrLen, "FAIL\n");
    }
}

/* The main() contains the Connectionless Iterative Server Conceptual Algorithm:
     1. Create socket
     2. Bind to well known address
     3. Repeat:
          Read request  (recvfrom)
          Process request
          Formulate response
          Send reply  (sendto)
*/
int main(void)
{
    WSADATA wsaData;

    /* Initialise Winsock */
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("[UDP SERVER] WSAStartup failed.\n");
        return 1;
    }

    /* Create a UDP socket */
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0); /* SOCK_DGRAM = UDP */
    if (sock == INVALID_SOCKET)
    {
        printf("[UDP SERVER] socket() failed.\n");
        WSACleanup();
        return 1;
    }

    /* Bind the socket to a well known address */
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT_UDP);

    if (bind(sock, (struct sockaddr *)&serverAddr,
             sizeof(serverAddr)) == SOCKET_ERROR)
    {
        printf("[UDP SERVER] bind() failed. Port %d may be in use.\n",
               SERVER_PORT_UDP);
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("[UDP SERVER] ChatLink UDP Server started on port %d\n",
           SERVER_PORT_UDP);
    printf("[UDP SERVER] Waiting for requests...\n");
    printf("[UDP SERVER] Press Ctrl+C to stop.\n\n");

    /* The iterative request loop */
    while (1)
    {
        char buffer[BUFFER_SIZE];
        struct sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);

        memset(buffer, 0, sizeof(buffer));

        /* READ REQUEST: recvfrom() gives us the data AND
           the client's address so we know where to send the reply */
        int bytesReceived = recvfrom(sock, buffer, sizeof(buffer) - 1,
                                     0,
                                     (struct sockaddr *)&clientAddr,
                                     &clientAddrLen);
        if (bytesReceived <= 0)
            continue;

        buffer[bytesReceived] = '\0';
        printf("[UDP SERVER] Received: %s", buffer);

        /* PROCESS REQUEST: parse the command (REGISTER OR LOGIN OR SUCH) and route to the respective handler based on the command */
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
            strncpy(command, buffer, 31);
            command[strcspn(command, "\n")] = '\0';
            args = buffer + strlen(command);
        }

        /* FORMULATE RESPONSE AND SEND REPLY. Each handler calls sendReply_UDP() which uses sendto() */
        if (strcmp(command, "REGISTER") == 0)
            handleRegister_UDP(sock, &clientAddr, clientAddrLen, args);
        else if (strcmp(command, "LOGIN") == 0)
            handleLogin_UDP(sock, &clientAddr, clientAddrLen, args);
        else if (strcmp(command, "LOADUSERS") == 0)
            handleLoadUsers_UDP(sock, &clientAddr, clientAddrLen, args);
        else if (strcmp(command, "SENDMSG") == 0)
            handleSendMessage_UDP(sock, &clientAddr, clientAddrLen, args);
        else if (strcmp(command, "LOADMSGS") == 0)
            handleLoadMessages_UDP(sock, &clientAddr, clientAddrLen, args);
        else if (strcmp(command, "SEARCHUSER") == 0)
            handleSearchUser_UDP(sock, &clientAddr, clientAddrLen, args);
        else if (strcmp(command, "DELETEUSER") == 0)
            handleDeleteUser_UDP(sock, &clientAddr, clientAddrLen, args);
        else
        {
            printf("[UDP SERVER] Unknown command: %s\n", command);
            sendReply_UDP(sock, &clientAddr, clientAddrLen,
                          "FAIL:UNKNOWN_COMMAND\n");
        }
        /* Loop back to recvfrom() to be ready for the next request */
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
