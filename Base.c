// Externals at warning level 3
#include <stdio.h>
#include <windows.h>
#include <stdint.h>
#include "Base.h"

HWND gGameWindow;
BOOL gGameIsRunning;
GAMEBITMAP gBackBuffer = { 0 };
GAMEPERFDATA gPerformanceData;

int WinMain(HINSTANCE Inst, HINSTANCE InstPrev, PSTR CmdLine, int CmdShow)
{
    UNREFERENCED_PARAMETER(InstPrev);
    UNREFERENCED_PARAMETER(CmdLine);
    UNREFERENCED_PARAMETER(CmdShow);

    MSG message = { 0 };
    int64_t frameStart;
    int64_t frameEnd;
    int64_t elapsedMsPerFrame;
    int64_t elapsedMsPerFrameAccumulatorRaw = 0;
    int64_t elapsedMsPerFrameAccumulatorCooked = 0;

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

        elapsedMsPerFrame = frameEnd - frameStart;
        elapsedMsPerFrame *= 1000000;
        elapsedMsPerFrame /= gPerformanceData.PerfFrequency;

        gPerformanceData.TotalFramesRendered++;
        elapsedMsPerFrameAccumulatorRaw += elapsedMsPerFrame;

        while (elapsedMsPerFrame <= TARGET_MICROSECONDS_PER_FRAME)
        {
            Sleep(0);

            elapsedMsPerFrame = frameEnd - frameStart;
            elapsedMsPerFrame *= 1000000;
            elapsedMsPerFrame /= gPerformanceData.PerfFrequency;

            QueryPerformanceCounter((LARGE_INTEGER*)&frameEnd);
        }

        elapsedMsPerFrameAccumulatorCooked += elapsedMsPerFrame;

        if (gPerformanceData.TotalFramesRendered % CALCULATE_AVG_FPS_EVERY_X_FRAMES == 0)
        {
            int64_t avgMsPerFrameRaw = elapsedMsPerFrameAccumulatorRaw / CALCULATE_AVG_FPS_EVERY_X_FRAMES;
            int64_t avgMsPerFrameCooked = elapsedMsPerFrameAccumulatorCooked / CALCULATE_AVG_FPS_EVERY_X_FRAMES;

            gPerformanceData.RawFPSAverage = 1.0f / ((elapsedMsPerFrameAccumulatorRaw / 60) * 0.000001f);
            gPerformanceData.CookedFPSAverage = 1.0f / ((elapsedMsPerFrameAccumulatorCooked / 60) * 0.000001f);

            char str[128] = { 0 };
            _snprintf_s(str, _countof(str), _TRUNCATE,
                "Avg milliseconds/frame: %.02f\tAvg FPS Cooked: %.01f\tAvg FPS Raw: %.01f\n",
                avgMsPerFrameRaw,
                gPerformanceData.CookedFPSAverage,
                gPerformanceData.RawFPSAverage);

            OutputDebugStringA(str);
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

    gGameWindow = CreateWindowExA(0, windowClass.lpszClassName, "Window Title", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, Inst, NULL);
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

    if (SetWindowLongPtrA(gGameWindow, GWL_STYLE, (WS_OVERLAPPEDWINDOW | WS_VISIBLE) & ~WS_OVERLAPPEDWINDOW) == 0)
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

    if (EscapeKeyIsDown)
    {
        SendMessageA(gGameWindow, WM_CLOSE, 0, 0);
    }
}

void RenderFrameGraphics(void)
{
    //memset(gBackBuffer.Memory, 0xFF, (GAME_RES_WIDTH * 4) * 3);

    PIXEL32 Pixel = { 0 };
    Pixel.Blue = 0xff;
    Pixel.Green = 0;
    Pixel.Red = 0;
    Pixel.Alpha = 0xff;

    memcpy_s(gBackBuffer.Memory, sizeof(PIXEL32), &Pixel, sizeof(PIXEL32));

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