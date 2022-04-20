#pragma once
#include <stdint.h>

typedef struct MENUITEM
{
	char* Name;
	int16_t x;
	int16_t y;
	BOOL Enabled;
	void(*Action)(void);
} MENUITEM;

typedef struct MENU
{
	char* Name;
	uint8_t SelectedItem;
	uint8_t ItemCount;
	MENUITEM** Items;
} MENU;

void MenuItem_TitleScreen_Resume(void);
void MenuItem_TitleScreen_NewGame(void);
void MenuItem_TitleScreen_Options(void);
void MenuItem_TitleScreen_Quit(void);

void MenuItem_Quit_No(void);
void MenuItem_Quit_Yes(void);

void MenuItem_Options_SFXVolume(void);
void MenuItem_Options_MusicVolume(void);
void MenuItem_Options_WindowSize(void);
void MenuItem_Options_Back(void);

void DrawSplashScreen(void);
void DrawTitleScreen(void);
void DrawOptionsScreen(void);
void DrawQuitScreen(void);
void DrawUnpluggedScreen(void);

// Title screen

MENUITEM gMI_ResumeGame   = { "Resume",   (GAME_RES_WIDTH / 2) - ((6 * 6) / 2), 100, FALSE, MenuItem_TitleScreen_Resume };
MENUITEM gMI_StartNewGame = { "New Game", (GAME_RES_WIDTH / 2) - ((8 * 6) / 2), 115, TRUE,  MenuItem_TitleScreen_NewGame };
MENUITEM gMI_Options      = { "Options",  (GAME_RES_WIDTH / 2) - ((7 * 6) / 2), 130, TRUE,  MenuItem_TitleScreen_Options };
MENUITEM gMI_Quit         = { "Quit",     (GAME_RES_WIDTH / 2) - ((4 * 6) / 2), 145, TRUE,  MenuItem_TitleScreen_Quit };

MENUITEM* gMI_TitleScreenItems[] = { &gMI_ResumeGame, &gMI_StartNewGame, &gMI_Options, &gMI_Quit };
MENU gMenu_TitleScreen = { "Title Screen Menu", 1, _countof(gMI_TitleScreenItems), gMI_TitleScreenItems };

MENUITEM gMI_Quit_No  = { "No",  ((GAME_RES_WIDTH / 2) - ((2 * 6) / 2) - 25), (GAME_RES_HEIGHT / 2) + 10, TRUE, MenuItem_Quit_No };
MENUITEM gMI_Quit_Yes = { "Yes", ((GAME_RES_WIDTH / 2) - ((3 * 6) / 2) + 25), (GAME_RES_HEIGHT / 2) + 10, TRUE, MenuItem_Quit_Yes };

MENUITEM* gMI_ExitScreenItems[] = { &gMI_Quit_No, &gMI_Quit_Yes };
MENU gMenu_QuitScreen = { "Are you sure you want to quit?", 0, _countof(gMI_ExitScreenItems), gMI_ExitScreenItems };

// Options screen
MENUITEM gMI_OptionsScreen_SFXVolume = { "SFX Volume:", (GAME_RES_WIDTH / 2) - ((11 * 6) / 2), 100, TRUE, MenuItem_Options_SFXVolume };
MENUITEM gMI_OptionsScreen_MusicVolume = { "Music Volume:", (GAME_RES_WIDTH / 2) - ((13 * 6) / 2), 115, TRUE, MenuItem_Options_MusicVolume };
MENUITEM gMI_OptionsScreen_Resolution = { "Window Size:", (GAME_RES_WIDTH / 2) - ((12 * 6) / 2), 130, TRUE, MenuItem_Options_WindowSize };
MENUITEM gMI_OptionsScreen_Back = { "Back:", (GAME_RES_WIDTH / 2) - ((5 * 6) / 2), 145, TRUE, MenuItem_Options_Back };

MENUITEM* gMI_OptionsScreenItems[] = { &gMI_OptionsScreen_SFXVolume, &gMI_OptionsScreen_MusicVolume, &gMI_OptionsScreen_Resolution, &gMI_OptionsScreen_Back };
MENU gMenu_OptionsScreen = { "Options", 0, _countof(gMI_OptionsScreenItems), gMI_OptionsScreenItems };