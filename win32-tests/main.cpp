#include <windows.h>
#include <gl/GL.h>

static const char WND_CLASS_NAME[] = "win32-tests";

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
            DWORD wndStyle = WS_CLIPSIBLINGS | WS_CLIPCHILDREN 
                | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU 
                | WS_MINIMIZEBOX |  WS_MAXIMIZEBOX | WS_SIZEBOX;
            DWORD wndExStyle = WS_EX_APPWINDOW;

            int fullW = w;
            int fullH = h;

            ctx->window = CreateWindowEx(
               wndExStyle, WND_CLASS_NAME, "My window", wndStyle,
               x, y, fullW, fullH,
               NULL, NULL, ctx->instance, NULL
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
        SetFocus(ctx->window);

        return ctx;
    }

    void run()
    {
        float rd = 0.001f;
        float gd = 0.005f;
        float bd = 0.0025f;

        float r = 0;
        float g = 0;
        float b = 0;

        while (doFinish == false) 
        {
            if (r > 1.0f || r < 0.f) rd = -rd;
            if (g > 1.0f || g < 0.f) gd = -gd;
            if (b > 1.0f || b < 0.f) bd = -bd;
            r += rd;
            g += gd;
            b += bd;

            Sleep(15);
            swapBuffers();
            poll();

            glClearColor(r, g, b, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);
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

    WindowContext(): doFinish(false)
    {
    }

    void poll()
    {
        MSG msg;

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            switch (msg.message)
            {
                case WM_QUIT:
                    doFinish = true;
                    break;

                default:
                    DispatchMessage(&msg);
                    break;
            }
        }
    }

    void swapBuffers()
    {
        SwapBuffers(dc);
    }

    bool doFinish;
    ATOM classAtom;
    HINSTANCE instance;
    HWND window;
    HDC dc;
    HGLRC context;
};

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    WindowContext* window = WindowContext::open(100, 100, 640, 480);
    window->run();
    delete window;

    return 0;
}
