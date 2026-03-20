#include "raylib.h"
#include "../include/chat.h"
#include "../include/ui.h"
#include "../include/screen.h"
#include "../include/client_socket.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Layout constants */
#define SIDEBAR_W 220 /* Width of the left sidebar */
#define HEADER_H 56   /* Height of the top header bar */
#define INPUT_H 60    /* Height of the message input area */
#define CHAT_W 860    /* Total screen width */
#define CHAT_SH 580   /* Total screen height */

/* Initialises the Chat Screen after a successful login. Requests all users and messages from the server */
void InitChatScreen(ChatScreen *cs, const char *username)
{
    strncpy(cs->currentUser, username, MAX_USERNAME - 1);

    /* No conversation open yet */
    memset(cs->activePeer, 0, sizeof(cs->activePeer));
    cs->hasPeer = false;
    cs->chatMessageCount = 0;
    cs->scrollOffset = 0;
    cs->showConfirmDialog = false;
    cs->userCount = 0;
    cs->allMessageCount = 0;

    /* LOADUSERS request - Ask server for all registered users except currentUser. Server replies with: USER:bob\nUSER:carol\nEND\n */
    char request[BUFFER_SIZE];
    char reply[BUFFER_SIZE];

    snprintf(request, sizeof(request), "LOADUSERS:%s", username);
    if (SendRequest(request, reply, sizeof(reply)))
    {
        /* Parse each USER:username line from the reply */
        char *line = strtok(reply, "\n");
        while (line != NULL && cs->userCount < MAX_USERS)
        {
            if (strncmp(line, "USER:", 5) == 0)
            {
                strncpy(cs->userList[cs->userCount].username,
                        line + 5, MAX_USERNAME - 1);
                cs->userCount++;
            }
            line = strtok(NULL, "\n");
        }
    }

    /* Sidebar buttons */
    cs->searchBtn = (Button){
        .rect = {10, HEADER_H + 10, SIDEBAR_W - 20, 38},
        .label = "Search User"};

    cs->logoutBtn = (Button){
        .rect = {10, CHAT_SH - 90, (SIDEBAR_W - 25) / 2, 36},
        .label = "Logout"};

    cs->deregisterBtn = (Button){
        .rect = {10 + (SIDEBAR_W - 25) / 2 + 5,
                 CHAT_SH - 90,
                 (SIDEBAR_W - 25) / 2, 36},
        .label = "Delete"};

    /* Message input field at the bottom of the chat area */
    cs->messageInput = (InputField){
        .rect = {SIDEBAR_W + 12, CHAT_SH - INPUT_H + 8,
                 CHAT_W - SIDEBAR_W - 80, 40},
        .placeholder = "Type a message...",
        .isPassword = false};

    /* Confirmation dialog buttons for account deletion */
    cs->confirmYesBtn = (Button){
        .rect = {CHAT_W / 2 - 110, CHAT_SH / 2 + 20, 100, 38},
        .label = "Yes, Delete"};
    cs->confirmNoBtn = (Button){
        .rect = {CHAT_W / 2 + 10, CHAT_SH / 2 + 20, 100, 38},
        .label = "Cancel"};
}

