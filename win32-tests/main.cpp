#include <windows.h>

static const char WND_CLASS_NAME[] = "win32-tests";

struct WindowContext
{
    ATOM classAtom;
    HINSTANCE instance;
    HWND window;
};

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CLOSE:
        {
            // Translate this to WM_QUIT so that we can handle all cases in the same place
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam );
}

static ATOM registerWindowClass(HINSTANCE instance)
{
    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));

    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = (WNDPROC) wndProc;
    wc.hInstance     = instance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(NULL, IDI_WINLOGO);
    wc.lpszClassName = WND_CLASS_NAME;

    return RegisterClass( &wc );
}

static HWND openWindow(const WindowContext& wctx, int x, int y, int w, int h)
{
    DWORD wndStyle = WS_CLIPSIBLINGS | WS_CLIPCHILDREN 
        | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU 
        | WS_MINIMIZEBOX |  WS_MAXIMIZEBOX | WS_SIZEBOX;
    DWORD wndExStyle = WS_EX_APPWINDOW;

    int fullW = w;
    int fullH = h;

    HWND window = CreateWindowEx(
        wndExStyle, WND_CLASS_NAME, "My window", wndStyle,
        x, y, fullW, fullH,
        NULL, NULL, wctx.instance, NULL
    );

    ShowWindow(window, SW_SHOWNORMAL);
    SetFocus(window);

    return window;
}

static void closeWindow(WindowContext& wctx)
{
    DestroyWindow(wctx.window);
    wctx.window = NULL;

    UnregisterClass(WND_CLASS_NAME, wctx.instance);
    wctx.classAtom = 0;
}

static void pollEvents(WindowContext& wctx)
{
    bool doClose = false;
    MSG msg;

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        switch (msg.message)
        {
            case WM_QUIT:
                doClose = true;
                break;

            default:
                DispatchMessage(&msg);
                break;
        }
    }

    if (doClose)
    {
        closeWindow(wctx);
    }
}


int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    WindowContext wctx;

    wctx.instance = GetModuleHandle(NULL);
    wctx.classAtom = registerWindowClass(wctx.instance);
    wctx.window = openWindow(wctx, 100, 100, 640, 480);

    while (wctx.window) 
    {
        Sleep(15);
        pollEvents(wctx);
    }

    return 0;
}
