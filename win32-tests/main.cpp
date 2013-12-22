#include <windows.h>
#include <gl/GL.h>

static const char WND_CLASS_NAME[] = "win32-tests";

struct WindowContext
{
    ATOM classAtom;
    HINSTANCE instance;
    HWND window;
    HDC dc;
    HGLRC context;
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

static int getPixelFormatId(const WindowContext& wctx)
{
    int count = DescribePixelFormat(wctx.dc, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL);

    for (int i=1; i<=count; i++)
    {
        // TODO: choose the closest from all if perfect match is not found
        PIXELFORMATDESCRIPTOR pfd;
        DescribePixelFormat(wctx.dc, i, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

        static const DWORD REQUIRED_FLAGS = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        if ((pfd.dwFlags & REQUIRED_FLAGS) != REQUIRED_FLAGS)
        {
            continue;
        }

        if (!(pfd.dwFlags & PFD_GENERIC_ACCELERATED) && (pfd.dwFlags & PFD_GENERIC_FORMAT))
        {
            continue;
        }

        if (pfd.iPixelType != PFD_TYPE_RGBA)
        {
            continue;
        }

        if (pfd.cRedBits == 8 && pfd.cGreenBits == 8 && pfd.cBlueBits == 8 && pfd.cAlphaBits == 8
            && pfd.cDepthBits == 24 && pfd.cStencilBits == 0)
        {
            return i;
        }
    }

    return 0;
}

static void createContext(WindowContext& wctx, int pixelFormatId)
{
    PIXELFORMATDESCRIPTOR pfd;
    SetPixelFormat(wctx.dc, pixelFormatId, &pfd);
    wctx.context = wglCreateContext(wctx.dc);
}

static bool openWindow(WindowContext& wctx, int x, int y, int w, int h)
{
    DWORD wndStyle = WS_CLIPSIBLINGS | WS_CLIPCHILDREN 
        | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU 
        | WS_MINIMIZEBOX |  WS_MAXIMIZEBOX | WS_SIZEBOX;
    DWORD wndExStyle = WS_EX_APPWINDOW;

    int fullW = w;
    int fullH = h;

    wctx.window = CreateWindowEx(
        wndExStyle, WND_CLASS_NAME, "My window", wndStyle,
        x, y, fullW, fullH,
        NULL, NULL, wctx.instance, NULL
    );

    wctx.dc = GetDC(wctx.window);
    createContext(wctx, getPixelFormatId(wctx));
    wglMakeCurrent(wctx.dc, wctx.context);

    ShowWindow(wctx.window, SW_SHOWNORMAL);
    SetFocus(wctx.window);

    return true;
}

static void closeWindow(WindowContext& wctx)
{
    wglMakeCurrent( NULL, NULL );
    wglDeleteContext(wctx.context);
    wctx.context = NULL;

    ReleaseDC(wctx.window, wctx.dc );
    wctx.dc = NULL;

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

static void swapBuffers(WindowContext& wctx)
{
    SwapBuffers(wctx.dc);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    WindowContext wctx;

    wctx.instance = GetModuleHandle(NULL);
    wctx.classAtom = registerWindowClass(wctx.instance);
    openWindow(wctx, 100, 100, 640, 480);

    float rd = 0.001f;
    float gd = 0.005f;
    float bd = 0.0025f;

    float r = 0;
    float g = 0;
    float b = 0;

    while (wctx.window) 
    {
        if (r > 1.0f || r < 0.f) rd = -rd;
        if (g > 1.0f || g < 0.f) gd = -gd;
        if (b > 1.0f || b < 0.f) bd = -bd;
        r += rd;
        g += gd;
        b += bd;

        Sleep(15);
        swapBuffers(wctx);
        pollEvents(wctx);

        glClearColor(r, g, b, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    return 0;
}
