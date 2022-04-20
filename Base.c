// Externals at warning level 3
#include <stdio.h>
#include <windows.h>
#include <psapi.h>
#include <immintrin.h>
#include <emmintrin.h>
#include <xaudio2.h>
#include <Xinput.h>
#include <stdint.h>
#include "Base.h"
#include "Menus.h"

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "XAudio2.lib")
#pragma comment(lib, "XInput.lib")

HWND gGameWindow;
BOOL gGameIsRunning;
GAMEBITMAP gBackBuffer;
GAMEBITMAP g6x7Font;
GAMEPERFDATA gPerformanceData;
PLAYER gPlayer;
BOOL gWindowHasFocus;
REGISTRYPARAMS gRegistryParams;

XINPUT_STATE gGamepadState;
int8_t gGamepadID = -1;
GAMEINPUT gGameInput;

IXAudio2* gXAudio;
IXAudio2MasteringVoice* gXAudio2MasteringVoice;
IXAudio2SourceVoice* gXAudioSFXSourceVoice[NUMBER_OF_SFX_SOURCE_VOICES];
IXAudio2SourceVoice* gXAudioMusicSourceVoice;
uint8_t gSFXSourceVoiceSelector;
float gSFXVolume = 0.5f;
float gMusicVolume = 0.5f;

GAMESOUND gMenuNavigate;
GAMESOUND gMenuChoose;

GAMESTATE gCurrentGameState = GS_TITLE;
GAMESTATE gPreviousGameState;
GAMESTATE gDesiredGameState;