/* Requests messages for a conversation from the server and filters them into chatMessages[] */
void LoadChatForPeer(ChatScreen *cs, const char *peer)
{
    strncpy(cs->activePeer, peer, MAX_USERNAME - 1);
    cs->hasPeer = true;
    cs->chatMessageCount = 0;
    cs->scrollOffset = 0;

    char request[BUFFER_SIZE];
    char reply[BUFFER_SIZE * 4];
    memset(reply, 0, sizeof(reply));

    snprintf(request, sizeof(request),
             "LOADMSGS:%s:%s", cs->currentUser, peer);

    if (!SendRequest(request, reply, sizeof(reply)))
        return;

    /* Parse reply line by line manually — avoids strtok re-entrancy issue.
     * Each line is: MSG:sender:recipient:content:HH:MM
     * We walk through the buffer character by character to extract lines. */
    char *pos = reply;

    while (*pos != '\0' && cs->chatMessageCount < MAX_CHAT_MESSAGES)
    {
        /* Find the end of this line */
        char *lineEnd = strchr(pos, '\n');
        if (lineEnd == NULL)
            break;

        /* Copy this line into a temp buffer */
        int lineLen = (int)(lineEnd - pos);
        if (lineLen <= 0)
        {
            pos = lineEnd + 1;
            continue;
        }

        char line[BUFFER_SIZE];
        memset(line, 0, sizeof(line));
        strncpy(line, pos, lineLen);
        pos = lineEnd + 1; /* Advance past this line */

        /* Only process MSG: lines */
        if (strncmp(line, "MSG:", 4) != 0)
            continue;

        /* Parse fields manually from "MSG:sender:recipient:content:HH:MM"
         * Use strchr repeatedly to find each colon separator */
        char *start = line + 4; /* Skip "MSG:" */

        /* Field 1: sender */
        char *sep = strchr(start, ':');
        if (!sep)
            continue;
        *sep = '\0';
        char sender[MAX_USERNAME] = {0};
        strncpy(sender, start, MAX_USERNAME - 1);
        start = sep + 1;

        /* Field 2: recipient */
        sep = strchr(start, ':');
        if (!sep)
            continue;
        *sep = '\0';
        char recipient[MAX_USERNAME] = {0};
        strncpy(recipient, start, MAX_USERNAME - 1);
        start = sep + 1;

        /* Field 3 + 4+5: content and timestamp HH:MM
         * Timestamp is always the last two colon-separated values.
         * Use strrchr to find from the end to avoid splitting on
         * colons that may appear inside the message content. */
        char *lastColon = strrchr(start, ':');
        if (!lastColon)
            continue;
        char mm[4] = {0};
        strncpy(mm, lastColon + 1, 3);
        mm[strcspn(mm, "\r\n ")] = '\0';
        *lastColon = '\0';

        char *hourColon = strrchr(start, ':');
        if (!hourColon)
            continue;
        char hh[4] = {0};
        strncpy(hh, hourColon + 1, 3);
        hh[strcspn(hh, "\r\n ")] = '\0';
        *hourColon = '\0';

        char content[MAX_MESSAGE] = {0};
        strncpy(content, start, MAX_MESSAGE - 1);

        char timestamp[20] = {0};
        snprintf(timestamp, sizeof(timestamp), "%s:%s", hh, mm);

        /* Store in chatMessages[] */
        Message *m = &cs->chatMessages[cs->chatMessageCount];
        strncpy(m->sender, sender, MAX_USERNAME - 1);
        strncpy(m->recipient, recipient, MAX_USERNAME - 1);
        strncpy(m->content, content, MAX_MESSAGE - 1);
        strncpy(m->timestamp, timestamp, 19);
        cs->chatMessageCount++;
    }
}

