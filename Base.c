// Externals at warning level 3
#include <stdio.h>
#include <windows.h>
#include <emmintrin.h>
#include <stdint.h>
#include "Base.h"

HWND gGameWindow;
BOOL gGameIsRunning;
GAMEBITMAP gBackBuffer = { 0 };
GAMEPERFDATA gPerformanceData;
PLAYER gPlayer;

int WinMain(HINSTANCE Inst, HINSTANCE InstPrev, PSTR CmdLine, int CmdShow)
{
    UNREFERENCED_PARAMETER(InstPrev);
    UNREFERENCED_PARAMETER(CmdLine);
    UNREFERENCED_PARAMETER(CmdShow);

    MSG message = { 0 };
    int64_t frameStart;
    int64_t frameEnd;
    int64_t elapsedMs;
    int64_t elapsedMsPerFrameAccumulatorRaw = 0;
    int64_t elapsedMsPerFrameAccumulatorCooked = 0;

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

    if (GameIsAlreadyRunning())
    {
        MessageBox(NULL, "Another instance is running", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if (CreateMainGameWindow(Inst) != ERROR_SUCCESS)
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

    gPlayer.WorldPosX = 25;
    gPlayer.WorldPosY = 25;

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
        elapsedMsPerFrameAccumulatorRaw += elapsedMs;

        while (elapsedMs <= TARGET_MICROSECONDS_PER_FRAME)
        {
            elapsedMs = frameEnd - frameStart;
            elapsedMs *= 1000000;
            elapsedMs /= gPerformanceData.PerfFrequency;

            QueryPerformanceCounter((LARGE_INTEGER*)&frameEnd);

            if (elapsedMs < ((int64_t)TARGET_MICROSECONDS_PER_FRAME - (gPerformanceData.CurrentTimerResolution * 0.1f)))
            {
                Sleep(1);
            }
        }

        elapsedMsPerFrameAccumulatorCooked += elapsedMs;

        if (gPerformanceData.TotalFramesRendered % CALCULATE_AVG_FPS_EVERY_X_FRAMES == 0)
        {
            gPerformanceData.RawFPSAverage = 1.0f / ((elapsedMsPerFrameAccumulatorRaw / CALCULATE_AVG_FPS_EVERY_X_FRAMES) * 0.000001f);
            gPerformanceData.CookedFPSAverage = 1.0f / ((elapsedMsPerFrameAccumulatorCooked / CALCULATE_AVG_FPS_EVERY_X_FRAMES) * 0.000001f);

            elapsedMsPerFrameAccumulatorRaw = 0;
            elapsedMsPerFrameAccumulatorCooked = 0;
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

    default:
        result = DefWindowProcA(WindowsHandle, Message, WParam, LParam);
    }
    return result;
}

DWORD CreateMainGameWindow(_In_ HANDLE Inst)
{
    DWORD result = ERROR_SUCCESS;

    WNDCLASSEXA windowClass = { 0 };

    windowClass.cbSize        = sizeof(WNDCLASSEX);
    windowClass.style         = 0;
    windowClass.lpfnWndProc   = MainWindowProc;
    windowClass.cbClsExtra    = 0;
    windowClass.cbWndExtra    = 0;
    windowClass.hInstance     = Inst;
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

    gGameWindow = CreateWindowExA(0, windowClass.lpszClassName, "Window Title", WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, Inst, NULL);
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
        if (gPlayer.WorldPosX > 0)
        {
            gPlayer.WorldPosX--;
        }
    }
    LeftKeyWasDown = LeftKeyIsDown;

    if (RightKeyWasDown)
    {
        if (gPlayer.WorldPosX < GAME_RES_WIDTH - 16)
        {
            gPlayer.WorldPosX++;
        }
    }
    RightKeyWasDown = RightKeyIsDown;

    if (UpKeyWasDown)
    {
        if (gPlayer.WorldPosY > 0)
        {
            gPlayer.WorldPosY--;
        }
    }
    UpKeyWasDown = UpKeyIsDown;

    if (DownKeyWasDown)
    {
        if (gPlayer.WorldPosY < GAME_RES_HEIGHT - 16)
        {
            gPlayer.WorldPosY++;
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

    int32_t screenX = gPlayer.WorldPosX;
    int32_t screenY = gPlayer.WorldPosY;

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