int WinMain(_In_ HINSTANCE Inst, _In_opt_ HINSTANCE InstPrev, _In_ PSTR CmdLine, _In_ int CmdShow)
{
    UNREFERENCED_PARAMETER(Inst);
    UNREFERENCED_PARAMETER(InstPrev);
    UNREFERENCED_PARAMETER(CmdLine);
    UNREFERENCED_PARAMETER(CmdShow);

    MSG message = { 0 };
    int64_t frameStart;
    int64_t frameEnd;
    int64_t elapsedMs;
    int64_t elapsedMsAccumulatorRaw = 0;
    int64_t elapsedMsAccumulatorCooked = 0;

    FILETIME processCreationTime = { 0 };
    FILETIME processExitTime = { 0 };
    int64_t currentUserCPUTime = 0;
    int64_t currentKernelCPUTime = 0;
    int64_t previousUserCPUTime = 0;
    int64_t previousKernelCPUTime = 0;

    HANDLE processHandle = GetCurrentProcess();

    if (LoadRegistryParameters() != ERROR_SUCCESS)
    {
        goto Return;
    }

    LogMessageA(6, "test");

    LogMessageA(LL_INFORMATIONAL, "[%s] Informational", __FUNCTION__);
    LogMessageA(LL_WARNING, "[%s] Warning", __FUNCTION__);
    LogMessageA(LL_ERROR, "[%s] Error", __FUNCTION__);
    LogMessageA(LL_DEBUG, "[%s] Debug", __FUNCTION__);

    HMODULE ntDllModuleHandle;

    if ((ntDllModuleHandle = GetModuleHandleA("ntdll.dll")) == NULL)
    {
        LogMessageA(LL_ERROR, "[%s] Couldn't load ntdll.dll!, Error 0x%08lx!", __FUNCTION__, GetLastError());
        MessageBoxA(NULL, "Couldn't load ntdll.dll!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((ntQueryTimerResolution = (_NtQueryTimerResolution)GetProcAddress(ntDllModuleHandle, "NtQueryTimerResolution")) == NULL)
    {
        LogMessageA(LL_ERROR, "[%s] Couldn't find the NtQueryTimerResolution function in ntdll.dll!, GetProcAddress failed! Error 0x%08lx!", __FUNCTION__, GetLastError());
        MessageBoxA(NULL, "Couldn't find the NtQueryTimerResolution function in ntdll.dll!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    ntQueryTimerResolution(&gPerformanceData.MinimumTimerResolution, &gPerformanceData.MaximumTimerResolution, &gPerformanceData.CurrentTimerResolution);
    GetSystemInfo(&gPerformanceData.SystemInfo);
    GetSystemTimeAsFileTime((FILETIME*)&gPerformanceData.CurrentSystemTime);

    if (GameIsAlreadyRunning())
    {
        LogMessageA(LL_WARNING, "[%s] Another instance is running, GetProcAddress failed!", __FUNCTION__);
        MessageBoxA(NULL, "Another instance is running", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if (timeBeginPeriod(1) == TIMERR_NOCANDO)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to set global timer resolution", __FUNCTION__);
        MessageBoxA(NULL, "Failed to set global timer resolution", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if (SetPriorityClass(processHandle, HIGH_PRIORITY_CLASS) == 0)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to set process priority", __FUNCTION__, GetLastError());
        MessageBoxA(NULL, "Failed to set process priority", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) == 0)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to set thread priority, Error 0x%08lx!", __FUNCTION__, GetLastError());
        MessageBoxA(NULL, "Failed to set thread priority", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if (CreateMainGameWindow() != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] CreateMainGameWindow failed!", __FUNCTION__);
        goto Return;
    }

    if ((Load32BppBitmapFromFile(".\\Assets\\6x7Font.bmpx", &g6x7Font)) != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Loading 6x7font.bmpx failed!", __FUNCTION__);
        MessageBoxA(NULL, "Failed to load font!", "Error!", MB_ICONERROR | MB_OK);
        goto Return;
    }

    if (InitializeSoundEngine() != S_OK)
    {
        LogMessageA(LL_ERROR, "[%s] InitializeSoundEngine failed!", __FUNCTION__);
        goto Return;
    }

    if (LoadWavFromFile(".\\Assets\\MenuNavigate.wav", &gMenuNavigate) != ERROR_SUCCESS)
    {
        LogMessageA(NULL, "[%s] LoadWavFromFile failed!", __FUNCTION__);
        goto Return;
    }

    if (LoadWavFromFile(".\\Assets\\MenuChoose.wav", &gMenuChoose) != ERROR_SUCCESS)
    {
        LogMessageA(NULL, "[%s] LoadWavFromFile failed!", __FUNCTION__);
        goto Return;
    }

    QueryPerformanceFrequency((LARGE_INTEGER*) & gPerformanceData.PerfFrequency);
    gPerformanceData.DisplayDebugInfo = TRUE;

    gBackBuffer.BitmapInfo.bmiHeader.biSize = sizeof(gBackBuffer.BitmapInfo.bmiHeader);
    gBackBuffer.BitmapInfo.bmiHeader.biWidth = GAME_RES_WIDTH;
    gBackBuffer.BitmapInfo.bmiHeader.biHeight = GAME_RES_HEIGHT;
    gBackBuffer.BitmapInfo.bmiHeader.biBitCount = GAME_BPP;
    gBackBuffer.BitmapInfo.bmiHeader.biCompression = BI_RGB;
    gBackBuffer.BitmapInfo.bmiHeader.biPlanes = 1;

    gBackBuffer.Memory = VirtualAlloc(NULL, GAME_DRAWING_AREA_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (gBackBuffer.Memory == NULL)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to allocate memory for drawing surface, Error 0x%08lx!", __FUNCTION__, GetLastError());
        MessageBoxA(NULL, "Failed to allocate memory for drawing surface!", "Error!", MB_ICONERROR | MB_OK);
        goto Return;
    }

    if (InitializeHero() != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Failed to initialize hero!, Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Failed to initialize hero!", "Error!", MB_ICONERROR | MB_OK);
    }

    gGameIsRunning = TRUE;

    while (gGameIsRunning)
    {
        QueryPerformanceCounter((LARGE_INTEGER*)&frameStart);

        while (PeekMessageA(&message, gGameWindow, 0, 0, PM_REMOVE))
        {
            DispatchMessageA(&message);
        }

        ProcessPlayerInput();
        RenderFrameGraphics();

        QueryPerformanceCounter((LARGE_INTEGER*)&frameEnd);

        elapsedMs = frameEnd - frameStart;
        elapsedMs *= 1000000;
        elapsedMs /= gPerformanceData.PerfFrequency;

        gPerformanceData.TotalFramesRendered++;
        elapsedMsAccumulatorRaw += elapsedMs;

        while (elapsedMs <= TARGET_MICROSECONDS_PER_FRAME)
        {
            elapsedMs = frameEnd - frameStart;
            elapsedMs *= 1000000;
            elapsedMs /= gPerformanceData.PerfFrequency;

            QueryPerformanceCounter((LARGE_INTEGER*)&frameEnd);

            if (elapsedMs < ((int64_t)TARGET_MICROSECONDS_PER_FRAME * 0.75f))
            {
                Sleep(1);
            }
        }

        elapsedMsAccumulatorCooked += elapsedMs;

        if (gPerformanceData.TotalFramesRendered % CALCULATE_AVG_FPS_EVERY_X_FRAMES == 0)
        {
            GetSystemTimeAsFileTime((FILETIME*)&gPerformanceData.CurrentSystemTime);

            FindFirstConnectedGamepad();

            GetProcessTimes(processHandle,
                &processCreationTime,
                &processExitTime,
                (FILETIME*)&currentKernelCPUTime,
                (FILETIME*)&currentUserCPUTime);

            gPerformanceData.CPUPercent  = (double)(currentKernelCPUTime - previousKernelCPUTime) + (currentUserCPUTime - previousUserCPUTime);
            gPerformanceData.CPUPercent /= (gPerformanceData.CurrentSystemTime - gPerformanceData.PreviousSystemTime);
            gPerformanceData.CPUPercent /= gPerformanceData.SystemInfo.dwNumberOfProcessors;
            gPerformanceData.CPUPercent *= 100;

            GetProcessHandleCount(processHandle, &gPerformanceData.HandleCount);
            K32GetProcessMemoryInfo(processHandle, (PROCESS_MEMORY_COUNTERS*)& gPerformanceData.MemInfo, sizeof(gPerformanceData.MemInfo));

            gPerformanceData.RawFPSAverage = 1.0f / ((elapsedMsAccumulatorRaw / CALCULATE_AVG_FPS_EVERY_X_FRAMES) * 0.000001f);
            gPerformanceData.CookedFPSAverage = 1.0f / ((elapsedMsAccumulatorCooked / CALCULATE_AVG_FPS_EVERY_X_FRAMES) * 0.000001f);

            elapsedMsAccumulatorRaw = 0;
            elapsedMsAccumulatorCooked = 0;

            previousKernelCPUTime = currentKernelCPUTime;
            previousUserCPUTime = currentUserCPUTime;
            gPerformanceData.PreviousSystemTime = gPerformanceData.CurrentSystemTime;
        }
    }

Return:
	return 0;
}

//===========================================================
// Function Definitions
//===========================================================

LRESULT CALLBACK MainWindowProc(_In_ HWND WindowsHandle, _In_ UINT Message, _In_ WPARAM WParam, _In_ LPARAM LParam)
{
    LRESULT result = 0;
    switch (Message)
    {
    case WM_CLOSE:
        gGameIsRunning = FALSE;
        PostQuitMessage(0);
        break;

    case WM_ACTIVATE:
        if (WParam == 0)
        {
            gWindowHasFocus = FALSE;
        }
        else
        {
            ShowCursor(FALSE);
            gWindowHasFocus = TRUE;
        }
        break;

    default:
        result = DefWindowProcA(WindowsHandle, Message, WParam, LParam);
    }
    return result;
}

DWORD CreateMainGameWindow(void)
{
    DWORD result = ERROR_SUCCESS;

    WNDCLASSEXA windowClass = { 0 };

    windowClass.cbSize        = sizeof(WNDCLASSEX);
    windowClass.style         = 0;
    windowClass.lpfnWndProc   = MainWindowProc;
    windowClass.cbClsExtra    = 0;
    windowClass.cbWndExtra    = 0;
    windowClass.hInstance     = GetModuleHandleA(NULL);
    windowClass.hIcon         = LoadIconA(NULL, IDI_APPLICATION);
    windowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    windowClass.hbrBackground = CreateSolidBrush(RGB(255, 0, 255));
    windowClass.lpszMenuName  = NULL;
    windowClass.lpszClassName = GAME_NAME "_WINDOWCLASS";
    windowClass.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

    // SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    if (!RegisterClassExA(&windowClass))
    {
        result = GetLastError();
        LogMessageA(LL_ERROR, "[%s] Window Registration failed! Error 0x%08lx!", __FUNCTION__, result);
        MessageBoxA(NULL, "Window Registration failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    gGameWindow = CreateWindowExA(0, windowClass.lpszClassName, "Window Title", WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (gGameWindow == NULL)
    {
        result = GetLastError();
        LogMessageA(LL_ERROR, "[%s] CreateWindowExA failed! Error 0x%08lx!", __FUNCTION__, result);
        MessageBox(NULL, "Window Creation failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    gPerformanceData.MonitorInfo.cbSize = sizeof(MONITORINFO);

    if (GetMonitorInfoA(MonitorFromWindow(gGameWindow, MONITOR_DEFAULTTOPRIMARY), &gPerformanceData.MonitorInfo) == 0)
    {
        result = ERROR_MONITOR_NO_DESCRIPTOR;
        LogMessageA(LL_ERROR, "[%s] GetMonitorInfoA(MonitorFromWindow()) failed! Error 0x%08lx!", __FUNCTION__, result);
        goto Return;
    }

    gPerformanceData.MonitorWidth = gPerformanceData.MonitorInfo.rcMonitor.right - gPerformanceData.MonitorInfo.rcMonitor.left;
    gPerformanceData.MonitorHeight = gPerformanceData.MonitorInfo.rcMonitor.bottom - gPerformanceData.MonitorInfo.rcMonitor.top;

    if (SetWindowLongPtrA(gGameWindow, GWL_STYLE, WS_VISIBLE) == 0)
    {
        result = GetLastError();
        LogMessageA(LL_ERROR, "[%s] SetWindowLongPtrA() failed! Error 0x%08lx!", __FUNCTION__, result);
        goto Return;
    }

    if (SetWindowPos(
        gGameWindow,
        HWND_TOP,
        gPerformanceData.MonitorInfo.rcMonitor.left,
        gPerformanceData.MonitorInfo.rcMonitor.top,
        gPerformanceData.MonitorWidth,
        gPerformanceData.MonitorHeight,
        SWP_NOOWNERZORDER | SWP_FRAMECHANGED) == 0)
    {
        result = GetLastError();
        LogMessageA(LL_ERROR, "[%s] SetWindowPos() failed! Error 0x%08lx!", __FUNCTION__, result);
        goto Return;
    }

Return:
    return result;
}

BOOL GameIsAlreadyRunning(void)
{
    HANDLE mutex = NULL;
    mutex = CreateMutexA(NULL, FALSE, GAME_NAME "_GameMutex");

    return GetLastError() == ERROR_ALREADY_EXISTS ? TRUE : FALSE;
}

void ProcessPlayerInput(void)
{
    if (!gWindowHasFocus)
    {
        return;
    }

    gGameInput.EscapeKeyIsDown = GetAsyncKeyState(VK_ESCAPE);
    gGameInput.DebugKeyIsDown = GetAsyncKeyState(VK_F1);

    gGameInput.LeftKeyIsDown = GetAsyncKeyState(VK_LEFT) | GetAsyncKeyState(0x41);
    gGameInput.RightKeyIsDown = GetAsyncKeyState(VK_RIGHT) | GetAsyncKeyState(0x44);
    gGameInput.UpKeyIsDown = GetAsyncKeyState(VK_UP) | GetAsyncKeyState(0x57);
    gGameInput.DownKeyIsDown = GetAsyncKeyState(VK_DOWN) | GetAsyncKeyState(0x53);

    gGameInput.ChooseKeyIsDown = GetAsyncKeyState(VK_RETURN);

    if (gGamepadID >= 0)
    {
        if (XInputGetState(gGamepadID, &gGamepadState) == ERROR_SUCCESS)
        {
            gGameInput.EscapeKeyIsDown |= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK;
            gGameInput.LeftKeyIsDown   |= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
            gGameInput.RightKeyIsDown  |= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
            gGameInput.UpKeyIsDown     |= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP;
            gGameInput.DownKeyIsDown   |= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
            gGameInput.ChooseKeyIsDown |= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_A;
        }
        else
        {
            gGamepadID = -1;
            gPreviousGameState = gCurrentGameState;
            gCurrentGameState = GS_UNPLUGGED;
        }
    }

    if (gGameInput.DebugKeyIsDown && !gGameInput.DebugKeyWasDown)
    {
        gPerformanceData.DisplayDebugInfo = !gPerformanceData.DisplayDebugInfo;
    }

    switch (gCurrentGameState)
    {
    case GS_SPLASH:
        SplashInput();
        break;
    case GS_TITLE:
        TitleInput();
        break;
    case GS_OVERWORLD:
        OverworldInput();
        break;
    case GS_BATTLE:
        break;
    case GS_OPTIONS:
        OptionsInput();
        break;
    case GS_QUIT:
        QuitInput();
        break;
    case GS_UNPLUGGED:
        UnpluggedInput();
        break;
    default:
        ASSERT(FALSE, "Unrecognized GameState");
    }

    gGameInput.EscapeKeyWasDown = gGameInput.EscapeKeyIsDown;
    gGameInput.LeftKeyWasDown = gGameInput.LeftKeyIsDown;
    gGameInput.RightKeyWasDown = gGameInput.RightKeyIsDown;
    gGameInput.UpKeyWasDown = gGameInput.UpKeyIsDown;
    gGameInput.DownKeyWasDown = gGameInput.DownKeyIsDown;
    gGameInput.ChooseKeyWasDown = gGameInput.ChooseKeyIsDown;
}

void RenderFrameGraphics(void)
{
    switch (gCurrentGameState)
    {
    case GS_SPLASH:
        DrawSplashScreen();
        break;
    case GS_TITLE:
        DrawTitleScreen();
        break;
    case GS_OVERWORLD:
        break;
    case GS_BATTLE:
        break;
    case GS_OPTIONS:
        DrawOptionsScreen();
        break;
    case GS_QUIT:
        DrawQuitScreen();
        break;
    case GS_UNPLUGGED:
        DrawUnpluggedScreen();
        break;
    default:
        ASSERT(FALSE, "Unrecognized GameState");
    }

//#if INSTRUCTIONS == AVX
//    __m256i OctoPixel = { 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff,
//                          0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff };
//    ClearScreen(&OctoPixel);
//#elif INSTRUCTIONS == SIMD
//    __m128i QuadPixel = { 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff };
//    ClearScreen(&QuadPixel);
//#else
//    PIXEL32 Pixel = { 0x7f, 0x00, 0x00, 0xff };
//    ClearScreen(&Pixel);
//#endif
//
//    Blit32BppBitmapToBuffer(&gPlayer.Sprite[gPlayer.CurrentArmour][gPlayer.Direction + gPlayer.SpriteIndex], gPlayer.ScreenPosX, gPlayer.ScreenPosY);

    if (gPerformanceData.DisplayDebugInfo)
    {
        DrawDebugInfo();
    }

    HDC DeviceContext = GetDC(gGameWindow);

    StretchDIBits(
        DeviceContext,
        0,
        0,
        gPerformanceData.MonitorWidth,
        gPerformanceData.MonitorHeight,
        0,
        0,
        GAME_RES_WIDTH,
        GAME_RES_HEIGHT,
        gBackBuffer.Memory,
        &gBackBuffer.BitmapInfo,
        DIB_RGB_COLORS,
        SRCCOPY);

    ReleaseDC(gGameWindow, DeviceContext);
}

#ifdef AVX
__forceinline void ClearScreen(_In_ __m256i* Colour)
{
#define PIXEL32S_PER_M256I 8
    for (int i = 0; i < (GAME_RES_WIDTH * GAME_RES_HEIGHT) / PIXEL32S_PER_M256I; i++)
    {
        _mm256_store_si256((__m256i*)gBackBuffer.Memory + i, *Colour);
    }
}
#elif define SSE2
__forceinline void ClearScreen(_In_ __m128i* Colour)
{
    for (int i = 0; i < GAME_RES_WIDTH * GAME_RES_HEIGHT; i += 4)
    {
        _mm_store_si128((PIXEL32*)gBackBuffer.Memory + i, *Colour);
    }
}
#else
__forceinline void ClearScreen(_In_ PIXEL32* Pixel)
{
    for (int i = 0; i < GAME_RES_WIDTH * GAME_RES_HEIGHT; i++)
    {
        memcpy((PIXEL32*)gBackBuffer.Memory + i, &Pixel, sizeof(PIXEL32));
    }
}
#endif

DWORD InitializeHero(void)
{
    DWORD error = ERROR_SUCCESS;

    gPlayer.ScreenPosX = 32;
    gPlayer.ScreenPosY = 32;
    gPlayer.CurrentArmour = SUIT_0;
    gPlayer.Direction = DOWN;
    gPlayer.MovementRemaining = 0;
    gPlayer.Active = FALSE;
    
    if ((error = Load32BppBitmapFromFile(".\\Assets\\Standing_Down.bmpx", &gPlayer.Sprite[SUIT_0][FACING_DOWN_0])) != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Load32BppBitmapFromFile \"Standing_Down.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Down_1.bmpx", &gPlayer.Sprite[SUIT_0][FACING_DOWN_1])) != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Load32BppBitmapFromFile \"Walk_Down_1.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Down_2.bmpx", &gPlayer.Sprite[SUIT_0][FACING_DOWN_2])) != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Load32BppBitmapFromFile \"Walk_Down_2.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Standing_Left.bmpx", &gPlayer.Sprite[SUIT_0][FACING_LEFT_0])) != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Load32BppBitmapFromFile \"Standing_Left.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Left_1.bmpx", &gPlayer.Sprite[SUIT_0][FACING_LEFT_1])) != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Load32BppBitmapFromFile \"Walk_Left_1.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Left_2.bmpx", &gPlayer.Sprite[SUIT_0][FACING_LEFT_2])) != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Load32BppBitmapFromFile \"Walk_Left_2.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Standing_Right.bmpx", &gPlayer.Sprite[SUIT_0][FACING_RIGHT_0])) != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Load32BppBitmapFromFile \"Standing_Right.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Right_1.bmpx", &gPlayer.Sprite[SUIT_0][FACING_RIGHT_1])) != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Load32BppBitmapFromFile \"Walk_Right_1.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Right_2.bmpx", &gPlayer.Sprite[SUIT_0][FACING_RIGHT_2])) != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Load32BppBitmapFromFile \"Walk_Right_2.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Standing_Up.bmpx", &gPlayer.Sprite[SUIT_0][FACING_UP_0])) != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Load32BppBitmapFromFile \"Standing_Up.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Up_1.bmpx", &gPlayer.Sprite[SUIT_0][FACING_UP_1])) != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Load32BppBitmapFromFile \"Walk_Up_1.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Up_2.bmpx", &gPlayer.Sprite[SUIT_0][FACING_UP_2])) != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] Load32BppBitmapFromFile \"Walk_Up_2.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

