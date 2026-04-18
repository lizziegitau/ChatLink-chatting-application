#include "raylib.h"
#include "../include/screen.h"
#include "../include/ui.h"
#include "../include/welcome.h"
#include "../include/auth.h"
#include "../include/chat.h"
#include "../include/search.h"
#include "../include/client_socket_udp.h"
#include <string.h>
#include <stdio.h>

int main(void)
{
    /* Initialise UDP socket before opening the window */
    if (!InitUDPSocket())
    {
        printf("ERROR: Could not initialise UDP socket.\n");
        printf("Make sure ChatLink_Server_UDP.exe is running first.\n");
        return 1;
    }

    InitWindow(860, 580, "ChatLink - One on One Chat (UDP)");
    SetTargetFPS(60);

    WelcomeScreen welcomeScreen;
    AuthScreen authScreen;
    ChatScreen chatScreen;
    SearchScreen searchScreen;

    AppScreen currentScreen = SCREEN_WELCOME;
    char loggedInUser[MAX_USERNAME] = {0};
    char peerToOpen[MAX_USERNAME] = {0};

    char toastMsg[128] = {0};
    float toastTimer = 0.0f;
    bool toastIsOk = true;

    InitWelcomeScreen(&welcomeScreen);

    while (!WindowShouldClose())
    {
        AppScreen nextScreen = currentScreen;

        switch (currentScreen)
        {
        case SCREEN_WELCOME:
        {
            AppScreen result = UpdateWelcomeScreen(&welcomeScreen);
            if (result != SCREEN_WELCOME)
            {
                bool wantsRegister = welcomeScreen.registerBtn.hovered;
                InitAuthScreen(&authScreen, wantsRegister);
                nextScreen = SCREEN_AUTH;
            }
            break;
        }

        case SCREEN_AUTH:
        {
            AppScreen result = UpdateAuthScreen(&authScreen, loggedInUser);
            if (result == SCREEN_CHAT)
            {
                InitChatScreen(&chatScreen, loggedInUser);
                snprintf(toastMsg, sizeof(toastMsg),
                         "Welcome, %s!", loggedInUser);
                toastTimer = 2.5f;
                toastIsOk = true;
            }
            nextScreen = result;
            break;
        }

        case SCREEN_CHAT:
        {
            if (strlen(peerToOpen) > 0)
            {
                LoadChatForPeer(&chatScreen, peerToOpen);
                memset(peerToOpen, 0, sizeof(peerToOpen));
            }

            AppScreen result = UpdateChatScreen(&chatScreen, loggedInUser);

            if (result == SCREEN_SEARCH)
            {
                InitSearchScreen(&searchScreen, loggedInUser);
                nextScreen = SCREEN_SEARCH;
            }
            else if (result == SCREEN_WELCOME)
            {
                snprintf(toastMsg, sizeof(toastMsg), "Goodbye! See you soon.");
                toastTimer = 2.0f;
                toastIsOk = true;
                InitWelcomeScreen(&welcomeScreen);
                nextScreen = SCREEN_WELCOME;
            }
            else
            {
                nextScreen = result;
            }
            break;
        }

        case SCREEN_SEARCH:
        {
            AppScreen result = UpdateSearchScreen(&searchScreen, peerToOpen);
            if (result == SCREEN_CHAT)
                nextScreen = SCREEN_CHAT;
            else
                nextScreen = result;
            break;
        }

        default:
            break;
        }

        currentScreen = nextScreen;

        BeginDrawing();
        switch (currentScreen)
        {
        case SCREEN_WELCOME:
            DrawWelcomeScreen(&welcomeScreen);
            break;
        case SCREEN_AUTH:
            DrawAuthScreen(&authScreen);
            break;
        case SCREEN_CHAT:
            DrawChatScreen(&chatScreen);
            break;
        case SCREEN_SEARCH:
            DrawSearchScreen(&searchScreen);
            break;
        default:
            ClearBackground(COLOR_BG);
            break;
        }
        DrawToast(toastMsg, toastIsOk, &toastTimer);
        EndDrawing();
    }

    /* Close UDP socket before exiting */
    CloseUDPSocket();
    CloseWindow();
    return 0;
}