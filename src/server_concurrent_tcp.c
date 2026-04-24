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
#define SERVER_PORT 8888
#define BUFFER_SIZE 4096
#define BACKLOG 5
#define MAX_REPLY 16384

/* ═══════════════════════════════════════════════
   sendReply
   Sends a string back to the connected client
   ═══════════════════════════════════════════════ */
void sendReply(int clientSock, const char *reply)
{
    send(clientSock, reply, strlen(reply), 0);
}

/* ═══════════════════════════════════════════════
   REGISTER:username:password
   ═══════════════════════════════════════════════ */
void handleRegister(int clientSock, char *args)
{
    char username[MAX_USERNAME] = {0};
    char password[MAX_PASSWORD] = {0};

    char *colon = strchr(args, ':');
    if (!colon)
    {
        sendReply(clientSock, "FAIL:EMPTY_FIELDS\n");
        return;
    }
    *colon = '\0';
    strncpy(username, args, MAX_USERNAME - 1);
    strncpy(password, colon + 1, MAX_PASSWORD - 1);
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

    printf("[SLAVE %d] Registered new user: %s\n", getpid(), username);
    sendReply(clientSock, "OK\n");
}

/* ═══════════════════════════════════════════════
   LOGIN:username:password
   ═══════════════════════════════════════════════ */
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
        printf("[SLAVE %d] Login success: %s\n", getpid(), username);
        sendReply(clientSock, "OK\n");
    }
    else
    {
        printf("[SLAVE %d] Login failed: %s\n", getpid(), username);
        sendReply(clientSock, "FAIL:INVALID_CREDENTIALS\n");
    }
}

/* ═══════════════════════════════════════════════
   LOADUSERS:currentUser
   ═══════════════════════════════════════════════ */
void handleLoadUsers(int clientSock, char *args)
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
    sendReply(clientSock, reply);
}

/* ═══════════════════════════════════════════════
   SENDMSG:sender:recipient:content:HH:MM
   ═══════════════════════════════════════════════ */
void handleSendMessage(int clientSock, char *args)
{
    Message msg;
    memset(&msg, 0, sizeof(msg));

    char *p = strtok(args, ":");
    if (!p)
    {
        sendReply(clientSock, "FAIL\n");
        return;
    }
    strncpy(msg.sender, p, MAX_USERNAME - 1);

    p = strtok(NULL, ":");
    if (!p)
    {
        sendReply(clientSock, "FAIL\n");
        return;
    }
    strncpy(msg.recipient, p, MAX_USERNAME - 1);

    p = strtok(NULL, ":");
    if (!p)
    {
        sendReply(clientSock, "FAIL\n");
        return;
    }
    strncpy(msg.content, p, MAX_MESSAGE - 1);

    char *hh = strtok(NULL, ":");
    char *mm = strtok(NULL, ":\n");
    if (!hh || !mm)
    {
        sendReply(clientSock, "FAIL\n");
        return;
    }
    snprintf(msg.timestamp, sizeof(msg.timestamp), "%s:%s", hh, mm);

    SaveMessage(&msg);
    printf("[SLAVE %d] Message saved: %s -> %s\n",
           getpid(), msg.sender, msg.recipient);
    sendReply(clientSock, "OK\n");
}

/* ═══════════════════════════════════════════════
   LOADMSGS:userA:userB
   ═══════════════════════════════════════════════ */
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
    sendReply(clientSock, reply);
}

/* ═══════════════════════════════════════════════
   SEARCHUSER:query:currentUser
   ═══════════════════════════════════════════════ */
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

/* ═══════════════════════════════════════════════
   DELETEUSER:username
   ═══════════════════════════════════════════════ */
