#pragma once
#define GAME_NAME "CGAME"
#define GAME_RES_WIDTH 384
#define GAME_RES_HEIGHT 240
#define GAME_BPP 32
#define GAME_DRAWING_AREA_MEMORY_SIZE (GAME_RES_WIDTH * GAME_RES_HEIGHT * (GAME_BPP / 8))
#define CALCULATE_AVG_FPS_EVERY_X_FRAMES 100
#define TARGET_MICROSECONDS_PER_FRAME 16667

LRESULT CALLBACK MainWindowProc(_In_ HWND WindowsHandle, _In_ UINT Message, _In_ WPARAM WParam, _In_ LPARAM LParam);
DWORD CreateMainGameWindow(_In_ HANDLE Inst);
BOOL GameIsAlreadyRunning(void);

void ProcessPlayerInput(void);
void RenderFrameGraphics(void);

#pragma warning(disable: 4820) // Disable structure padding warning
#pragma warning(disable: 4710) // Disable function inline warning
typedef struct GAMEBITMAP
{
	BITMAPINFO BitmapInfo;
	void* Memory;
} GAMEBITMAP;

typedef struct PIXEL
{
	uint8_t Blue;
	uint8_t Green;
	uint8_t Red;
	uint8_t Alpha;
} PIXEL32;

typedef struct GAMEPERFDATA
{
	uint64_t TotalFramesRendered;
	float RawFPSAverage;
	float CookedFPSAverage;

	int64_t PerfFrequency;

	MONITORINFO MonitorInfo;
	int32_t MonitorWidth;
	int32_t MonitorHeight;
} GAMEPERFDATA;