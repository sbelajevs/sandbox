#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/GL.h>

#include <math.h>

#include "system.h"

// TODO: add support for multiple monitors
// * check if maximizing works on both monitors correctly
// * check minimal window size
// * check maximal window size
// * check how resizing and positioning works

namespace {

const char WND_CLASS_NAME[] = "win32-tests";

LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);

template <class T>
void clamp(T& value, const T& min, const T& max)
{
    if (value < min) {
        value = min;
    } else if (value > max) {
        value = max;
    }
}

int getDisplayRefreshRate(HWND window)
{
    HMONITOR hmon = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEX minfo;
    minfo.cbSize = sizeof(minfo);
    GetMonitorInfo(hmon, &minfo);

    DEVMODE devMode;
    devMode.dmSize = sizeof(devMode);
    devMode.dmDriverExtra = 0;
    EnumDisplaySettings(minfo.szDevice, ENUM_CURRENT_SETTINGS, &devMode);

    int result = (int)devMode.dmDisplayFrequency;
    clamp(result, 30, 120);

    return result;
}

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

        if (pfd.cRedBits == 8 
            && pfd.cGreenBits == 8 
            && pfd.cBlueBits  == 8 
            && pfd.cAlphaBits == 8
            && pfd.cDepthBits == 24 
            && pfd.cStencilBits == 8)
        {
            return i;
        }
    }

    return 0;
}

class HighResTimer
{
public:
    HighResTimer()
    {
        // TODO: we're assuming that it doesn't fail
        __int64 freq = 1;
        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        mResolution = 1.0/freq;
        reset();
    }

    void reset()
    {
        QueryPerformanceCounter((LARGE_INTEGER*) &mLastTime);
    }

    double getDeltaSeconds()
    {
        __int64 curTime = 0;
        QueryPerformanceCounter((LARGE_INTEGER*)&curTime);
        double result = (curTime-mLastTime) * mResolution;
        mLastTime = curTime;
        return result;
    }

private:
    double mResolution;
    __int64 mLastTime;
};

struct Graphics
{
    Graphics()
        : textureLen(0)
    {
    }

    int addTexture(const unsigned char* data, int w, int h)
    {
        GLuint id;

        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);

        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, -0.25f);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
                     (GLsizei)w, (GLsizei)h, 
                     0, GL_RGBA, 
                     GL_UNSIGNED_BYTE, data);

        textureIds[textureLen] = id;
        return textureLen++;
    }

    GLuint textureIds[16];

    int textureLen;

private:
    static const int GL_GENERATE_MIPMAP = 0x8191;
    static const int GL_TEXTURE_FILTER_CONTROL = 0x8500;
    static const int GL_TEXTURE_LOD_BIAS = 0x8501;
};

struct Win32Window
{
public:
    Win32Window()
        : mFrameTime(0.f)
        , mMinWidth(1)
        , mMinHeight(1)
        , mDoFinish(false)
        , mCallbackArg(NULL)
        , mUpdateFun(NULL)
        , mRenderFun(NULL)
        , mOnCloseFun(NULL)
        , mExitCheckFun(NULL)
        , mResizeFun(NULL)
    {
    }

    ~Win32Window()
    {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(mContext);
        mContext = NULL;

        ReleaseDC(mWindow, mDc);
        mDc = NULL;

        DestroyWindow(mWindow);
        mWindow = NULL;

        UnregisterClass(WND_CLASS_NAME, mInstance);
        mClassAtom = 0;
    }

    void getClientSize(int& w, int& h)
    {
        RECT area;
        GetClientRect(mWindow, &area);
        w = area.right;
        h = area.bottom;
    }

    void setClientSize(int w, int h)
    {
        int wndW = -1;
        int wndH = -1;
        getWindowSize(w, h, wndW, wndH);
        SetWindowPos(mWindow, HWND_TOP, 0, 0, wndW, wndH,
                     SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER);
    }

    void setMinClientSize(int w, int h)
    {
        mMinWidth = w;
        mMinHeight = h;
    }

    void getMinWindowSize(int& w, int& h)
    {
        getWindowSize(mMinWidth, mMinHeight, w, h);
    }

    void doResize(int newW, int newH)
    {
        if (mResizeFun) { 
            mResizeFun(mCallbackArg, newW, newH); 
        }
    }

    void doUpdateStep()
    {
        if (mUpdateFun) { 
            mUpdateFun(mCallbackArg); 
        }
    }

    void doRenderingStep()
    {
        if (mRenderFun) {
            mRenderFun(mCallbackArg);
            SwapBuffers(mDc);
        }
    }

    bool doCheckForExit()
    {
        return mExitCheckFun && mExitCheckFun(mCallbackArg);
    }

    void initFrameTime()
    {
        int refreshRate = getDisplayRefreshRate(mWindow);
        mFrameTime = 1.f / (float)refreshRate;
        mFrameTime -= 0.002f;
        clamp(mFrameTime, 1/120.f, 1/30.f);
    }

    void getWindowSize(int clientW, int clientH, int& wndW, int& wndH)
    {
        RECT rect = { 0, 0, clientW, clientH };
        AdjustWindowRectEx(&rect, mWndStyle, FALSE, mWndExStyle);
        wndW = rect.right - rect.left;
        wndH = rect.bottom - rect.top;
    }

    void poll()
    {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) 
            {
                if (mOnCloseFun) { 
                    mOnCloseFun(mCallbackArg); 
                } else { 
                    mDoFinish = true; 
                }
            } 
            else 
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    ATOM mClassAtom;
    HINSTANCE mInstance;
    DWORD mWndStyle;
    DWORD mWndExStyle;
    HWND mWindow;
    HDC mDc;
    HGLRC mContext;

    float mFrameTime;

