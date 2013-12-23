#include <windows.h>
#include <gl/GL.h>

static const char WND_CLASS_NAME[] = "win32-tests";

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static int getPixelFormatId(const HDC& dc)
{
    int count = DescribePixelFormat(dc, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL);

    for (int i=1; i<=count; i++)
    {
        // TODO: choose the closest from all if perfect match is not found
        PIXELFORMATDESCRIPTOR pfd;
        DescribePixelFormat(dc, i, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

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
            && pfd.cDepthBits == 24 && pfd.cStencilBits == 8)
        {
            return i;
        }
    }

    return 0;
}

class WindowContext
{
public:
    typedef void (*UserCallback)(void* data);

    static WindowContext* open(int x, int y, int w, int h)
    {
        WindowContext* ctx = new WindowContext();
        ctx->instance = GetModuleHandle(NULL);

        // Register a class first
        {
            WNDCLASS wc;
            ZeroMemory(&wc, sizeof(wc));

            wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
            wc.lpfnWndProc   = (WNDPROC) wndProc;
            wc.hInstance     = ctx->instance;
            wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
            wc.hIcon         = LoadIcon(NULL, IDI_WINLOGO);
            wc.lpszClassName = WND_CLASS_NAME;

            ctx->classAtom = RegisterClass(&wc);
        }

        // Now we're ready to open the window
        {
            ctx->wndStyle = WS_CLIPSIBLINGS | WS_CLIPCHILDREN 
                | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU 
                | WS_MINIMIZEBOX |  WS_MAXIMIZEBOX | WS_SIZEBOX;
            ctx->wndExStyle = WS_EX_APPWINDOW;

            int fullW = -1;
            int fullH = -1;
            ctx->getFullWindowSize(w, h, fullW, fullH);

            ctx->window = CreateWindowEx(
               ctx->wndExStyle, WND_CLASS_NAME, "My window", ctx->wndStyle,
               x, y, fullW, fullH,
               NULL, NULL, ctx->instance, ctx
            );
        }

        // Create OpenGL context
        {
            ctx->dc = GetDC(ctx->window);
            PIXELFORMATDESCRIPTOR pfd;
            int bestPixelFormatId = getPixelFormatId(ctx->dc);
            SetPixelFormat(ctx->dc, bestPixelFormatId, &pfd);
            ctx->context = wglCreateContext(ctx->dc);
            wglMakeCurrent(ctx->dc, ctx->context);
        }

        // And finally, make it visible
        ShowWindow(ctx->window, SW_SHOWNORMAL);
        BringWindowToTop(ctx->window);
        SetForegroundWindow(ctx->window);
        SetFocus(ctx->window);

        return ctx;
    }

    void setUpdateCallback(UserCallback callback, void* arg)
    {
        updateFun = callback;
        updateArg = arg;
    }

    void setRenderCallback(UserCallback callback, void* arg)
    {
        renderFun = callback;
        renderArg = arg;
    }

    void getClientSize(int& w, int& h)
    {
        RECT area;
        GetClientRect(window, &area);
        w = area.right;
        h = area.bottom;
    }

    void setClientSize(int w, int h)
    {
        int fullW = -1;
        int fullH = -1;
        getFullWindowSize(w, h, fullW, fullH);
        SetWindowPos(window, HWND_TOP, 0, 0, fullW, fullH,
                     SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER);
    }

    void run()
    {
        while (doFinish == false) 
        {
            poll();
            if (updateFun) { updateFun(updateArg); }
            if (renderFun) { renderFun(renderArg); }
            Sleep(15);
            swapBuffers();
        }
    }

    ~WindowContext()
    {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(context);
        context = NULL;

        ReleaseDC(window, dc);
        dc = NULL;

        DestroyWindow(window);
        window = NULL;

        UnregisterClass(WND_CLASS_NAME, instance);
        classAtom = 0;
    }

private:
    WindowContext(const WindowContext& ctx);
    WindowContext& operator = (const WindowContext& other);

    WindowContext()
        : doFinish(false)
        , updateFun(NULL)
        , updateArg(NULL)
        , renderFun(NULL)
        , renderArg(NULL)
    {
    }

    void getFullWindowSize(int clientW, int clientH, int& fullW, int& fullH)
    {
        RECT rect = { 0, 0, clientW, clientH };
        AdjustWindowRectEx(&rect, wndStyle, FALSE, wndExStyle);
        fullW = rect.right - rect.left;
        fullH = rect.bottom - rect.top;
    }

    void poll()
    {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) {
                doFinish = true;
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    void swapBuffers()
    {
        SwapBuffers(dc);
    }

    ATOM classAtom;
    HINSTANCE instance;
    DWORD wndStyle;
    DWORD wndExStyle;
    HWND window;
    HDC dc;
    HGLRC context;

    bool doFinish;

    UserCallback updateFun;
    void* updateArg;

    UserCallback renderFun;
    void* renderArg;
};

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WindowContext* window = (WindowContext*) GetWindowLongPtr(hwnd, 0);

    switch (msg)
    {
        case WM_CREATE:
        {
            CREATESTRUCT* cs = (CREATESTRUCT*) lParam;
            SetWindowLongPtr(hwnd, 0, (LONG_PTR) cs->lpCreateParams);
            break;
        }

        case WM_CLOSE:
        {
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

class Game
{
public:
    Game(): r(0.f), g(0.f), b(0.f), rd(0.001f), gd(0.005f), bd(0.0025f)
    {
    }

    void update()
    {
        if (r > 1.0f || r < 0.f) rd = -rd;
        if (g > 1.0f || g < 0.f) gd = -gd;
        if (b > 1.0f || b < 0.f) bd = -bd;
        r += rd;
        g += gd;
        b += bd;
    }

    void render()
    {
        glClearColor(r, g, b, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

private:
    float r;
    float g;
    float b;

    float rd;
    float gd;
    float bd;
};

void update(void* arg)
{
    if (arg != NULL) {
        ((Game*)arg)->update();
    }
}

void render(void* arg)
{
    if (arg != NULL) {
        ((Game*)arg)->render();
    }
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    Game game;

    WindowContext* window = WindowContext::open(100, 100, 640, 480);
    window->setUpdateCallback(update, &game);
    window->setRenderCallback(render, &game);
    window->setClientSize(800, 480);
    window->run();
    delete window;

    return 0;
}
