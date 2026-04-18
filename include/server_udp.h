/* Defines constants and declares handler functions for the ChatLink UDP server */
#ifndef SERVER_UDP_H
#define SERVER_UDP_H

#define SERVER_PORT_UDP 8889 /* Different port from TCP to avoid conflicts */
#define BUFFER_SIZE 4096
#define MAX_REPLY 4096

/* Request handlers — same logic as TCP version, different transport */
void handleRegister_UDP(SOCKET sock, struct sockaddr_in *clientAddr, int addrLen, char *args);
void handleLogin_UDP(SOCKET sock, struct sockaddr_in *clientAddr, int addrLen, char *args);
void handleLoadUsers_UDP(SOCKET sock, struct sockaddr_in *clientAddr, int addrLen, char *args);
void handleSendMessage_UDP(SOCKET sock, struct sockaddr_in *clientAddr, int addrLen, char *args);
void handleLoadMessages_UDP(SOCKET sock, struct sockaddr_in *clientAddr, int addrLen, char *args);
void handleSearchUser_UDP(SOCKET sock, struct sockaddr_in *clientAddr, int addrLen, char *args);
void handleDeleteUser_UDP(SOCKET sock, struct sockaddr_in *clientAddr, int addrLen, char *args);

/* Sends a reply back to the client using sendto() */
void sendReply_UDP(SOCKET sock, struct sockaddr_in *clientAddr, int addrLen, const char *reply);

#endif
