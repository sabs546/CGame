// Externals at warning level 3
#include <stdio.h>
#include <windows.h>
#include <psapi.h>
#include <emmintrin.h>
#include <stdint.h>
#include "Base.h"

#pragma comment(lib, "Winmm.lib")

HWND gGameWindow;
BOOL gGameIsRunning;
GAMEBITMAP gBackBuffer;
GAMEBITMAP g6x7Font;
GAMEPERFDATA gPerformanceData;
PLAYER gPlayer;
BOOL gWindowHasFocus;
REGISTRYPARAMS gRegistryParams;

int WinMain(HINSTANCE Inst, HINSTANCE InstPrev, PSTR CmdLine, int CmdShow)
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

    LogMessageA(Informational, "[%s] Informational", __FUNCTION__);
    LogMessageA(Warning, "[%s] Warning", __FUNCTION__);
    LogMessageA(Error, "[%s] Error", __FUNCTION__);
    LogMessageA(Debug, "[%s] Debug", __FUNCTION__);

    HMODULE ntDllModuleHandle;

    if ((ntDllModuleHandle = GetModuleHandleA("ntdll.dll")) == NULL)
    {
        LogMessageA(Error, "[%s] Couldn't load ntdll.dll!, Error 0x%08lx!", __FUNCTION__, GetLastError());
        MessageBoxA(NULL, "Couldn't load ntdll.dll!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((ntQueryTimerResolution = (_NtQueryTimerResolution)GetProcAddress(ntDllModuleHandle, "NtQueryTimerResolution")) == NULL)
    {
        LogMessageA(Error, "[%s] Couldn't find the NtQueryTimerResolution function in ntdll.dll!, GetProcAddress failed! Error 0x%08lx!", __FUNCTION__, GetLastError());
        MessageBoxA(NULL, "Couldn't find the NtQueryTimerResolution function in ntdll.dll!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    ntQueryTimerResolution(&gPerformanceData.MinimumTimerResolution, &gPerformanceData.MaximumTimerResolution, &gPerformanceData.CurrentTimerResolution);
    GetSystemInfo(&gPerformanceData.SystemInfo);
    GetSystemTimeAsFileTime((FILETIME*)&gPerformanceData.CurrentSystemTime);

    if (GameIsAlreadyRunning())
    {
        LogMessageA(Warning, "[%s] Another instance is running, GetProcAddress failed!", __FUNCTION__);
        MessageBoxA(NULL, "Another instance is running", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if (timeBeginPeriod(1) == TIMERR_NOCANDO)
    {
        LogMessageA(Error, "[%s] Failed to set global timer resolution", __FUNCTION__);
        MessageBoxA(NULL, "Failed to set global timer resolution", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if (SetPriorityClass(processHandle, HIGH_PRIORITY_CLASS) == 0)
    {
        LogMessageA(Error, "[%s] Failed to set process priority", __FUNCTION__, GetLastError());
        MessageBoxA(NULL, "Failed to set process priority", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) == 0)
    {
        LogMessageA(Error, "[%s] Failed to set thread priority, Error 0x%08lx!", __FUNCTION__, GetLastError());
        MessageBoxA(NULL, "Failed to set thread priority", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if (CreateMainGameWindow() != ERROR_SUCCESS)
    {
        LogMessageA(Error, "[%s] CreateMainGameWindow failed!", __FUNCTION__);
        goto Return;
    }

    if ((Load32BppBitmapFromFile(".\\Assets\\6x7Font.bmpx", &g6x7Font)) != ERROR_SUCCESS)
    {
        LogMessageA(Error, "[%s] Loading 6x7font.bmpx failed!", __FUNCTION__);
        MessageBox(NULL, "Failed to load font!", "Error!", MB_ICONERROR | MB_OK);
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
        LogMessageA(Error, "[%s] Failed to allocate memory for drawing surface, Error 0x%08lx!", __FUNCTION__, GetLastError());
        MessageBoxA(NULL, "Failed to allocate memory for drawing surface!", "Error!", MB_ICONERROR | MB_OK);
        goto Return;
    }

    if (InitializeHero() != ERROR_SUCCESS)
    {
        LogMessageA(Error, "[%s] Failed to initialize hero!, Error 0x%08lx!", __FUNCTION__);
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
        LogMessageA(Error, "[%s] Window Registration failed! Error 0x%08lx!", __FUNCTION__, result);
        MessageBoxA(NULL, "Window Registration failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    gGameWindow = CreateWindowExA(0, windowClass.lpszClassName, "Window Title", WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (gGameWindow == NULL)
    {
        result = GetLastError();
        LogMessageA(Error, "[%s] CreateWindowExA failed! Error 0x%08lx!", __FUNCTION__, result);
        MessageBox(NULL, "Window Creation failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    gPerformanceData.MonitorInfo.cbSize = sizeof(MONITORINFO);

    if (GetMonitorInfoA(MonitorFromWindow(gGameWindow, MONITOR_DEFAULTTOPRIMARY), &gPerformanceData.MonitorInfo) == 0)
    {
        result = ERROR_MONITOR_NO_DESCRIPTOR;
        LogMessageA(Error, "[%s] GetMonitorInfoA(MonitorFromWindow()) failed! Error 0x%08lx!", __FUNCTION__, result);
        goto Return;
    }

    gPerformanceData.MonitorWidth = gPerformanceData.MonitorInfo.rcMonitor.right - gPerformanceData.MonitorInfo.rcMonitor.left;
    gPerformanceData.MonitorHeight = gPerformanceData.MonitorInfo.rcMonitor.bottom - gPerformanceData.MonitorInfo.rcMonitor.top;

    if (SetWindowLongPtrA(gGameWindow, GWL_STYLE, WS_VISIBLE) == 0)
    {
        result = GetLastError();
        LogMessageA(Error, "[%s] SetWindowLongPtrA() failed! Error 0x%08lx!", __FUNCTION__, result);
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
        LogMessageA(Error, "[%s] SetWindowPos() failed! Error 0x%08lx!", __FUNCTION__, result);
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

    int16_t EscapeKeyIsDown = GetAsyncKeyState(VK_ESCAPE);
    int16_t DebugKeyIsDown = GetAsyncKeyState(VK_F1);
    static int16_t DebugKeyWasDown;

    int16_t LeftKeyIsDown = GetAsyncKeyState(VK_LEFT) | GetAsyncKeyState(0x41);
    int16_t RightKeyIsDown = GetAsyncKeyState(VK_RIGHT) | GetAsyncKeyState(0x44);
    int16_t UpKeyIsDown = GetAsyncKeyState(VK_UP) | GetAsyncKeyState(0x57);
    int16_t DownKeyIsDown = GetAsyncKeyState(VK_DOWN) | GetAsyncKeyState(0x53);
    static int16_t LeftKeyWasDown;
    static int16_t RightKeyWasDown;
    static int16_t UpKeyWasDown;
    static int16_t DownKeyWasDown;

    if (EscapeKeyIsDown)
    {
        SendMessageA(gGameWindow, WM_CLOSE, 0, 0);
    }

    if (DebugKeyIsDown && !DebugKeyWasDown)
    {
        gPerformanceData.DisplayDebugInfo = !gPerformanceData.DisplayDebugInfo;
    }

    if (!gPlayer.MovementRemaining)
    {
        if (DownKeyIsDown)
        {
            if (gPlayer.ScreenPosY < GAME_RES_HEIGHT - 16)
            {
                gPlayer.MovementRemaining = 16;
                gPlayer.Direction = Down;
            }
        }
        else if (LeftKeyIsDown)
        {
            if (gPlayer.ScreenPosX > 0)
            {
                gPlayer.MovementRemaining = 16;
                gPlayer.Direction = Left;
            }
        }
        else if (RightKeyIsDown)
        {
            if (gPlayer.ScreenPosX < GAME_RES_WIDTH - 16)
            {
                gPlayer.MovementRemaining = 16;
                gPlayer.Direction = Right;
            }
        }
        else if (UpKeyIsDown)
        {
            if (gPlayer.ScreenPosY > 0)
            {
                gPlayer.MovementRemaining = 16;
                gPlayer.Direction = Up;
            }
        }
    }
    else
    {
        gPlayer.MovementRemaining--;
        if (gPlayer.Direction == Down)
        {
            gPlayer.ScreenPosY++;
        }
        else if (gPlayer.Direction == Left)
        {
            gPlayer.ScreenPosX--;
        }
        else if (gPlayer.Direction == Right)
        {
            gPlayer.ScreenPosX++;
        }
        else if (gPlayer.Direction == Up)
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
    DebugKeyWasDown = DebugKeyIsDown;
    //LeftKeyWasDown = LeftKeyIsDown;
    //RightKeyWasDown = RightKeyIsDown;
    //UpKeyWasDown = UpKeyIsDown;
    //DownKeyWasDown = DownKeyIsDown;
}

void RenderFrameGraphics(void)
{
#ifdef SIMD
    __m128i QuadPixel = { 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff };
    ClearScreen(&QuadPixel);
#else
    PIXEL32 Pixel = { 0x7f, 0x00, 0x00, 0xff };
    ClearScreen(&Pixel);
#endif // SIMD

    PIXEL32 green = { 0x00, 0xff, 0x00, 0xff };
    BlitStringToBuffer("GAME OVER", &g6x7Font, &green, 60, 60);

    Blit32BppBitmapToBuffer(&gPlayer.Sprite[gPlayer.CurrentArmour][gPlayer.Direction + gPlayer.SpriteIndex], gPlayer.ScreenPosX, gPlayer.ScreenPosY);

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

#ifdef SIMD
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
#endif // SIMD

DWORD InitializeHero(void)
{
    DWORD error = ERROR_SUCCESS;

    gPlayer.ScreenPosX = 32;
    gPlayer.ScreenPosY = 32;
    gPlayer.CurrentArmour = SUIT_0;
    gPlayer.Direction = Down;
    gPlayer.MovementRemaining = 0;
    
    if ((error = Load32BppBitmapFromFile(".\\Assets\\Standing_Down.bmpx", &gPlayer.Sprite[SUIT_0][FACING_DOWN_0])) != ERROR_SUCCESS)
    {
        LogMessageA(Error, "[%s] Load32BppBitmapFromFile \"Standing_Down.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Down_1.bmpx", &gPlayer.Sprite[SUIT_0][FACING_DOWN_1])) != ERROR_SUCCESS)
    {
        LogMessageA(Error, "[%s] Load32BppBitmapFromFile \"Walk_Down_1.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Down_2.bmpx", &gPlayer.Sprite[SUIT_0][FACING_DOWN_2])) != ERROR_SUCCESS)
    {
        LogMessageA(Error, "[%s] Load32BppBitmapFromFile \"Walk_Down_2.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Standing_Left.bmpx", &gPlayer.Sprite[SUIT_0][FACING_LEFT_0])) != ERROR_SUCCESS)
    {
        LogMessageA(Error, "[%s] Load32BppBitmapFromFile \"Standing_Left.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Left_1.bmpx", &gPlayer.Sprite[SUIT_0][FACING_LEFT_1])) != ERROR_SUCCESS)
    {
        LogMessageA(Error, "[%s] Load32BppBitmapFromFile \"Walk_Left_1.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Left_2.bmpx", &gPlayer.Sprite[SUIT_0][FACING_LEFT_2])) != ERROR_SUCCESS)
    {
        LogMessageA(Error, "[%s] Load32BppBitmapFromFile \"Walk_Left_2.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Standing_Right.bmpx", &gPlayer.Sprite[SUIT_0][FACING_RIGHT_0])) != ERROR_SUCCESS)
    {
        LogMessageA(Error, "[%s] Load32BppBitmapFromFile \"Standing_Right.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Right_1.bmpx", &gPlayer.Sprite[SUIT_0][FACING_RIGHT_1])) != ERROR_SUCCESS)
    {
        LogMessageA(Error, "[%s] Load32BppBitmapFromFile \"Walk_Right_1.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Right_2.bmpx", &gPlayer.Sprite[SUIT_0][FACING_RIGHT_2])) != ERROR_SUCCESS)
    {
        LogMessageA(Error, "[%s] Load32BppBitmapFromFile \"Walk_Right_2.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Standing_Up.bmpx", &gPlayer.Sprite[SUIT_0][FACING_UP_0])) != ERROR_SUCCESS)
    {
        LogMessageA(Error, "[%s] Load32BppBitmapFromFile \"Standing_Up.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Up_1.bmpx", &gPlayer.Sprite[SUIT_0][FACING_UP_1])) != ERROR_SUCCESS)
    {
        LogMessageA(Error, "[%s] Load32BppBitmapFromFile \"Walk_Up_1.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
        MessageBoxA(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((error = Load32BppBitmapFromFile(".\\Assets\\Walk_Up_2.bmpx", &gPlayer.Sprite[SUIT_0][FACING_UP_2])) != ERROR_SUCCESS)
    {
        LogMessageA(Error, "[%s] Load32BppBitmapFromFile \"Walk_Up_2.bmpx\" failed! Error 0x%08lx!", __FUNCTION__);
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
        case '0xBB':
            widthOffset = charWidth * 95;
            break;
        case '0xAB':
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
        LogMessageA(Error, "[%s] RegCreateKey faield with error code 0x%08lx!", __FUNCTION__, result);
        goto Return;
    }

    if (regDisposition == REG_CREATED_NEW_KEY)
    {
        LogMessageA(Informational, "[%s] Registry key did not exist; created new key HKCU\\SOFTWARE\\%s" __FUNCTION__, GAME_NAME);
    }
    else
    {
        LogMessageA(Informational, "[%s] Opened existing registry key HCKU\\SOFTWARE\\%s", __FUNCTION__, GAME_NAME);
    }
    result = RegGetValueA(regKey, NULL, "LogLevel", RRF_RT_DWORD, NULL, (BYTE*)&gRegistryParams.LogLevel, &regBytesRead);

    if (result != ERROR_SUCCESS)
    {
        if (result == ERROR_FILE_NOT_FOUND)
        {
            result = Informational;
            LogMessageA(Informational, "[%s] Registry 'LogLevel' not found; using default of 0. (LOG_LEVEL_NONE)" __FUNCTION__);
            gRegistryParams.LogLevel = None;
        }
        else
        {
            LogMessageA(Error, "[%s] Failed to read the 'LogLevel' registry value! Error 0x%08lx!" __FUNCTION__, result);
            goto Return;
        }
    }

    LogMessageA(Informational, "[%s] LogLevel is %d.", __FUNCTION__, gRegistryParams.LogLevel);

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
        return;
    }

    switch (LogLevel)
    {
    case None:
        return;
    case Informational:
        strcpy_s(severityString, sizeof(severityString), "[INFO]");
        break;
    case Warning:
        strcpy_s(severityString, sizeof(severityString), "[WARN]");
        break;
    case Error:
        strcpy_s(severityString, sizeof(severityString), "[ERROR]");
        break;
    case Debug:
        strcpy_s(severityString, sizeof(severityString), "[DEBUG]");
        break;
    default:
        ASSERT(FALSE);
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
        return;
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