Return:
    return error;
}

DWORD Load32BppBitmapFromFile(_In_ char* FileName, _Inout_ GAMEBITMAP* GameBitmap)
{
    DWORD error = ERROR_SUCCESS;
    HANDLE fileHandle = INVALID_HANDLE_VALUE;

    WORD bitmapHeader = 0;
    DWORD pixelDataOffset = 0;
    DWORD numberOfBytesRead = 2;

    if ((fileHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
    {
        error = GetLastError();
        goto Return;
    }

    if (ReadFile(fileHandle, &bitmapHeader, 2, &numberOfBytesRead, NULL) == 0)
    {
        error = GetLastError();
        goto Return;
    }

    if (bitmapHeader != 0x4d42) // "BM" backwards
    {
        error = ERROR_FILE_INVALID;
        goto Return;
    }

    if (SetFilePointer(fileHandle, 0xA, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
    {
        error = GetLastError();
        goto Return;
    }

    if (ReadFile(fileHandle, &pixelDataOffset, sizeof(DWORD), &numberOfBytesRead, NULL) == 0)
    {
        error = GetLastError();
        goto Return;
    }

    if (SetFilePointer(fileHandle, 0xE, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
    {
        error = GetLastError();
        goto Return;
    }

    if (ReadFile(fileHandle, &GameBitmap->BitmapInfo.bmiHeader, sizeof(BITMAPINFOHEADER), &numberOfBytesRead, NULL) == 0)
    {
        error = GetLastError();
        goto Return;
    }

    if ((GameBitmap->Memory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, GameBitmap->BitmapInfo.bmiHeader.biSizeImage)) == NULL)
    {
        error = ERROR_NOT_ENOUGH_MEMORY;
        goto Return;
    }

    if (SetFilePointer(fileHandle, pixelDataOffset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
    {
        error = GetLastError();
        goto Return;
    }

    if (ReadFile(fileHandle, GameBitmap->Memory, GameBitmap->BitmapInfo.bmiHeader.biSizeImage, &numberOfBytesRead, NULL) == 0)
    {
        error = GetLastError();
        goto Return;
    }
Return:
    if (fileHandle && (fileHandle != INVALID_HANDLE_VALUE))
    {
        CloseHandle(fileHandle);
    }
    return error;
}

void Blit32BppBitmapToBuffer(_In_ GAMEBITMAP* GameBitmap, _In_ uint16_t x, _In_ uint16_t y)
{
    int32_t biWidth = GameBitmap->BitmapInfo.bmiHeader.biWidth;
    int32_t biHeight = GameBitmap->BitmapInfo.bmiHeader.biHeight;

    int32_t startingScreenPixel = ((GAME_RES_WIDTH * GAME_RES_HEIGHT) - GAME_RES_WIDTH) - (GAME_RES_WIDTH * y) + x;
    int32_t startingBitmapPixel = ((biWidth * biHeight) - biWidth);

    int32_t memoryOffset = 0;
    int32_t BitmapOffset = 0;

    PIXEL32 BitmapPixel = { 0 };
    //PIXEL32 BackgroundPixel = { 0 };

    for (int16_t yPixel = 0; yPixel < biHeight; yPixel++)
    {
        for (int16_t xPixel = 0; xPixel < biWidth; xPixel++)
        {
            memoryOffset = startingScreenPixel + xPixel - (GAME_RES_WIDTH * yPixel);
            BitmapOffset = startingBitmapPixel + xPixel - (biWidth * yPixel);

            memcpy_s(&BitmapPixel, sizeof(PIXEL32*), (PIXEL32*)GameBitmap->Memory + BitmapOffset, sizeof(PIXEL32));

            if (BitmapPixel.Alpha == 255)
            {
                memcpy_s((PIXEL32*)gBackBuffer.Memory + memoryOffset, sizeof(PIXEL32*), &BitmapPixel, sizeof(PIXEL32));
            }
        }
    }
}

void BlitStringToBuffer(_In_ char* String, _In_ GAMEBITMAP* FontSheet, _In_ PIXEL32* Colour, _In_ uint16_t x, _In_ uint16_t y)
{
    uint16_t charWidth = (uint16_t)FontSheet->BitmapInfo.bmiHeader.biWidth / FONT_SHEET_ROW_SIZE;
    uint16_t charHeight = (uint16_t)FontSheet->BitmapInfo.bmiHeader.biHeight;

    uint16_t bytesPerChar = (charWidth * charHeight * (FontSheet->BitmapInfo.bmiHeader.biBitCount / 8));
    uint16_t stringLength = (uint16_t)strlen(String);

    GAMEBITMAP stringBitmap = { 0 };
    stringBitmap.BitmapInfo.bmiHeader.biBitCount = GAME_BPP;
    stringBitmap.BitmapInfo.bmiHeader.biHeight = charHeight;
    stringBitmap.BitmapInfo.bmiHeader.biWidth = charWidth * stringLength;
    stringBitmap.BitmapInfo.bmiHeader.biPlanes = 1;
    stringBitmap.BitmapInfo.bmiHeader.biCompression = BI_RGB;
    stringBitmap.Memory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ((size_t)bytesPerChar * (size_t)stringLength));

    for (int ch = 0; ch < stringLength; ch++)
    {
        int widthOffset = 0;
        switch (String[ch])
        {
        case 'A':
            widthOffset = 0;
            break;
        case 'B':
            widthOffset = charWidth;
            break;
        case 'C':
            widthOffset = charWidth * 2;
            break;
        case 'D':
            widthOffset = charWidth * 3;
            break;
        case 'E':
            widthOffset = charWidth * 4;
            break;
        case 'F':
            widthOffset = charWidth * 5;
            break;
        case 'G':
            widthOffset = charWidth * 6;
            break;
        case 'H':
            widthOffset = charWidth * 7;
            break;
        case 'I':
            widthOffset = charWidth * 8;
            break;
        case 'J':
            widthOffset = charWidth * 9;
            break;
        case 'K':
            widthOffset = charWidth * 10;
            break;
        case 'L':
            widthOffset = charWidth * 11;
            break;
        case 'M':
            widthOffset = charWidth * 12;
            break;
        case 'N':
            widthOffset = charWidth * 13;
            break;
        case 'O':
            widthOffset = charWidth * 14;
            break;
        case 'P':
            widthOffset = charWidth * 15;
            break;
        case 'Q':
            widthOffset = charWidth * 16;
            break;
        case 'R':
            widthOffset = charWidth * 17;
            break;
        case 'S':
            widthOffset = charWidth * 18;
            break;
        case 'T':
            widthOffset = charWidth * 19;
            break;
        case 'U':
            widthOffset = charWidth * 20;
            break;
        case 'V':
            widthOffset = charWidth * 21;
            break;
        case 'W':
            widthOffset = charWidth * 22;
            break;
        case 'X':
            widthOffset = charWidth * 23;
            break;
        case 'Y':
            widthOffset = charWidth * 24;
            break;
        case 'Z':
            widthOffset = charWidth * 25;
            break;
        case 'a':
            widthOffset = charWidth * 26;
            break;
        case 'b':
            widthOffset = charWidth * 27;
            break;
        case 'c':
            widthOffset = charWidth * 28;
            break;
        case 'd':
            widthOffset = charWidth * 29;
            break;
        case 'e':
            widthOffset = charWidth * 30;
            break;
        case 'f':
            widthOffset = charWidth * 31;
            break;
        case 'g':
            widthOffset = charWidth * 32;
            break;
        case 'h':
            widthOffset = charWidth * 33;
            break;
        case 'i':
            widthOffset = charWidth * 34;
            break;
        case 'j':
            widthOffset = charWidth * 35;
            break;
        case 'k':
            widthOffset = charWidth * 36;
            break;
        case 'l':
            widthOffset = charWidth * 37;
            break;
        case 'm':
            widthOffset = charWidth * 38;
            break;
        case 'n':
            widthOffset = charWidth * 39;
            break;
        case 'o':
            widthOffset = charWidth * 40;
            break;
        case 'p':
            widthOffset = charWidth * 41;
            break;
        case 'q':
            widthOffset = charWidth * 42;
            break;
        case 'r':
            widthOffset = charWidth * 43;
            break;
        case 's':
            widthOffset = charWidth * 44;
            break;
        case 't':
            widthOffset = charWidth * 45;
            break;
        case 'u':
            widthOffset = charWidth * 46;
            break;
        case 'v':
            widthOffset = charWidth * 47;
            break;
        case 'w':
            widthOffset = charWidth * 48;
            break;
        case 'x':
            widthOffset = charWidth * 49;
            break;
        case 'y':
            widthOffset = charWidth * 50;
            break;
        case 'z':
            widthOffset = charWidth * 51;
            break;
        case '0':
            widthOffset = charWidth * 52;
            break;
        case '1':
            widthOffset = charWidth * 53;
            break;
        case '2':
            widthOffset = charWidth * 54;
            break;
        case '3':
            widthOffset = charWidth * 55;
            break;
        case '4':
            widthOffset = charWidth * 56;
            break;
        case '5':
            widthOffset = charWidth * 57;
            break;
        case '6':
            widthOffset = charWidth * 58;
            break;
        case '7':
            widthOffset = charWidth * 59;
            break;
        case '8':
            widthOffset = charWidth * 60;
            break;
        case '9':
            widthOffset = charWidth * 61;
            break;
        case '`':
            widthOffset = charWidth * 62;
            break;
        case '~':
            widthOffset = charWidth * 63;
            break;
        case '!':
            widthOffset = charWidth * 64;
            break;
        case '@':
            widthOffset = charWidth * 65;
            break;
        case '#':
            widthOffset = charWidth * 66;
            break;
        case '$':
            widthOffset = charWidth * 67;
            break;
        case '%':
            widthOffset = charWidth * 68;
            break;
        case '^':
            widthOffset = charWidth * 69;
            break;
        case '&':
            widthOffset = charWidth * 70;
            break;
        case '*':
            widthOffset = charWidth * 71;
            break;
        case '(':
            widthOffset = charWidth * 72;
            break;
        case ')':
            widthOffset = charWidth * 73;
            break;
        case '-':
            widthOffset = charWidth * 74;
            break;
        case '=':
            widthOffset = charWidth * 75;
            break;
        case '_':
            widthOffset = charWidth * 76;
            break;
        case '+':
            widthOffset = charWidth * 77;
            break;
        case '\\':
            widthOffset = charWidth * 78;
            break;
        case '|':
            widthOffset = charWidth * 79;
            break;
        case '[':
            widthOffset = charWidth * 80;
            break;
        case ']':
            widthOffset = charWidth * 81;
            break;
        case '{':
            widthOffset = charWidth * 82;
            break;
        case '}':
            widthOffset = charWidth * 83;
            break;
        case ';':
            widthOffset = charWidth * 84;
            break;
        case '\'':
            widthOffset = charWidth * 85;
            break;
        case ':':
            widthOffset = charWidth * 86;
            break;
        case '"':
            widthOffset = charWidth * 87;
            break;
        case ',':
            widthOffset = charWidth * 88;
            break;
        case '<':
            widthOffset = charWidth * 89;
            break;
        case '>':
            widthOffset = charWidth * 90;
            break;
        case '.':
            widthOffset = charWidth * 91;
            break;
        case '/':
            widthOffset = charWidth * 92;
            break;
        case '?':
            widthOffset = charWidth * 93;
            break;
        case ' ':
            widthOffset = charWidth * 94;
            break;
        case '»':
            widthOffset = charWidth * 95;
            break;
        case '«':
            widthOffset = charWidth * 96;
            break;
        case '\xf2':
            widthOffset = charWidth * 97;
            break;
        default:
            widthOffset = charWidth * 98;
            break;
        }

        int fontSheetOffset = 0;
        int stringBitmapOffset = 0;
        PIXEL32 fontSheetPixel = { 0 };
        int startingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + widthOffset;

        for (int yPixel = 0; yPixel < charHeight; yPixel++)
        {
            for (int xPixel = 0; xPixel < charWidth; xPixel++)
            {
                fontSheetOffset = startingFontSheetPixel + xPixel - (FontSheet->BitmapInfo.bmiHeader.biWidth * yPixel);
                stringBitmapOffset = (ch * charWidth) + ((stringBitmap.BitmapInfo.bmiHeader.biWidth * stringBitmap.BitmapInfo.bmiHeader.biHeight) -
                                     stringBitmap.BitmapInfo.bmiHeader.biWidth) + xPixel - (stringBitmap.BitmapInfo.bmiHeader.biWidth) * yPixel;

                memcpy_s(&fontSheetPixel, sizeof(PIXEL32), (PIXEL32*)FontSheet->Memory + fontSheetOffset, sizeof(PIXEL32));
                fontSheetPixel.Red = Colour->Red;
                fontSheetPixel.Green = Colour->Green;
                fontSheetPixel.Blue = Colour->Blue;
                memcpy_s((PIXEL32*)stringBitmap.Memory + stringBitmapOffset, sizeof(PIXEL32), &fontSheetPixel, sizeof(PIXEL32));
            }
        }
    }

    Blit32BppBitmapToBuffer(&stringBitmap, x, y);

    if (stringBitmap.Memory)
    {
        HeapFree(GetProcessHeap(), 0, stringBitmap.Memory);
    }
}

DWORD LoadRegistryParameters(void)
{
    DWORD result = ERROR_SUCCESS;

    HKEY regKey = NULL;
    DWORD regDisposition = 0;
    DWORD regBytesRead = sizeof(DWORD);
    result = RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\" GAME_NAME, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &regKey, &regDisposition);

    if (result != ERROR_SUCCESS)
    {
        LogMessageA(LL_ERROR, "[%s] RegCreateKey faield with error code 0x%08lx!", __FUNCTION__, result);
        goto Return;
    }

    if (regDisposition == REG_CREATED_NEW_KEY)
    {
        LogMessageA(LL_INFORMATIONAL, "[%s] Registry key did not exist; created new key HKCU\\SOFTWARE\\%s" __FUNCTION__, GAME_NAME);
    }
    else
    {
        LogMessageA(LL_INFORMATIONAL, "[%s] Opened existing registry key HCKU\\SOFTWARE\\%s", __FUNCTION__, GAME_NAME);
    }
    result = RegGetValueA(regKey, NULL, "LogLevel", RRF_RT_DWORD, NULL, (BYTE*)&gRegistryParams.LogLevel, &regBytesRead);

    if (result != ERROR_SUCCESS)
    {
        if (result == ERROR_FILE_NOT_FOUND)
        {
            result = LL_INFORMATIONAL;
            LogMessageA(LL_INFORMATIONAL, "[%s] Registry 'LogLevel' not found; using default of 0. (LOG_LEVEL_NONE)" __FUNCTION__);
            gRegistryParams.LogLevel = LL_NONE;
        }
        else
        {
            LogMessageA(LL_ERROR, "[%s] Failed to read the 'LogLevel' registry value! Error 0x%08lx!" __FUNCTION__, result);
            goto Return;
        }
    }

    LogMessageA(LL_INFORMATIONAL, "[%s] LogLevel is %d.", __FUNCTION__, gRegistryParams.LogLevel);

Return:
    if (regKey)
    {
        RegCloseKey(regKey);
    }

    return result;
}

void LogMessageA(_In_ DWORD LogLevel, _In_ char* Message, _In_ ...)
{
    size_t messageLength = strlen(Message);
    SYSTEMTIME time = { 0 };
    HANDLE logFileHandle = INVALID_HANDLE_VALUE;
    DWORD endOfFile = 0;
    DWORD numberOfBytesWritten = 0;
    char dateTimeString[96] = { 0 };
    char severityString[8] = { 0 };
    char formattedString[4096] = { 0 };
    int error = 0;

    if (gRegistryParams.LogLevel < LogLevel)
    {
        return;
    }

    if (messageLength < 1 || messageLength > 4096)
    {
        ASSERT(FALSE, "Message was either too long or too short");
    }

    switch (LogLevel)
    {
    case LL_NONE:
        return;
    case LL_INFORMATIONAL:
        strcpy_s(severityString, sizeof(severityString), "[INFO]");
        break;
    case LL_WARNING:
        strcpy_s(severityString, sizeof(severityString), "[WARN]");
        break;
    case LL_ERROR:
        strcpy_s(severityString, sizeof(severityString), "[ERROR]");
        break;
    case LL_DEBUG:
        strcpy_s(severityString, sizeof(severityString), "[DEBUG]");
        break;
    default:
        ASSERT(FALSE, "Unrecognized log level");
        break;
    }

    GetLocalTime(&time);

    va_list argPointer = NULL;
    va_start(argPointer, Message);
    _vsnprintf_s(formattedString, sizeof(formattedString), _TRUNCATE, Message, argPointer);
    va_end(argPointer);

    error = _snprintf_s(dateTimeString, sizeof(dateTimeString), _TRUNCATE, "\r\n[%02u/%02u/%u %02u:%02u:%02u.%03u]", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);

    if ((logFileHandle = CreateFileA(LOG_FILE_NAME, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
    {
        ASSERT(FALSE, "Failed to access log file");
    }

    endOfFile = SetFilePointer(logFileHandle, 0, NULL, FILE_END);
    WriteFile(logFileHandle, dateTimeString, (DWORD)strlen(dateTimeString), &numberOfBytesWritten, NULL);
    WriteFile(logFileHandle, severityString, (DWORD)strlen(severityString), &numberOfBytesWritten, NULL);
    WriteFile(logFileHandle, formattedString, (DWORD)strlen(formattedString), &numberOfBytesWritten, NULL);

    if (logFileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(logFileHandle);
    }
}

void DrawDebugInfo(void)
{
    char debugTextBuffer[64] = { 0 };

    PIXEL32 white = { 0xFF, 0xFF, 0xFF, 0xFF };

    sprintf_s(debugTextBuffer, _countof(debugTextBuffer), "FPS Raw: %.01f", gPerformanceData.RawFPSAverage);
    BlitStringToBuffer(debugTextBuffer, &g6x7Font, &white, 0, 0);

    sprintf_s(debugTextBuffer, _countof(debugTextBuffer), "FPS Cooked: %.01f", gPerformanceData.CookedFPSAverage);
    BlitStringToBuffer(debugTextBuffer, &g6x7Font, &white, 0, 8);

    sprintf_s(debugTextBuffer, _countof(debugTextBuffer), "Min Timer Res: %.02f", gPerformanceData.MinimumTimerResolution / 10000.0f);
    BlitStringToBuffer(debugTextBuffer, &g6x7Font, &white, 0, 16);

    sprintf_s(debugTextBuffer, _countof(debugTextBuffer), "Max Timer Res: %.02f", gPerformanceData.MaximumTimerResolution / 10000.0f);
    BlitStringToBuffer(debugTextBuffer, &g6x7Font, &white, 0, 24);

    sprintf_s(debugTextBuffer, _countof(debugTextBuffer), "Cur Timer Res: %.02f", gPerformanceData.CurrentTimerResolution / 10000.0f);
    BlitStringToBuffer(debugTextBuffer, &g6x7Font, &white, 0, 32);

    sprintf_s(debugTextBuffer, _countof(debugTextBuffer), "Handles: %lu", gPerformanceData.HandleCount);
    BlitStringToBuffer(debugTextBuffer, &g6x7Font, &white, 0, 40);

    sprintf_s(debugTextBuffer, _countof(debugTextBuffer), "Memory: %llu KB", gPerformanceData.MemInfo.PrivateUsage / 1024);
    BlitStringToBuffer(debugTextBuffer, &g6x7Font, &white, 0, 48);

    sprintf_s(debugTextBuffer, _countof(debugTextBuffer), "CPU: %.02f%%", gPerformanceData.CPUPercent);
    BlitStringToBuffer(debugTextBuffer, &g6x7Font, &white, 0, 56);

    sprintf_s(debugTextBuffer, _countof(debugTextBuffer), "Total Frames: %llu", gPerformanceData.TotalFramesRendered);
    BlitStringToBuffer(debugTextBuffer, &g6x7Font, &white, 0, 64);

    sprintf_s(debugTextBuffer, _countof(debugTextBuffer), "ScreenPos: (%d,%d)", gPlayer.ScreenPosX, gPlayer.ScreenPosY);
    BlitStringToBuffer(debugTextBuffer, &g6x7Font, &white, 0, 72);
}

void FindFirstConnectedGamepad(void)
{
    gGamepadID = -1;
    for (int8_t gamepadIndex = 0; gamepadIndex < XUSER_MAX_COUNT && gGamepadID == -1; gamepadIndex++)
    {
        XINPUT_STATE state = { 0 };

        if (XInputGetState(gamepadIndex, &state) == ERROR_SUCCESS)
        {
            gGamepadID = gamepadIndex;
        }
    }
}

HRESULT InitializeSoundEngine(void)
{
    HRESULT result = S_OK;
    WAVEFORMATEX sfxWaveFormat = { 0 };
    WAVEFORMATEX musicWaveFormat = { 0 };

    result = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (result != S_OK)
    {
        LogMessageA(LL_ERROR, "[%s] CoInitializeEx failed with 0x%8lx!" __FUNCTION__);
        goto Return;
    }

    result = XAudio2Create(&gXAudio, 0, XAUDIO2_ANY_PROCESSOR);
    if (FAILED(result))
    {
        LogMessageA(LL_ERROR, "[%s] XAudio2Create failed with 0x%8lx!" __FUNCTION__);
        goto Return;
    }

    result = gXAudio->lpVtbl->CreateMasteringVoice(gXAudio, &gXAudio2MasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL, 0);
    if (FAILED(result))
    {
        LogMessageA(LL_ERROR, "[%s] CreateMasteringVoice failed with 0x%8lx!" __FUNCTION__);
        goto Return;
    }

    sfxWaveFormat.wFormatTag = WAVE_FORMAT_PCM;
    sfxWaveFormat.nChannels = 1;
    sfxWaveFormat.nSamplesPerSec = 44100;
    sfxWaveFormat.nAvgBytesPerSec = sfxWaveFormat.nSamplesPerSec * sfxWaveFormat.nChannels * 2;
    sfxWaveFormat.nBlockAlign = sfxWaveFormat.nChannels * 2;
    sfxWaveFormat.wBitsPerSample = 16;
    sfxWaveFormat.cbSize = 0x6164;

    for (uint8_t counter = 0; counter < NUMBER_OF_SFX_SOURCE_VOICES; counter++)
    {
        result = gXAudio->lpVtbl->CreateSourceVoice(gXAudio, &gXAudioSFXSourceVoice[counter], &sfxWaveFormat, 0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL);
        if (result != S_OK)
        {
            LogMessageA(LL_ERROR, "[%s] CreateSourceVoice failed with 0x%8lx!" __FUNCTION__, result);
            goto Return;
        }

        gXAudioSFXSourceVoice[counter]->lpVtbl->SetVolume(gXAudioSFXSourceVoice[counter], gSFXVolume, XAUDIO2_COMMIT_NOW);
    }

    musicWaveFormat.wFormatTag = WAVE_FORMAT_PCM;
    musicWaveFormat.nChannels = 2;
    musicWaveFormat.nSamplesPerSec = 44100;
    musicWaveFormat.nAvgBytesPerSec = musicWaveFormat.nSamplesPerSec * musicWaveFormat.nChannels * 2;
    musicWaveFormat.nBlockAlign = musicWaveFormat.nChannels * 2;
    musicWaveFormat.wBitsPerSample = 16;
    musicWaveFormat.cbSize = 0;

    result = gXAudio->lpVtbl->CreateSourceVoice(gXAudio, &gXAudioMusicSourceVoice, &musicWaveFormat, 0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL);
    if (result != S_OK)
    {
        LogMessageA(LL_ERROR, "[%s] CreateSourceVoice failed with 0x%8lx!" __FUNCTION__, result);
        goto Return;
    }

    gXAudioMusicSourceVoice->lpVtbl->SetVolume(gXAudioMusicSourceVoice, gMusicVolume, XAUDIO2_COMMIT_NOW);

Return:
    return result;
}

DWORD LoadWavFromFile(_In_ char* FileName, _Inout_ GAMESOUND* GameSound)
{
    DWORD error = ERROR_SUCCESS;
    DWORD RIFF = 0;
    DWORD numberOfBytesRead = 0;

    uint16_t dataChunkOffset = 0;
    DWORD dataChunkSearcher = 0;
    BOOL dataChunkFound = FALSE;
    DWORD dataChunkSize = 0;

    HANDLE fileHandle = INVALID_HANDLE_VALUE;
    void* audioData = NULL;

    if ((fileHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
    {
        error = GetLastError();
        LogMessageA(LL_ERROR, "[%s] CreateFileA failed with 0x%08lx!", __FUNCTION__, error);
        goto Return;
    }

    if (ReadFile(fileHandle, &RIFF, sizeof(DWORD), &numberOfBytesRead, NULL) == 0)
    {
        error = GetLastError();
        LogMessageA(LL_ERROR, "[%s] ReadFile failed with 0x%08lx!", __FUNCTION__, error);
        goto Return;
    }

    if (RIFF != 0x46464952) // "RIFF" backwards
    {
        error = ERROR_FILE_INVALID;
        LogMessageA(LL_ERROR, "[%s] First 4 bytes of this file are not RIFF!", __FUNCTION__, error);
        goto Return;
    }

    if (SetFilePointer(fileHandle, 20, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
    {
        error = GetLastError();
        LogMessageA(LL_ERROR, "[%s] SetFilePointer failed with 0x%08lx!", __FUNCTION__, error);
        goto Return;
    }

    if (ReadFile(fileHandle, &GameSound->WaveFormat, sizeof(WAVEFORMATEX), &numberOfBytesRead, NULL) == 0)
    {
        error = GetLastError();
        LogMessageA(LL_ERROR, "[%s] This wav file did not meet the format requirements! Only PCM format, 44.1KHz, 16 bits per sample wav files are supported. 0x%08lx!", __FUNCTION__, error);
        goto Return;
    }

    if (GameSound->WaveFormat.nBlockAlign != ((GameSound->WaveFormat.nChannels * GameSound->WaveFormat.wBitsPerSample) / 8) ||
       (GameSound->WaveFormat.wFormatTag != WAVE_FORMAT_PCM) ||
       (GameSound->WaveFormat.wBitsPerSample != 16))
    {
        error = ERROR_SUCCESS;
        goto Return;
    }

    while (dataChunkFound == FALSE)
    {
        if (SetFilePointer(fileHandle, dataChunkOffset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
        {
            error = GetLastError();
            LogMessageA(LL_ERROR, "[%s] SetFilePointer failed with 0x%08lx!", __FUNCTION__, error);
            goto Return;
        }

        if (ReadFile(fileHandle, &dataChunkSearcher, sizeof(DWORD), &numberOfBytesRead, NULL) == 0)
        {
            error = GetLastError();
            LogMessageA(LL_ERROR, "[%s] This wav file did not meet the format requirements! Only PCM format, 44.1KHz, 16 bits per sample wav files are supported. 0x%08lx!", __FUNCTION__, error);
            goto Return;
        }

        if (dataChunkSearcher == 0x61746164) // "data" backwards
        {
            dataChunkFound = TRUE;
            break;
        }
        else
        {
            dataChunkOffset += 4;
        }

        if (dataChunkOffset > 256)
        {
            error = ERROR_DATATYPE_MISMATCH;
            LogMessageA(LL_ERROR, "[%s] Data chunk not found within the first 256 bits of this file! 0x%08lx!", __FUNCTION__, error);
            goto Return;
        }
    }

    if (SetFilePointer(fileHandle, dataChunkOffset + 4, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
    {
        error = GetLastError();
        LogMessageA(LL_ERROR, "[%s] SetFilePointer failed with 0x%08lx!", __FUNCTION__, error);
        goto Return;
    }

    if (ReadFile(fileHandle, &dataChunkSize, sizeof(DWORD), &numberOfBytesRead, NULL) == 0)
    {
        error = GetLastError();
        LogMessageA(LL_ERROR, "[%s] ReadFile failed with 0x%08lx!", __FUNCTION__, error);
        goto Return;
    }

    audioData = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dataChunkSize);

    if (audioData == NULL)
    {
        error = ERROR_NOT_ENOUGH_MEMORY;
        LogMessageA(LL_ERROR, "[%s] HeapAlloc failed with 0x%08lx!", __FUNCTION__, error);
        goto Return;
    }

    GameSound->Buffer.Flags = XAUDIO2_END_OF_STREAM;
    GameSound->Buffer.AudioBytes = dataChunkSize;

    if (SetFilePointer(fileHandle, dataChunkOffset + 8, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
    {
        error = GetLastError();
        LogMessageA(LL_ERROR, "[%s] SetFilePointer failed with 0x%08lx!", __FUNCTION__, error);
        goto Return;
    }

    if (ReadFile(fileHandle, audioData, dataChunkSize, &numberOfBytesRead, NULL) == 0)
    {
        error = GetLastError();
        LogMessageA(LL_ERROR, "[%s] ReadFile failed with 0x%08lx!", __FUNCTION__, error);
        goto Return;
    }

    GameSound->Buffer.pAudioData = audioData;

Return:
    if (error == ERROR_SUCCESS)
    {
        LogMessageA(LL_INFORMATIONAL, "[%s] Successfully loaded %s.", __FUNCTION__, FileName, error);
    }
    else
    {
        LogMessageA(LL_ERROR, "[%s] Failed to load %s! Error: 0x%08lx!", __FUNCTION__, FileName, error);
    }

    if (fileHandle && (fileHandle != INVALID_HANDLE_VALUE))
    {
        CloseHandle(fileHandle);
    }
    return error;
}

void PlayGameSound(_In_ GAMESOUND* GameSound)
{
    gXAudioSFXSourceVoice[gSFXSourceVoiceSelector]->lpVtbl->SubmitSourceBuffer(gXAudioSFXSourceVoice[gSFXSourceVoiceSelector], &GameSound->Buffer, NULL);
    gXAudioSFXSourceVoice[gSFXSourceVoiceSelector]->lpVtbl->Start(gXAudioSFXSourceVoice[gSFXSourceVoiceSelector], 0, XAUDIO2_COMMIT_NOW);
    gSFXSourceVoiceSelector++;
    if (gSFXSourceVoiceSelector > NUMBER_OF_SFX_SOURCE_VOICES - 1)
    {
        gSFXSourceVoiceSelector = 0;
    }
}

// MENU FUNCTIONS

void MenuItem_TitleScreen_Resume(void)
{

}

void MenuItem_TitleScreen_NewGame(void)
{

}

void MenuItem_TitleScreen_Options(void)
{
    gPreviousGameState = gCurrentGameState;
    gCurrentGameState = GS_OPTIONS;
}

void MenuItem_TitleScreen_Quit(void)
{
    gPreviousGameState = gCurrentGameState;
    gCurrentGameState = GS_QUIT;
}

void MenuItem_Quit_No(void)
{
    gCurrentGameState = gPreviousGameState;
    gPreviousGameState = GS_QUIT;
}

void MenuItem_Quit_Yes(void)
{
    SendMessageA(gGameWindow, WM_CLOSE, 0, 0);
}

void MenuItem_Options_SFXVolume(void)
{
    if (gGameInput.LeftKeyIsDown && !gGameInput.LeftKeyWasDown && (uint8_t)(gSFXVolume * 10) > 0.0f)
    {
        gSFXVolume -= 0.1f;
    }
    if (gGameInput.RightKeyIsDown && !gGameInput.RightKeyWasDown && (uint8_t)(gSFXVolume * 10) < 10.0f)
    {
        gSFXVolume += 0.1f;
    }

    for (uint8_t counter = 0; counter < NUMBER_OF_SFX_SOURCE_VOICES; counter++)
    {
        gXAudioSFXSourceVoice[counter]->lpVtbl->SetVolume(gXAudioSFXSourceVoice[counter], gSFXVolume, XAUDIO2_COMMIT_NOW);
    }
}

void MenuItem_Options_MusicVolume(void)
{
    if (gGameInput.LeftKeyIsDown && !gGameInput.LeftKeyWasDown && (uint8_t)(gMusicVolume * 10) > 0.0f)
    {
        gMusicVolume -= 0.1f;
    }
    if (gGameInput.RightKeyIsDown && !gGameInput.RightKeyWasDown && (uint8_t)(gMusicVolume * 10) < 10.0f)
    {
        gMusicVolume += 0.1f;
    }

    gXAudioMusicSourceVoice->lpVtbl->SetVolume(gXAudioMusicSourceVoice, gSFXVolume, XAUDIO2_COMMIT_NOW);
}

void MenuItem_Options_WindowSize(void)
{

}

void MenuItem_Options_Back(void)
{
    gCurrentGameState = gPreviousGameState;
    gPreviousGameState = GS_OPTIONS;
}

void DrawSplashScreen(void)
{

}

void DrawTitleScreen(void)
{
    PIXEL32 white = { 0xFF, 0xFF, 0xFF, 0xFF };
    static uint64_t localFrameCounter;
    static uint64_t lastFrameSeen;

    if (gPerformanceData.TotalFramesRendered > (lastFrameSeen + 1))
    {
        gMenu_TitleScreen.SelectedItem = !gPlayer.Active;
    }

    memset(gBackBuffer.Memory, 0, GAME_DRAWING_AREA_MEMORY_SIZE);
    BlitStringToBuffer(GAME_NAME, &g6x7Font, &white, (GAME_RES_WIDTH / 2) - ((uint16_t)strlen(GAME_NAME) * 6 / 2), 60);

    for (uint8_t menuItem = 0; menuItem < gMenu_TitleScreen.ItemCount; menuItem++)
    {
        if (gMenu_TitleScreen.Items[menuItem]->Enabled == TRUE)
        {
            BlitStringToBuffer(gMenu_TitleScreen.Items[menuItem]->Name, &g6x7Font, &white, gMenu_TitleScreen.Items[menuItem]->x, gMenu_TitleScreen.Items[menuItem]->y);
        }
    }

    BlitStringToBuffer("»", &g6x7Font, &white, gMenu_TitleScreen.Items[gMenu_TitleScreen.SelectedItem]->x - 6, gMenu_TitleScreen.Items[gMenu_TitleScreen.SelectedItem]->y);

    localFrameCounter++;
    lastFrameSeen = gPerformanceData.TotalFramesRendered;
}

void DrawOptionsScreen(void)
{
    PIXEL32 white = { 0xFF, 0xFF, 0xFF, 0xFF };
    PIXEL32 grey  = { 0x6F, 0x6F, 0x6F, 0x6F };
    static uint64_t localFrameCounter;
    static uint64_t lastFrameSeen;
    memset(gBackBuffer.Memory, 0, GAME_DRAWING_AREA_MEMORY_SIZE);

    if (gPerformanceData.TotalFramesRendered > (lastFrameSeen + 1))
    {
        gMenu_OptionsScreen.SelectedItem = 0;
    }

    for (uint8_t menuItem = 0; menuItem < gMenu_OptionsScreen.ItemCount; menuItem++)
    {
        if (gMenu_OptionsScreen.Items[menuItem]->Enabled == TRUE)
        {
            BlitStringToBuffer(gMenu_OptionsScreen.Items[menuItem]->Name, &g6x7Font, &white, gMenu_OptionsScreen.Items[menuItem]->x, gMenu_OptionsScreen.Items[menuItem]->y);
        }
    }

    for (uint8_t volume = 0; volume < 10; volume++)
    {
        if (volume >= (uint8_t)(gSFXVolume * 10))
        {
            BlitStringToBuffer("\xf2", &g6x7Font, &grey, 240 + (volume * 6), 100);
        }
        else
        {
            BlitStringToBuffer("\xf2", &g6x7Font, &white, 240 + (volume * 6), 100);
        }
    }

    for (uint8_t volume = 0; volume < 10; volume++)
    {
        if (volume >= (uint8_t)(gMusicVolume * 10))
        {
            BlitStringToBuffer("\xf2", &g6x7Font, &grey, 240 + (volume * 6), 115);
        }
        else
        {
            BlitStringToBuffer("\xf2", &g6x7Font, &white, 240 + (volume * 6), 115);
        }
    }

    BlitStringToBuffer("»", &g6x7Font, &white, gMenu_OptionsScreen.Items[gMenu_OptionsScreen.SelectedItem]->x - 6, gMenu_OptionsScreen.Items[gMenu_OptionsScreen.SelectedItem]->y);

    localFrameCounter++;
    lastFrameSeen = gPerformanceData.TotalFramesRendered;
}

void DrawQuitScreen(void)
{
    PIXEL32 white = { 0xFF, 0xFF, 0xFF, 0xFF };
    static uint64_t localFrameCounter;
    static uint64_t lastFrameSeen;
    memset(gBackBuffer.Memory, 0, GAME_DRAWING_AREA_MEMORY_SIZE);

    BlitStringToBuffer(gMenu_QuitScreen.Name, &g6x7Font, &white,
        (GAME_RES_WIDTH / 2) - ((uint16_t)strlen(gMenu_QuitScreen.Name) * 6 / 2),
        (GAME_RES_HEIGHT / 2) - 10);
    BlitStringToBuffer(gMenu_QuitScreen.Items[0]->Name, &g6x7Font, &white,
        ((GAME_RES_WIDTH / 2) - ((uint16_t)strlen(gMenu_QuitScreen.Items[0]->Name) * 6 / 2) - 25),
        (GAME_RES_HEIGHT / 2) + 10);
    BlitStringToBuffer(gMenu_QuitScreen.Items[1]->Name, &g6x7Font, &white,
        ((GAME_RES_WIDTH / 2) - ((uint16_t)strlen(gMenu_QuitScreen.Items[1]->Name) * 6 / 2) + 25),
        (GAME_RES_HEIGHT / 2) + 10);

    BlitStringToBuffer("»", &g6x7Font, &white, gMenu_QuitScreen.Items[gMenu_QuitScreen.SelectedItem]->x - 6, gMenu_QuitScreen.Items[gMenu_QuitScreen.SelectedItem]->y);
}

void DrawUnpluggedScreen(void)
{
#define GAMEPADUNPLUGGEDSTRING1 "Gamepad Disconnected"
#define GAMEPADUNPLUGGEDSTRING2 "Reconnect it, or press escape to continue using keyboard"
    PIXEL32 white = { 0xFF, 0xFF, 0xFF, 0xFF };
    memset(gBackBuffer.Memory, 0, GAME_DRAWING_AREA_MEMORY_SIZE);

    BlitStringToBuffer(GAMEPADUNPLUGGEDSTRING1, &g6x7Font, &white, (GAME_RES_WIDTH / 2) - (((uint16_t)strlen(GAMEPADUNPLUGGEDSTRING1) * 6) / 2), 100);
    BlitStringToBuffer(GAMEPADUNPLUGGEDSTRING2, &g6x7Font, &white, (GAME_RES_WIDTH / 2) - (((uint16_t)strlen(GAMEPADUNPLUGGEDSTRING2) * 6) / 2), 115);
}

// INPUT SCREENS

void SplashInput(void)
{

}

void TitleInput(void)
{
    if (gGameInput.DownKeyIsDown && !gGameInput.DownKeyWasDown)
    {
        if (gMenu_TitleScreen.SelectedItem < gMenu_TitleScreen.ItemCount - 1)
        {
            gMenu_TitleScreen.SelectedItem++;
            PlayGameSound(&gMenuNavigate);
        }
    }
    if (gGameInput.UpKeyIsDown && !gGameInput.UpKeyWasDown)
    {
        int8_t startingPos = !gPlayer.Active; // Hide the resume button
        if (gMenu_TitleScreen.SelectedItem > startingPos)
        {
            gMenu_TitleScreen.SelectedItem--;
            PlayGameSound(&gMenuNavigate);
        }
    }
    if (gGameInput.ChooseKeyIsDown && !gGameInput.ChooseKeyWasDown)
    {
        gMenu_TitleScreen.Items[gMenu_TitleScreen.SelectedItem]->Action();
        PlayGameSound(&gMenuChoose);
    };
}

void OverworldInput(void)
{
    if (!gPlayer.MovementRemaining)
    {
        if (gGameInput.DownKeyIsDown)
        {
            if (gPlayer.ScreenPosY < GAME_RES_HEIGHT - 16)
            {
                gPlayer.MovementRemaining = 16;
                gPlayer.Direction = DOWN;
            }
        }
        else if (gGameInput.LeftKeyIsDown)
        {
            if (gPlayer.ScreenPosX > 0)
            {
                gPlayer.MovementRemaining = 16;
                gPlayer.Direction = LEFT;
            }
        }
        else if (gGameInput.RightKeyIsDown)
        {
            if (gPlayer.ScreenPosX < GAME_RES_WIDTH - 16)
            {
                gPlayer.MovementRemaining = 16;
                gPlayer.Direction = RIGHT;
            }
        }
        else if (gGameInput.UpKeyIsDown)
        {
            if (gPlayer.ScreenPosY > 0)
            {
                gPlayer.MovementRemaining = 16;
                gPlayer.Direction = UP;
            }
        }
    }
    else
    {
        gPlayer.MovementRemaining--;
        if (gPlayer.Direction == DOWN)
        {
            gPlayer.ScreenPosY++;
        }
        else if (gPlayer.Direction == LEFT)
        {
            gPlayer.ScreenPosX--;
        }
        else if (gPlayer.Direction == RIGHT)
        {
            gPlayer.ScreenPosX++;
        }
        else if (gPlayer.Direction == UP)
        {
            gPlayer.ScreenPosY--;
        }

        switch (gPlayer.MovementRemaining)
        {
        case 16:
            gPlayer.SpriteIndex = 0;
            break;
        case 12:
            gPlayer.SpriteIndex = 1;
            break;
        case 8:
            gPlayer.SpriteIndex = 0;
            break;
        case 4:
            gPlayer.SpriteIndex = 2;
            break;
        case 0:
            gPlayer.SpriteIndex = 0;
            break;
        }
    }
}

void OptionsInput(void)
{
    // Up Down Control
    if (gGameInput.DownKeyIsDown && !gGameInput.DownKeyWasDown)
    {
        if (gMenu_OptionsScreen.SelectedItem < gMenu_OptionsScreen.ItemCount - 1)
        {
            gMenu_OptionsScreen.SelectedItem++;
            PlayGameSound(&gMenuNavigate);
        }
    }
    if (gGameInput.UpKeyIsDown && !gGameInput.UpKeyWasDown)
    {
        if (gMenu_OptionsScreen.SelectedItem > 0)
        {
            gMenu_OptionsScreen.SelectedItem--;
            PlayGameSound(&gMenuNavigate);
        }
    }
    if (gGameInput.ChooseKeyIsDown && !gGameInput.ChooseKeyWasDown)
    {
        gMenu_OptionsScreen.Items[gMenu_OptionsScreen.SelectedItem]->Action();
        PlayGameSound(&gMenuChoose);
    };

    // Sound control
    if (gGameInput.LeftKeyIsDown && !gGameInput.LeftKeyWasDown || gGameInput.RightKeyIsDown && !gGameInput.RightKeyWasDown &&
        gMenu_OptionsScreen.Items[gMenu_OptionsScreen.SelectedItem]->Name != "Back:") // Other options can be toggled with left/right, not this one though
    {
        gMenu_OptionsScreen.Items[gMenu_OptionsScreen.SelectedItem]->Action();
        PlayGameSound(&gMenuChoose);
    }
}

void QuitInput(void)
{
    if (gGameInput.LeftKeyIsDown && !gGameInput.LeftKeyWasDown)
    {
        if (gMenu_QuitScreen.SelectedItem > 0)
        {
            gMenu_QuitScreen.SelectedItem--;
            PlayGameSound(&gMenuNavigate);
        }
    }
    if (gGameInput.RightKeyIsDown && !gGameInput.RightKeyWasDown)
    {
        if (gMenu_QuitScreen.SelectedItem < gMenu_QuitScreen.ItemCount - 1)
        {
            gMenu_QuitScreen.SelectedItem++;
            PlayGameSound(&gMenuNavigate);
        }
    }
    if (gGameInput.ChooseKeyIsDown && !gGameInput.ChooseKeyWasDown)
    {
        gMenu_QuitScreen.Items[gMenu_QuitScreen.SelectedItem]->Action();
        PlayGameSound(&gMenuChoose);
    };
}

void UnpluggedInput(void)
{
    if (gGamepadID > -1 || (gGameInput.EscapeKeyIsDown && !gGameInput.EscapeKeyWasDown))
    {
        gCurrentGameState = gPreviousGameState;
        gPreviousGameState = GS_UNPLUGGED;
    }
}