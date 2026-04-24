#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include "../include/fileio.h"

/* ═══════════════════════════════════════════════
   Constants
   ═══════════════════════════════════════════════ */
#define SERVER_PORT_UDP 8889
#define BUFFER_SIZE 4096
#define MAX_REPLY 16384

/* ═══════════════════════════════════════════════
   sendReply_UDP
   Sends reply back to client using sendto()
   ═══════════════════════════════════════════════ */
void sendReply_UDP(int sock, struct sockaddr_in *clientAddr,
                   socklen_t addrLen, const char *reply)
{
    sendto(sock, reply, strlen(reply), 0,
           (struct sockaddr *)clientAddr, addrLen);
}

/* ═══════════════════════════════════════════════
   Request Handlers — same logic as iterative UDP
   Only difference: prints slave PID for clarity
   ═══════════════════════════════════════════════ */
void handleRegister_UDP(int sock, struct sockaddr_in *clientAddr,
                        socklen_t addrLen, char *args)
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

    printf("[SLAVE %d] Registered: %s\n", getpid(), username);
    sendReply_UDP(sock, clientAddr, addrLen, "OK\n");
}

void handleLogin_UDP(int sock, struct sockaddr_in *clientAddr,
                     socklen_t addrLen, char *args)
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
        printf("[SLAVE %d] Login success: %s\n", getpid(), username);
        sendReply_UDP(sock, clientAddr, addrLen, "OK\n");
    }
    else
    {
        printf("[SLAVE %d] Login failed: %s\n", getpid(), username);
        sendReply_UDP(sock, clientAddr, addrLen, "FAIL:INVALID_CREDENTIALS\n");
    }
}

void handleLoadUsers_UDP(int sock, struct sockaddr_in *clientAddr,
                         socklen_t addrLen, char *args)
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

void handleSendMessage_UDP(int sock, struct sockaddr_in *clientAddr,
                           socklen_t addrLen, char *args)
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
    printf("[SLAVE %d] Message saved: %s -> %s\n",
           getpid(), msg.sender, msg.recipient);
    sendReply_UDP(sock, clientAddr, addrLen, "OK\n");
}

void handleLoadMessages_UDP(int sock, struct sockaddr_in *clientAddr,
                            socklen_t addrLen, char *args)
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

    char reply[MAX_REPLY] = {0};
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
            strncat(reply, line, MAX_REPLY - strlen(reply) - 1);
        }
    }
    strncat(reply, "END\n", MAX_REPLY - strlen(reply) - 1);
    sendReply_UDP(sock, clientAddr, addrLen, reply);
}

void handleSearchUser_UDP(int sock, struct sockaddr_in *clientAddr,
                          socklen_t addrLen, char *args)
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
        sendReply_UDP(sock, clientAddr, addrLen, "NOTFOUND\n");
}

void handleDeleteUser_UDP(int sock, struct sockaddr_in *clientAddr,
                          socklen_t addrLen, char *args)
{
    char username[MAX_USERNAME] = {0};
    strncpy(username, args, MAX_USERNAME - 1);
    username[strcspn(username, "\n")] = '\0';

    if (DeleteUser(username))
    {
        printf("[SLAVE %d] Deleted user: %s\n", getpid(), username);
        sendReply_UDP(sock, clientAddr, addrLen, "OK\n");
    }
    else
        sendReply_UDP(sock, clientAddr, addrLen, "FAIL\n");
}

/* ═══════════════════════════════════════════════
   main — MASTER process
   Follows the conceptual algorithm from lecture:
     1. Create socket
     2. Bind to well known address
     3. Repeat:
          recvfrom() — read next request
          fork() — create slave process
          master: loop back to recvfrom immediately
          slave:  process request, send reply, exit()
   ═══════════════════════════════════════════════ */
int main(void)
{
    /* ── STEP 1: Create UDP socket ── */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("[MASTER] socket() failed");
        return 1;
    }

    /* Allow port reuse */
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* ── STEP 2: Bind to well known address ── */
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT_UDP);

    if (bind(sock, (struct sockaddr *)&serverAddr,
             sizeof(serverAddr)) < 0)
    {
        perror("[MASTER] bind() failed");
        close(sock);
        return 1;
    }

    printf("[MASTER] ChatLink Concurrent UDP Server started on port %d\n",
           SERVER_PORT_UDP);
    printf("[MASTER] Waiting for requests...\n");
    printf("[MASTER] Press Ctrl+C to stop.\n\n");

    /* ── STEP 3: Request loop ── */
    while (1)
    {
        char buffer[BUFFER_SIZE];
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        memset(buffer, 0, sizeof(buffer));

        /* ── READ REQUEST ── */
        int bytesReceived = recvfrom(sock, buffer,
                                     sizeof(buffer) - 1, 0,
                                     (struct sockaddr *)&clientAddr,
                                     &clientAddrLen);
        if (bytesReceived <= 0)
            continue;

        buffer[bytesReceived] = '\0';
        printf("[MASTER] Received request from %s: %s",
               inet_ntoa(clientAddr.sin_addr), buffer);

        /* ── FORK: create slave for this request ── */
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("[MASTER] fork() failed");
            continue;
        }
        else if (pid != 0)
        {
            /* ── MASTER PROCESS ──
               Loop back immediately to recvfrom()
               Ready to receive the next request while
               slave handles this one concurrently */
            printf("[MASTER] Spawned slave %d for request\n", pid);
            waitpid(-1, NULL, WNOHANG);
        }
        else
        {
            /* ── SLAVE PROCESS ──
               Parse command and handle this one request
               Send reply then exit */

            /* Parse command */
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

            /* Route to handler */
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
                sendReply_UDP(sock, &clientAddr, clientAddrLen,
                              "FAIL:UNKNOWN_COMMAND\n");

            printf("[SLAVE %d] Done. Exiting.\n", getpid());
            exit(0);
        }
    }

    close(sock);
    return 0;
}