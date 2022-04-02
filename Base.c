// Externals at warning level 3
#include <stdio.h>
#include <windows.h>
#include "Base.h"

HWND gGameWindow;
BOOL gGameIsRunning;
GAMEBITMAP gDrawingSurface = { 0 };

int WinMain(HINSTANCE Inst, HINSTANCE InstPrev, PSTR CmdLine, int CmdShow)
{
    UNREFERENCED_PARAMETER(InstPrev);
    UNREFERENCED_PARAMETER(CmdLine);
    UNREFERENCED_PARAMETER(CmdShow);

    if (GameIsAlreadyRunning())
    {
        MessageBox(NULL, "Another instance is running", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    if (CreateMainGameWindow(Inst) != ERROR_SUCCESS)
    {
        goto Return;
    }

    gDrawingSurface.BitmapInfo.bmiHeader.biSize = sizeof(gDrawingSurface.BitmapInfo.bmiHeader);
    gDrawingSurface.BitmapInfo.bmiHeader.biWidth = GAME_RES_WIDTH;
    gDrawingSurface.BitmapInfo.bmiHeader.biHeight = GAME_RES_HEIGHT;
    gDrawingSurface.BitmapInfo.bmiHeader.biBitCount = GAME_BPP;
    gDrawingSurface.BitmapInfo.bmiHeader.biCompression = BI_RGB;
    gDrawingSurface.BitmapInfo.bmiHeader.biPlanes = 1;

    gDrawingSurface.Memory = VirtualAlloc(NULL, GAME_DRAWING_AREA_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (gDrawingSurface.Memory == NULL)
    {
        MessageBoxA(NULL, "Failed to allocate memory for drawing surface!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    gGameIsRunning = TRUE;
    MSG message = { 0 };

    while (gGameIsRunning)
    {
        while (PeekMessageA(&message, gGameWindow, 0, 0, PM_REMOVE))
        {
            DispatchMessageA(&message);
        }

        ProcessPlayerInput();
        RenderFrameGraphics();

        Sleep(1);
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
    windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    windowClass.lpszMenuName  = NULL;
    windowClass.lpszClassName = GAME_NAME "_WINDOWCLASS";
    windowClass.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassExA(&windowClass))
    {
        result = GetLastError();
        MessageBoxA(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Return;
    }

    gGameWindow = CreateWindowExA(WS_EX_CLIENTEDGE, windowClass.lpszClassName, "Window Title", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, Inst, NULL);
    if (gGameWindow == NULL)
    {
        result = GetLastError();
        MessageBox(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
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
    short EscapeKeyIsDown = GetAsyncKeyState(VK_ESCAPE);

    if (EscapeKeyIsDown)
    {
        SendMessageA(gGameWindow, WM_CLOSE, 0, 0);
    }
}

void RenderFrameGraphics(void)
{

}