void handleDeleteUser(int clientSock, char *args)
{
    char username[MAX_USERNAME] = {0};
    strncpy(username, args, MAX_USERNAME - 1);
    username[strcspn(username, "\n")] = '\0';

    if (DeleteUser(username))
    {
        printf("[SLAVE %d] Deleted user: %s\n", getpid(), username);
        sendReply(clientSock, "OK\n");
    }
    else
    {
        sendReply(clientSock, "FAIL\n");
    }
}

/* ═══════════════════════════════════════════════
   handleClient — SLAVE process function
   Handles all requests from one client then exits
   ═══════════════════════════════════════════════ */
void handleClient(int clientSock)
{
    char buffer[BUFFER_SIZE];

    printf("[SLAVE %d] Handling client...\n", getpid());

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));

        int bytesReceived = recv(clientSock, buffer,
                                 sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0)
        {
            printf("[SLAVE %d] Client disconnected.\n", getpid());
            break;
        }

        buffer[bytesReceived] = '\0';
        printf("[SLAVE %d] Received: %s", getpid(), buffer);

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
            printf("[SLAVE %d] Unknown command: %s\n",
                   getpid(), command);
            sendReply(clientSock, "FAIL:UNKNOWN_COMMAND\n");
        }
    }

    close(clientSock);
}

/* ═══════════════════════════════════════════════
   main — MASTER process
   Follows the conceptual algorithm from lecture:
     1. Create socket
     2. Bind to well known address
     3. Place in passive mode (listen)
     4. Repeat:
          accept next connection
          fork() — create slave process
          master: close clientSock, loop back
          slave:  handle client, exit()
   ═══════════════════════════════════════════════ */
int main(void)
{
    /* ── STEP 1: Create TCP socket ── */
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0)
    {
        perror("[MASTER] socket() failed");
        return 1;
    }

    /* Allow port reuse so we can restart quickly */
    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR,
               &opt, sizeof(opt));

    /* ── STEP 2: Bind to well known address ── */
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(serverSock, (struct sockaddr *)&serverAddr,
             sizeof(serverAddr)) < 0)
    {
        perror("[MASTER] bind() failed");
        close(serverSock);
        return 1;
    }

    /* ── STEP 3: Place socket in passive mode ── */
    if (listen(serverSock, BACKLOG) < 0)
    {
        perror("[MASTER] listen() failed");
        close(serverSock);
        return 1;
    }

    printf("[MASTER] ChatLink Concurrent TCP Server started on port %d\n",
           SERVER_PORT);
    printf("[MASTER] Waiting for connections...\n");
    printf("[MASTER] Press Ctrl+C to stop.\n\n");

    /* ── STEP 4: Accept loop ── */
    while (1)
    {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        /* Block until a client connects */
        int clientSock = accept(serverSock,
                                (struct sockaddr *)&clientAddr,
                                &clientAddrLen);
        if (clientSock < 0)
        {
            perror("[MASTER] accept() failed");
            continue;
        }

        printf("[MASTER] Accepted connection from %s\n",
               inet_ntoa(clientAddr.sin_addr));

        /* ── FORK: create slave process ── */
        pid_t pid = fork();

        if (pid < 0)
        {
            /* Fork failed */
            perror("[MASTER] fork() failed");
            close(clientSock);
            continue;
        }
        else if (pid != 0)
        {
            /* ── MASTER PROCESS ──
               Close the client socket — master never talks to clients
               Loop back to accept the next connection immediately
               This is what makes it CONCURRENT — master doesn't block */
            printf("[MASTER] Spawned slave process %d for client\n", pid);
            close(clientSock);

            /* Clean up any finished slave processes to avoid zombies */
            waitpid(-1, NULL, WNOHANG);
        }
        else
        {
            /* ── SLAVE PROCESS ──
               Close the server socket — slave only talks to this client
               Handle all requests from this client
               Exit when client disconnects */
            close(serverSock);
            handleClient(clientSock);
            printf("[SLAVE %d] Done. Exiting.\n", getpid());
            exit(0);
        }
    }

    close(serverSock);
    return 0;
}