/* Handles all Chat Screen logic — contact clicks, sending messages, scroll, sidebar buttons and the deregister confirmation dialog. */
AppScreen UpdateChatScreen(ChatScreen *cs, char *loggedInUser)
{
    /* Handle confirmation dialog before anything else — blocks all other input */
    if (cs->showConfirmDialog)
    {
        if (IsButtonClicked(&cs->confirmYesBtn))
        {
            /* DELETEUSER request - Ask server to remove this user and all their messages. Server replies: "OK" or "FAIL" */
            char request[BUFFER_SIZE];
            char reply[BUFFER_SIZE];
            snprintf(request, sizeof(request),
                     "DELETEUSER:%s", cs->currentUser);
            SendRequest(request, reply, sizeof(reply));

            memset(loggedInUser, 0, MAX_USERNAME);
            return SCREEN_WELCOME;
        }
        if (IsButtonClicked(&cs->confirmNoBtn))
            cs->showConfirmDialog = false;

        return SCREEN_CHAT;
    }

    /* Sidebar contact rows — each is a clickable rectangle */
    int contactY = HEADER_H + 64;
    for (int i = 0; i < cs->userCount; i++)
    {
        if (strcmp(cs->userList[i].username, cs->currentUser) == 0)
            continue; /* Skip self — cannot chat with yourself */

        Rectangle contactRect = {0, (float)contactY, SIDEBAR_W, 54};

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointRec(GetMousePosition(), contactRect))
        {
            /* Only reload if switching to a different contact */
            if (!cs->hasPeer ||
                strcmp(cs->activePeer, cs->userList[i].username) != 0)
                LoadChatForPeer(cs, cs->userList[i].username);
        }

        contactY += 56;
    }

    /* Sidebar action buttons */
    if (IsButtonClicked(&cs->searchBtn))
        return SCREEN_SEARCH;
    if (IsButtonClicked(&cs->logoutBtn))
    {
        memset(loggedInUser, 0, MAX_USERNAME);
        return SCREEN_WELCOME;
    }
    if (IsButtonClicked(&cs->deregisterBtn))
        cs->showConfirmDialog = true;

    /* Message input and sending — only active when a conversation is open */
    if (cs->hasPeer)
    {
        /* Activate the input field on click */
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            cs->messageInput.active = CheckCollisionPointRec(
                GetMousePosition(), cs->messageInput.rect);

        HandleInputField(&cs->messageInput);

        Rectangle sendRect = {CHAT_W - 58.0f, CHAT_SH - INPUT_H + 8.0f, 48, 40};
        bool sendClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                           CheckCollisionPointRec(GetMousePosition(), sendRect);
        bool enterPressed = IsKeyPressed(KEY_ENTER) && cs->messageInput.active;

        if ((sendClicked || enterPressed) && cs->messageInput.textLength > 0)
        {
            /* Stamp with current local time */
            time_t t = time(NULL);
            struct tm *tm = localtime(&t);
            char timestamp[20];
            snprintf(timestamp, sizeof(timestamp),
                     "%02d:%02d", tm->tm_hour, tm->tm_min);

            /* SENDMSG request - Ask server to save the message to messages.txt. Format: SENDMSG:sender:recipient:content:HH:MM, server replies: "OK" or "FAIL" */
            char request[BUFFER_SIZE];
            char reply[BUFFER_SIZE];
            snprintf(request, sizeof(request), "SENDMSG:%s:%s:%s:%s",
                     cs->currentUser,
                     cs->activePeer,
                     cs->messageInput.text,
                     timestamp);

            if (SendRequest(request, reply, sizeof(reply)) &&
                strncmp(reply, "OK", 2) == 0)
            {
                /* Add to in-memory arrays for instant display */
                Message msg;
                strncpy(msg.sender, cs->currentUser, MAX_USERNAME - 1);
                strncpy(msg.recipient, cs->activePeer, MAX_USERNAME - 1);
                strncpy(msg.content, cs->messageInput.text, MAX_MESSAGE - 1);
                strncpy(msg.timestamp, timestamp, 19);

                if (cs->chatMessageCount < MAX_CHAT_MESSAGES)
                    cs->chatMessages[cs->chatMessageCount++] = msg;
            }

            ClearInputField(&cs->messageInput);
            cs->messageInput.active = true;
            cs->scrollOffset = 999999;
        }

        /* Mouse wheel scrolling — clamp so it never goes above the top */
        float wheel = GetMouseWheelMove();
        cs->scrollOffset -= wheel * 30;
        if (cs->scrollOffset < 0)
            cs->scrollOffset = 0;
    }

    return SCREEN_CHAT;
}

