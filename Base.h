#pragma once
#define ASSERT(Expression) if (!(Expression)) { *(int*)0 = 0; }
#define GAME_NAME "CGAME"
#define GAME_RES_WIDTH 384
#define GAME_RES_HEIGHT 240
#define GAME_BPP 32
#define GAME_DRAWING_AREA_MEMORY_SIZE (GAME_RES_WIDTH * GAME_RES_HEIGHT * (GAME_BPP / 8))
#define CALCULATE_AVG_FPS_EVERY_X_FRAMES 120
#define TARGET_MICROSECONDS_PER_FRAME 16667ULL
#define SIMD

#define SUIT_0 0
#define SUIT_1 1
#define SUIT_2 2
#define FACING_DOWN_0  0
#define FACING_DOWN_1  1
#define FACING_DOWN_2  2
#define FACING_LEFT_0  3
#define FACING_LEFT_1  4
#define FACING_LEFT_2  5
#define FACING_RIGHT_0 6
#define FACING_RIGHT_1 7
#define FACING_RIGHT_2 8
#define FACING_UP_0    9
#define FACING_UP_1    10
#define FACING_UP_2    11

typedef enum DIRECTION { Down = FACING_DOWN_0,
						 Left = FACING_LEFT_0,
						 Right = FACING_RIGHT_0,
						 Up = FACING_UP_0 } DIRECTION;

typedef enum LOGLEVEL { None, Error, Warning, Informational, Debug } LOGLEVEL;

#define FONT_SHEET_ROW_SIZE 98
#define LOG_FILE_NAME GAME_NAME ".log"

LRESULT CALLBACK MainWindowProc(_In_ HWND WindowsHandle, _In_ UINT Message, _In_ WPARAM WParam, _In_ LPARAM LParam);
DWORD CreateMainGameWindow(void);
BOOL GameIsAlreadyRunning(void);

void ProcessPlayerInput(void);
void RenderFrameGraphics(void);

#pragma warning(disable: 4820) // Disable structure padding warning
#pragma warning(disable: 5045) // Disable Qspectre vulnerability warning
#pragma warning(disable: 4710) // Disable function inline warning

typedef LONG(NTAPI* _NtQueryTimerResolution) (OUT PULONG MinimumResolution, OUT PULONG MaximumResolution, OUT PULONG CurrentResolution);

_NtQueryTimerResolution ntQueryTimerResolution;

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

	BOOL DisplayDebugInfo;
	ULONG MinimumTimerResolution;
	ULONG MaximumTimerResolution;
	ULONG CurrentTimerResolution;

	DWORD HandleCount;
	DWORD PrivateBytes;

	PROCESS_MEMORY_COUNTERS_EX MemInfo;
	SYSTEM_INFO SystemInfo;
	uint16_t CPUCount;
	int64_t CurrentSystemTime;
	int64_t PreviousSystemTime;
	double CPUPercent;
} GAMEPERFDATA;

typedef struct PLAYER
{
	char Name[12];
	GAMEBITMAP Sprite[3][12];
	int16_t ScreenPosX;
	int16_t ScreenPosY;

	uint8_t MovementRemaining;
	uint8_t Direction;
	uint8_t CurrentArmour;
	uint8_t SpriteIndex;
} PLAYER;

typedef struct REGISTRYPARAMS
{
	DWORD LogLevel;
} REGISTRYPARAMS;

DWORD InitializeHero(void);
DWORD Load32BppBitmapFromFile(_In_ char* FileName, _Inout_ GAMEBITMAP* GameBitmap);
void Blit32BppBitmapToBuffer(_In_ GAMEBITMAP* GameBitmap, _In_ uint16_t x, _In_ uint16_t y);
void BlitStringToBuffer(_In_ char* String, _In_ GAMEBITMAP* FontSheet, _In_ PIXEL32* Colour, _In_ uint16_t x, _In_ uint16_t y);

DWORD LoadRegistryParameters(void);
void LogMessageA(_In_ DWORD LogLevel, _In_ char* Message, _In_ ...);
void DrawDebugInfo(void);

#ifdef SIMD
void ClearScreen(_In_ __m128i* Colour);
#else
void ClearScreen(_In_ PIXEL32* Pixel);
#endif