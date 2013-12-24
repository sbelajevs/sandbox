#include <windows.h>
#include <gl/GL.h>

// TODO: add support for multiple monitors
// * check if maximizing works on both monitors correctly
// * check minimal window size
// * check maximal window size
// * check how resizing and positioning works

namespace {

const char WND_CLASS_NAME[] = "win32-tests";

LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);

int getPixelFormatId(const HDC& dc)
{
    int count = DescribePixelFormat(
        dc, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL);

    for (int i=1; i<=count; i++)
    {
        // TODO: choose the best from all
        PIXELFORMATDESCRIPTOR pfd;
        DescribePixelFormat(dc, i, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

        static const DWORD REQUIRED_FLAGS = 
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        if ((pfd.dwFlags & REQUIRED_FLAGS) != REQUIRED_FLAGS)
        {
            continue;
        }

        if (!(pfd.dwFlags & PFD_GENERIC_ACCELERATED) 
            && (pfd.dwFlags & PFD_GENERIC_FORMAT))
        {
            continue;
        }

        if (pfd.iPixelType != PFD_TYPE_RGBA)
        {
            continue;
        }

        if (pfd.cRedBits == 8 && pfd.cGreenBits == 8 
            && pfd.cBlueBits == 8 && pfd.cAlphaBits == 8
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
    typedef void (*GameCallback)(void* data);
    typedef void (*ResizeCallback)(void* data, int newW, int newH);

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
            wc.cbClsExtra    = 0;
            wc.cbWndExtra    = sizeof(void*) + sizeof(int);
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

            int wndW = -1;
            int wndH = -1;
            ctx->getWindowSize(w, h, wndW, wndH);

            ctx->window = CreateWindowEx(
               ctx->wndExStyle, WND_CLASS_NAME, 
               "My window", ctx->wndStyle,
               x, y, wndW, wndH,
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

        ctx->ready = true;
        return ctx;
    }

    void setCallbackArgument(void* arg)
    {
        callbackArg = arg;
    }

    void setUpdateCallback(GameCallback callback)
    {
        updateFun = callback;
    }

    void setRenderCallback(GameCallback callback)
    {
        renderFun = callback;
    }

    void setResizeCallback(ResizeCallback callback)
    {
        resizeFun = callback;
    }

    void getClientSize(int& w, int& h)
    {
        if (ready == false) { return; }

        RECT area;
        GetClientRect(window, &area);
        w = area.right;
        h = area.bottom;
    }

    void setClientSize(int w, int h)
    {
        if (ready == false) { return; }

        int wndW = -1;
        int wndH = -1;
        getWindowSize(w, h, wndW, wndH);
        SetWindowPos(window, HWND_TOP, 0, 0, wndW, wndH,
                     SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER);
    }

    void setMinSize(int w, int h)
    {
        minWidth = w;
        minHeight = h;
    }

    void getMinSize(int& w, int& h)
    {
        w = minWidth;
        h = minHeight;
    }

    void doResize(int newW, int newH)
    {
        if (resizeFun) { resizeFun(callbackArg, newW, newH); }
    }

    void doUpdateStep()
    {
        if (updateFun) { updateFun(callbackArg); }
    }

    void doRenderingStep()
    {
        if (renderFun)
        {
            renderFun(callbackArg);
            swapBuffers();
        }
    }

    void run()
    {
        if (ready == false) { return; }

        while (doFinish == false) 
        {
            poll();
            doUpdateStep();
            doRenderingStep();
            Sleep(15);
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
        : minWidth(1)
        , minHeight(1)
        , ready(false)
        , doFinish(false)
        , callbackArg(NULL)
        , updateFun(NULL)
        , renderFun(NULL)
        , resizeFun(NULL)
    {
    }

    void getWindowSize(int clientW, int clientH, int& wndW, int& wndH)
    {
        RECT rect = { 0, 0, clientW, clientH };
        AdjustWindowRectEx(&rect, wndStyle, FALSE, wndExStyle);
        wndW = rect.right - rect.left;
        wndH = rect.bottom - rect.top;
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

    int minWidth;
    int minHeight;

    bool ready;
    bool doFinish;

    void* callbackArg;
    GameCallback updateFun;
    GameCallback renderFun;
    ResizeCallback resizeFun;
};

static LRESULT CALLBACK wndProc(HWND   hwnd, 
                                UINT   msg, 
                                WPARAM wParam, 
                                LPARAM lParam)
{
    WindowContext* window = (WindowContext*)GetWindowLongPtr(hwnd, 0);
    
    switch (msg)
    {
        // TODO: see if we really need to handle something else here, 
        // like minimzation and focus lost/gain

        case WM_CREATE:
        {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            SetWindowLongPtr(hwnd, 0, (LONG)cs->lpCreateParams);
            break;
        }

        case WM_SIZE:
        {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            // TODO: reset also projection matrix
            glViewport(0, 0, w, h);
            window->doResize(w, h);
            window->doRenderingStep();
            return 0;
        }

        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            int minW = -1;
            int minH = -1;
            window->getMinSize(minW, minH);
            mmi->ptMinTrackSize.x = minW;
            mmi->ptMinTrackSize.y = minH;
            return 0;
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

} // anonymous namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    Game game;

    WindowContext* window = WindowContext::open(100, 100, 640, 480);
    window->setCallbackArgument(&game);
    window->setUpdateCallback(update);
    window->setRenderCallback(render);
    window->setClientSize(800, 480);
    window->setMinSize(320, 200);
    window->run();
    delete window;

    return 0;
}