/* Draws the full Chat Screen every frame — sidebar, chat area, input and dialog */
void DrawChatScreen(ChatScreen *cs)
{
    ClearBackground(COLOR_BG);

    /* SIDEBAR */
    DrawPanel(0, 0, SIDEBAR_W, CHAT_SH, COLOR_SIDEBAR, COLOR_BORDER);

    /* Sidebar header — logged-in user avatar, name and online status */
    DrawPanel(0, 0, SIDEBAR_W, HEADER_H, COLOR_CARD, COLOR_BORDER);
    DrawAvatar(28, HEADER_H / 2, 18, COLOR_ACCENT, cs->currentUser[0]);
    DrawText(cs->currentUser, 52, HEADER_H / 2 - 8, 15, COLOR_TEXT);
    DrawCircle(52, HEADER_H / 2 + 12, 4, COLOR_GREEN);
    DrawText("Online", 60, HEADER_H / 2 + 5, 12, COLOR_GREEN);

    DrawButton(&cs->searchBtn, COLOR_CARD, COLOR_TEAL);
    DrawText("CONTACTS", 12, HEADER_H + 58, 11, COLOR_MUTED);

    /* Contact list — one row per registered user (excluding self) */
    int contactY = HEADER_H + 74;
    for (int i = 0; i < cs->userCount; i++)
    {
        if (strcmp(cs->userList[i].username, cs->currentUser) == 0)
            continue;

        const char *uname = cs->userList[i].username;
        bool isActive = cs->hasPeer &&
                        strcmp(cs->activePeer, uname) == 0;

        /* Highlight the active contact with a tinted background and accent bar */
        if (isActive)
        {
            DrawRectangle(0, contactY, SIDEBAR_W, 54,
                          (Color){233, 69, 96, 40});
            DrawRectangle(0, contactY + 8, 3, 38, COLOR_ACCENT);
        }

        /* Avatar — color derived from first letter of username for variety */
        Color avatarColor = (Color){
            (unsigned char)(60 + (uname[0] * 37) % 120),
            (unsigned char)(80 + (uname[0] * 71) % 100),
            (unsigned char)(120 + (uname[0] * 53) % 100),
            255};
        DrawAvatar(24, contactY + 27, 16, avatarColor, uname[0]);

        DrawText(uname, 48, contactY + 14, 14,
                 isActive ? COLOR_ACCENT : COLOR_TEXT);

        /* Last message preview — scan allMessages backwards */
        /* Last message preview — show last chatMessage if this is the active contact */
        const char *preview = "No messages yet";
        if (isActive && cs->chatMessageCount > 0)
            preview = cs->chatMessages[cs->chatMessageCount - 1].content;

        /* Truncate preview to 22 chars to fit the sidebar width */
        char previewShort[26] = {0};
        strncpy(previewShort, preview, 22);
        if (strlen(preview) > 22)
            strcat(previewShort, "...");
        DrawText(previewShort, 48, contactY + 32, 12, COLOR_MUTED);

        contactY += 56;
    }

    /* Logout and Delete buttons at the bottom of the sidebar */
    DrawButton(&cs->logoutBtn, COLOR_CARD, COLOR_TEXT);
    DrawButton(&cs->deregisterBtn, COLOR_CARD, COLOR_ACCENT);

    /* MAIN CHAT AREA */
    int chatX = SIDEBAR_W;
    int chatW = CHAT_W - SIDEBAR_W;

    /* Chat header */
    DrawPanel(chatX, 0, chatW, HEADER_H, COLOR_SURFACE, COLOR_BORDER);
    if (cs->hasPeer)
    {
        DrawAvatar(chatX + 28, HEADER_H / 2, 18,
                   COLOR_TEAL, cs->activePeer[0]);
        DrawText(cs->activePeer,
                 chatX + 52, HEADER_H / 2 - 8, 16, COLOR_TEXT);
        DrawCircle(chatX + 52, HEADER_H / 2 + 12, 4, COLOR_GREEN);
        DrawText("Online", chatX + 60, HEADER_H / 2 + 5, 12, COLOR_GREEN);
    }
    else
    {
        DrawText("Select a contact to start chatting",
                 chatX + chatW / 2 -
                     MeasureText("Select a contact to start chatting", 16) / 2,
                 HEADER_H / 2 - 8, 16, COLOR_MUTED);
    }

    /* Message bubble area — clipped to chat region */
    int msgAreaY = HEADER_H;
    int msgAreaH = CHAT_SH - HEADER_H - INPUT_H;
    BeginScissorMode(chatX, msgAreaY, chatW, msgAreaH);

    if (!cs->hasPeer)
    {
        DrawText("No chat open",
                 chatX + chatW / 2 - 60,
                 CHAT_SH / 2 - 20, 20, COLOR_MUTED);
        DrawText("Click a contact on the left to begin",
                 chatX + chatW / 2 -
                     MeasureText("Click a contact on the left to begin", 14) / 2,
                 CHAT_SH / 2 + 10, 14, COLOR_MUTED);
    }
    else
    {
        /* Calculate scroll bounds and clamp scrollOffset */
        int totalMsgHeight = cs->chatMessageCount * 54;
        int visibleHeight = msgAreaH - 20;
        float maxScroll = (float)(totalMsgHeight - visibleHeight);
        if (maxScroll < 0)
            maxScroll = 0;
        if (cs->scrollOffset > maxScroll)
            cs->scrollOffset = maxScroll;

        int startY = msgAreaY + 10 - (int)cs->scrollOffset;

        /* Draw each message bubble */
        for (int i = 0; i < cs->chatMessageCount; i++)
        {
            Message *m = &cs->chatMessages[i];
            bool sent = strcmp(m->sender, cs->currentUser) == 0;
            int bubbleY = startY + i * 54;
            int maxBubW = chatW - 40;

            if (bubbleY + 44 < msgAreaY)
                continue;
            if (bubbleY > msgAreaY + msgAreaH)
                break;

            if (sent)
            {
                DrawChatBubble(m->content,
                               chatX + chatW - 14, bubbleY, maxBubW, true);
                DrawText(m->timestamp,
                         chatX + chatW - 14 -
                             MeasureText(m->timestamp, 11) - 4,
                         bubbleY + 44, 11, COLOR_MUTED);
            }
            else
            {
                DrawChatBubble(m->content,
                               chatX + 14, bubbleY, maxBubW, false);
                DrawText(m->timestamp,
                         chatX + 18, bubbleY + 44, 11, COLOR_MUTED);
            }
        }
    }

    EndScissorMode();

    /* Input area */
    DrawPanel(chatX, CHAT_SH - INPUT_H,
              chatW, INPUT_H, COLOR_SURFACE, COLOR_BORDER);
    if (cs->hasPeer)
    {
        DrawInputField(&cs->messageInput);

        Rectangle sendRect = {CHAT_W - 58.0f,
                              CHAT_SH - INPUT_H + 8.0f, 48, 40};
        bool sendHovered = CheckCollisionPointRec(
            GetMousePosition(), sendRect);
        DrawRectangleRounded(sendRect, 0.3f, 6,
                             sendHovered ? COLOR_ACCENT : (Color){233, 69, 96, 180});
        DrawText(">>", CHAT_W - 48,
                 CHAT_SH - INPUT_H + 20, 16, WHITE);
    }
    else
    {
        DrawText("Select a contact to send a message",
                 chatX + chatW / 2 -
                     MeasureText("Select a contact to send a message", 14) / 2,
                 CHAT_SH - INPUT_H + 22, 14, COLOR_MUTED);
    }

    /* DEREGISTER CONFIRMATION DIALOG */
    if (cs->showConfirmDialog)
    {
        DrawRectangle(0, 0, CHAT_W, CHAT_SH, (Color){0, 0, 0, 160});

        int dw = 340, dh = 160;
        int dx = CHAT_W / 2 - dw / 2;
        int dy = CHAT_SH / 2 - dh / 2;
        DrawPanel(dx, dy, dw, dh, COLOR_CARD, COLOR_ACCENT);
        DrawCenteredText("Delete Account?", dy + 20, 20, COLOR_TEXT);
        DrawCenteredText("This cannot be undone.", dy + 50, 15, COLOR_MUTED);
        DrawButton(&cs->confirmYesBtn, COLOR_ACCENT, WHITE);
        DrawButton(&cs->confirmNoBtn, COLOR_SURFACE, COLOR_TEXT);
    }
}