    int mMinWidth;
    int mMinHeight;

    bool mDoFinish;

    void* mCallbackArg;
    SysGameFun mUpdateFun;
    SysGameFun mRenderFun;
    SysGameFun mOnCloseFun;
    SysExitCheckFun mExitCheckFun;
    SysResizeFun mResizeFun;

    Graphics gfx;
};

static LRESULT CALLBACK wndProc(HWND   hwnd, 
                                UINT   msg, 
                                WPARAM wParam, 
                                LPARAM lParam)
{
    Win32Window* window = (Win32Window*)GetWindowLongPtr(hwnd, 0);
    
    switch (msg)
    {
        // TODO: see if we really need to handle something else here, 
        // like minimization and focus lost/gain

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
            window->getMinWindowSize(minW, minH);
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

}  // anonymous namespace


struct SysAPI
{
    Win32Window wnd;
};

SysAPI* Sys_OpenWindow(const WindowParams* p)
{
    SysAPI* sys = new SysAPI();
    Win32Window* ctx = &sys->wnd;
    ctx->mInstance = GetModuleHandle(NULL);

    // Register a class first
    {
        WNDCLASS wc;
        ZeroMemory(&wc, sizeof(wc));

        wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc   = (WNDPROC) wndProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = sizeof(void*) + sizeof(int);
        wc.hInstance     = ctx->mInstance;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hIcon         = LoadIcon(NULL, IDI_WINLOGO);
        wc.lpszClassName = WND_CLASS_NAME;

        ctx->mClassAtom = RegisterClass(&wc);
    }

    // Now we're ready to open the window
    {
        ctx->mWndStyle = WS_CLIPSIBLINGS | WS_CLIPCHILDREN 
            | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU 
            | WS_MINIMIZEBOX |  WS_MAXIMIZEBOX | WS_SIZEBOX;
        ctx->mWndExStyle = WS_EX_APPWINDOW;

        int wndW = -1;
        int wndH = -1;
        ctx->getWindowSize(p->width, p->height, wndW, wndH);

        ctx->mWindow = CreateWindowEx(
            ctx->mWndExStyle, WND_CLASS_NAME, 
            p->windowTitle, ctx->mWndStyle,
            CW_USEDEFAULT, 0, wndW, wndH,
            NULL, NULL, ctx->mInstance, ctx
        );
    }

    // Create OpenGL context
    {
        ctx->mDc = GetDC(ctx->mWindow);
        PIXELFORMATDESCRIPTOR pfd;
        int bestPixelFormatId = getPixelFormatId(ctx->mDc);
        SetPixelFormat(ctx->mDc, bestPixelFormatId, &pfd);
        ctx->mContext = wglCreateContext(ctx->mDc);
        wglMakeCurrent(ctx->mDc, ctx->mContext);
    }

    // And finally, make it visible
    ShowWindow(ctx->mWindow, SW_SHOWNORMAL);
    BringWindowToTop(ctx->mWindow);
    SetForegroundWindow(ctx->mWindow);
    SetFocus(ctx->mWindow);

    ctx->initFrameTime();

    ctx->mCallbackArg = p->gameObject;
    ctx->mUpdateFun = p->updateFun;
    ctx->mRenderFun = p->renderFun;
    ctx->mOnCloseFun = p->onCloseFun;
    ctx->mResizeFun = p->onResizeFun;
    ctx->mExitCheckFun = p->checkExitFun;

    return sys;
}

void Sys_SetMinClientSize(SysAPI* sys, int minW, int minH)
{
    sys->wnd.setMinClientSize(minW, minH);
}

void Sys_SetClientSize(SysAPI* sys, int w, int h)
{
    sys->wnd.setClientSize(w, h);
}

void Sys_GetDisplayRes(SysAPI* sys, int* w, int* h)
{
    HMONITOR hmon = MonitorFromWindow(sys->wnd.mWindow, MONITOR_DEFAULTTONEAREST);
    MONITORINFO minfo;
    minfo.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(hmon, &minfo);
    *w = minfo.rcMonitor.right  - minfo.rcMonitor.left;
    *h = minfo.rcMonitor.bottom - minfo.rcMonitor.top;
}

float Sys_GetFrameTime(SysAPI* sys)
{
    return sys->wnd.mFrameTime;
}

void Sys_RunApp(SysAPI* sys)
{
     const float FRAME_TIME = sys->wnd.mFrameTime;
     float elapsedTime = 0.f;
     HighResTimer timer;

     while (sys->wnd.mDoFinish == false) 
     {
         if (sys->wnd.doCheckForExit()) {
             break;
         }

         elapsedTime += (float)timer.getDeltaSeconds();

         sys->wnd.poll();
         // Do no more than 3 updates, if more then something is wrong
         for (int i=0; i<3 && elapsedTime>FRAME_TIME; i++) {
             sys->wnd.doUpdateStep();
             elapsedTime -= FRAME_TIME;
         }
         clamp(elapsedTime, 0.f, FRAME_TIME);
         sys->wnd.doRenderingStep();

         float currentFrameTime = (float)timer.getDeltaSeconds();
         elapsedTime += currentFrameTime;
         float sleepTime = FRAME_TIME - currentFrameTime;
         if (sleepTime > 0.f) {
             Sleep((DWORD)floor(sleepTime * 1000));
         }
    }
}

int Sys_LoadTexture(SysAPI* sys, const unsigned char* data, int w, int h)
{
    return sys->wnd.gfx.addTexture(data, w, h);
}

void Sys_ClearScreen(SysAPI* sys, float r, float g, float b)
{
    glClearColor(r, g, b, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Sys_Release(SysAPI* sys)
{
    delete sys;
}

extern "C" int Game_Run();

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    int result = Game_Run();
    return result;
}
