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
GAMEBITMAP gBackBuffer = { 0 };
GAMEPERFDATA gPerformanceData;
PLAYER gPlayer;
BOOL gWindowHasFocus;

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

    HMODULE ntDllModuleHandle;

    if ((ntDllModuleHandle = GetModuleHandleA("ntdll.dll")) == NULL)
    {
        MessageBoxA(NULL, "Couldn't load ntdll.dll!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if ((ntQueryTimerResolution = (_NtQueryTimerResolution)GetProcAddress(ntDllModuleHandle, "NtQueryTimerResolution")) == NULL)
    {
        MessageBoxA(NULL, "Couldn't find the NtQueryTimerResolution function in ntdll.dll!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    ntQueryTimerResolution(&gPerformanceData.MinimumTimerResolution, &gPerformanceData.MaximumTimerResolution, &gPerformanceData.CurrentTimerResolution);
    GetSystemInfo(&gPerformanceData.SystemInfo);
    GetSystemTimeAsFileTime((FILETIME*)&gPerformanceData.CurrentSystemTime);

    if (GameIsAlreadyRunning())
    {
        MessageBoxA(NULL, "Another instance is running", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if (timeBeginPeriod(1) == TIMERR_NOCANDO)
    {
        MessageBoxA(NULL, "Failed to set global timer resolution", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if (SetPriorityClass(processHandle, HIGH_PRIORITY_CLASS) == 0)
    {
        MessageBoxA(NULL, "Failed to set process priority", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) == 0)
    {
        MessageBoxA(NULL, "Failed to set thread priority", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if (CreateMainGameWindow() != ERROR_SUCCESS)
    {
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
        MessageBoxA(NULL, "Failed to allocate memory for drawing surface!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if (InitializeHero() != ERROR_SUCCESS)
    {

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

            gPerformanceData.CPUPercent  = (currentKernelCPUTime - previousKernelCPUTime) + (currentUserCPUTime - previousUserCPUTime);
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
        MessageBoxA(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    gGameWindow = CreateWindowExA(0, windowClass.lpszClassName, "Window Title", WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (gGameWindow == NULL)
    {
        result = GetLastError();
        MessageBox(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    gPerformanceData.MonitorInfo.cbSize = sizeof(MONITORINFO);

    if (GetMonitorInfoA(MonitorFromWindow(gGameWindow, MONITOR_DEFAULTTOPRIMARY), &gPerformanceData.MonitorInfo) == 0)
    {
        result = ERROR_MONITOR_NO_DESCRIPTOR;
        goto Return;
    }

    gPerformanceData.MonitorWidth = gPerformanceData.MonitorInfo.rcMonitor.right - gPerformanceData.MonitorInfo.rcMonitor.left;
    gPerformanceData.MonitorHeight = gPerformanceData.MonitorInfo.rcMonitor.bottom - gPerformanceData.MonitorInfo.rcMonitor.top;

    if (SetWindowLongPtrA(gGameWindow, GWL_STYLE, WS_VISIBLE) == 0)
    {
        result = GetLastError();
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

    if (DebugKeyIsDown)
    {
        gPerformanceData.DisplayDebugInfo = !gPerformanceData.DisplayDebugInfo;
    }
    DebugKeyWasDown = DebugKeyIsDown;

    if (LeftKeyWasDown)
    {
        if (gPlayer.ScreenPosX > 0)
        {
            gPlayer.ScreenPosX--;
        }
    }
    LeftKeyWasDown = LeftKeyIsDown;

    if (RightKeyWasDown)
    {
        if (gPlayer.ScreenPosX < GAME_RES_WIDTH - 16)
        {
            gPlayer.ScreenPosX++;
        }
    }
    RightKeyWasDown = RightKeyIsDown;

    if (UpKeyWasDown)
    {
        if (gPlayer.ScreenPosY > 0)
        {
            gPlayer.ScreenPosY--;
        }
    }
    UpKeyWasDown = UpKeyIsDown;

    if (DownKeyWasDown)
    {
        if (gPlayer.ScreenPosY < GAME_RES_HEIGHT - 16)
        {
            gPlayer.ScreenPosY++;
        }
    }
    DownKeyWasDown = DownKeyIsDown;
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

    int32_t screenX = gPlayer.ScreenPosX;
    int32_t screenY = gPlayer.ScreenPosY;

    int32_t startingScreenPixel = ((GAME_RES_WIDTH * GAME_RES_HEIGHT) - GAME_RES_WIDTH) - (GAME_RES_WIDTH * screenY) + screenX;

    for (int32_t y = 0; y < 16; y++)
    {
        for (int32_t x = 0; x < 16; x++)
        {
            memset((PIXEL32*)gBackBuffer.Memory + (uintptr_t)startingScreenPixel + x - ((uintptr_t)GAME_RES_WIDTH * y), 0xFF, sizeof(PIXEL32));
        }
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

    if (gPerformanceData.DisplayDebugInfo)
    {
        SelectObject(DeviceContext, (HFONT)GetStockObject(ANSI_FIXED_FONT));

        char DebugTextBuffer[64] = { 0 };

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "FPS Raw:       %.01f", gPerformanceData.RawFPSAverage);
        TextOutA(DeviceContext, 0, 0, DebugTextBuffer, (int)strlen(DebugTextBuffer));

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "FPS Cooked:    %.01f", gPerformanceData.CookedFPSAverage);
        TextOutA(DeviceContext, 0, 13, DebugTextBuffer, (int)strlen(DebugTextBuffer));

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "Min Timer Res: %.02f", gPerformanceData.MinimumTimerResolution / 10000.0f);
        TextOutA(DeviceContext, 0, 26, DebugTextBuffer, (int)strlen(DebugTextBuffer));

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "Max Timer Res: %.02f", gPerformanceData.MaximumTimerResolution / 10000.0f);
        TextOutA(DeviceContext, 0, 39, DebugTextBuffer, (int)strlen(DebugTextBuffer));

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "Cur Timer Res: %.02f", gPerformanceData.CurrentTimerResolution / 10000.0f);
        TextOutA(DeviceContext, 0, 52, DebugTextBuffer, (int)strlen(DebugTextBuffer));

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "Handles: %lu", gPerformanceData.HandleCount);
        TextOutA(DeviceContext, 0, 65, DebugTextBuffer, (int)strlen(DebugTextBuffer));

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "Memory:  %llu KB", gPerformanceData.MemInfo.PrivateUsage / 1024);
        TextOutA(DeviceContext, 0, 78, DebugTextBuffer, (int)strlen(DebugTextBuffer));

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "CPU:     %.02f%%", gPerformanceData.CPUPercent);
        TextOutA(DeviceContext, 0, 91, DebugTextBuffer, (int)strlen(DebugTextBuffer));
    }

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

    gPlayer.ScreenPosX = 25;
    gPlayer.ScreenPosY = 25;
    
    if ((error = Load32BppBitmapFromFile("F:\\GitHub\\CGame\\Assets\\Hero_Suit0_Down_Standing.bmpx", &gPlayer.Sprite[SUIT_0][FACING_DOWN_0])) != ERROR_SUCCESS)
    {
        MessageBox(NULL, "Load32BppBitmapFromFile Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
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

    if (SetFilePointer(fileHandle, pixelDataOffset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
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