#include "raylib.h"
#include "../include/search.h"
#include "../include/ui.h"
#include "../include/screen.h"
#include "../include/client_socket.h"
#include <string.h>
#include <stdio.h>

#define SCREEN_W 860
#define SCREEN_H 580

/* Sets up input field and button positions, resets result state */
void InitSearchScreen(SearchScreen *ss, const char *currentUser)
{
    int cx = SCREEN_W / 2;

    strncpy(ss->currentUser, currentUser, MAX_USERNAME - 1);
    ss->result = SEARCH_IDLE;
    ss->openChat = false;
    memset(ss->selectedPeer, 0, sizeof(ss->selectedPeer));
    memset(ss->foundUser.username, 0, sizeof(ss->foundUser.username));

    /* Search input field */
    ss->searchField = (InputField){
        .rect = {cx - 200, 220, 320, 46},
        .placeholder = "Enter username to search...",
        .isPassword = false};

    /* Search button */
    ss->searchBtn = (Button){
        .rect = {cx + 130, 220, 110, 46},
        .label = "Search"};

    /* Back button */
    ss->backBtn = (Button){
        .rect = {20, 20, 110, 38},
        .label = "< Back"};

    /* Open Chat button */
    ss->chatBtn = (Button){
        .rect = {cx - 80, 370, 160, 44},
        .label = "Open Chat"};
}

/* Handles search logic and button clicks. Sends SEARCHUSER request to server instead of loading users from file. Writes the found username into peerToOpen when Open Chat is clicked */
AppScreen UpdateSearchScreen(SearchScreen *ss, char *peerToOpen)
{
    /* Activate field on click */
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        ss->searchField.active = CheckCollisionPointRec(
            GetMousePosition(), ss->searchField.rect);

    HandleInputField(&ss->searchField);

    /* Trigger search on Search button click or Enter key */
    bool doSearch = IsButtonClicked(&ss->searchBtn) ||
                    (IsKeyPressed(KEY_ENTER) && ss->searchField.active);

    if (doSearch && ss->searchField.textLength > 0)
    {
        const char *query = ss->searchField.text;

        /* Self-search is not allowed — check locally before sending */
        if (strcmp(query, ss->currentUser) == 0)
        {
            ss->result = SEARCH_NOT_FOUND;
        }
        else
        {
            /* SEARCHUSER request - Send: "SEARCHUSER:query:currentUser", server replies: "FOUND:username" or "NOTFOUND" */
            char request[BUFFER_SIZE];
            char reply[BUFFER_SIZE];
            snprintf(request, sizeof(request),
                     "SEARCHUSER:%s:%s", query, ss->currentUser);

            if (!SendRequest(request, reply, sizeof(reply)))
            {
                /* Socket error — treat as not found */
                ss->result = SEARCH_NOT_FOUND;
            }
            else if (strncmp(reply, "FOUND:", 6) == 0)
            {
                /* Parse the username from "FOUND:username" */
                char found[MAX_USERNAME] = {0};
                strncpy(found, reply + 6, MAX_USERNAME - 1);
                found[strcspn(found, "\n")] = '\0';

                strncpy(ss->foundUser.username, found, MAX_USERNAME - 1);
                ss->result = SEARCH_FOUND;
            }
            else
            {
                ss->result = SEARCH_NOT_FOUND;
            }
        }
    }

    /* Open Chat — write the found username into peerToOpen for main.c to consume */
    if (ss->result == SEARCH_FOUND && IsButtonClicked(&ss->chatBtn))
    {
        strncpy(peerToOpen, ss->foundUser.username, MAX_USERNAME - 1);
        return SCREEN_CHAT;
    }

    /* Back button — return to Chat Screen without opening any conversation */
    if (IsButtonClicked(&ss->backBtn))
    {
        memset(peerToOpen, 0, MAX_USERNAME);
        return SCREEN_CHAT;
    }

    return SCREEN_SEARCH;
}

/* Draws the full Search Screen every frame */
void DrawSearchScreen(SearchScreen *ss)
{
    int cx = SCREEN_W / 2;

    ClearBackground(COLOR_BG);

    /* Thin accent bar along the top edge */
    DrawRectangle(0, 0, SCREEN_W, 4, COLOR_ACCENT);

    DrawButton(&ss->backBtn, COLOR_CARD, COLOR_MUTED);

    /* Page title and subtitle */
    DrawCenteredText("Search Users", 80, 32, COLOR_TEXT);
    DrawCenteredText("Find a registered user to start chatting",
                     122, 16, COLOR_MUTED);

    /* Divider line below the title */
    DrawRectangle(cx - 200, 160, 440, 1, COLOR_BORDER);

    DrawText("USERNAME", cx - 200, 196, 12, COLOR_MUTED);

    /* Search input field and button */
    DrawInputField(&ss->searchField);
    DrawButton(&ss->searchBtn, COLOR_ACCENT, WHITE);

    /* Result area — content changes based on current SearchResult state */
    if (ss->result == SEARCH_IDLE)
    {
        DrawCenteredText("Type a username and press Search",
                         290, 15, COLOR_MUTED);
    }
    else if (ss->result == SEARCH_NOT_FOUND)
    {
        /* Not found card — red X with error message */
        int cw = 340, ch = 80;
        int cx2 = SCREEN_W / 2 - cw / 2;
        DrawPanel(cx2, 280, cw, ch, COLOR_CARD, COLOR_BORDER);
        DrawText("✗", cx2 + 20, 280 + ch / 2 - 14, 28, COLOR_ACCENT);
        DrawText("User not found",
                 cx2 + 60, 280 + 16, 17, COLOR_TEXT);
        DrawText("No registered user with that username",
                 cx2 + 60, 280 + 40, 13, COLOR_MUTED);
    }
    else if (ss->result == SEARCH_FOUND)
    {
        /* Found card — teal border with avatar, username, status and Open Chat button */
        int cw = 340, ch = 100;
        int cardX = SCREEN_W / 2 - cw / 2;
        int cardY = 272;

        DrawRectangle(cardX, cardY, cw, ch, COLOR_CARD);
        DrawRectangleLines(cardX, cardY, cw, ch, COLOR_TEAL);

        DrawAvatar(cardX + 36, cardY + ch / 2, 22,
                   COLOR_TEAL, ss->foundUser.username[0]);
        DrawText(ss->foundUser.username,
                 cardX + 68, cardY + 22, 18, COLOR_TEXT);
        DrawCircle(cardX + 68, cardY + 52, 5, COLOR_GREEN);
        DrawText("Registered & Online",
                 cardX + 78, cardY + 44, 13, COLOR_GREEN);

        DrawButton(&ss->chatBtn, COLOR_ACCENT, WHITE);
    }

    /* Bottom hint */
    DrawCenteredText("Press Enter or click Search to find a user",
                     SCREEN_H - 30, 13, COLOR_MUTED);
}