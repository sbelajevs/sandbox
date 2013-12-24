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

class HighFreqTimer
{
public:
    HighFreqTimer()
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

struct SysAPI
{
public:
    SysAPI()
        : mFrameTime(0.f)
        , mMinWidth(1)
        , mMinHeight(1)
        , mReady(false)
        , mDoFinish(false)
        , mCallbackArg(NULL)
        , mUpdateFun(NULL)
        , mRenderFun(NULL)
        , mOnCloseFun(NULL)
        , mExitCheckFun(NULL)
        , mResizeFun(NULL)
    {
    }

    ~SysAPI()
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
        if (mReady == false) { 
            return; 
        }

        RECT area;
        GetClientRect(mWindow, &area);
        w = area.right;
        h = area.bottom;
    }

    void getDisplayResolution(int& w, int& h)
    {
        HMONITOR hmon = MonitorFromWindow(mWindow, MONITOR_DEFAULTTONEAREST);
        MONITORINFO minfo;
        minfo.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(hmon, &minfo);
        w = minfo.rcMonitor.right - minfo.rcMonitor.left;
        h = minfo.rcMonitor.bottom - minfo.rcMonitor.top;
    }

    void setClientSize(int w, int h)
    {
        if (mReady == false) { 
            return; 
        }

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

    void run()
    {
        if (mReady == false) { 
            return; 
        }

        mTimer.reset();
        float elapsedTime = 0.f;

        while (mDoFinish == false) 
        {
            if (mExitCheckFun && mExitCheckFun(mCallbackArg)) { 
                break; 
            }

            elapsedTime += (float)mTimer.getDeltaSeconds();

            poll();
            // Do no more than 3 updates, if more then something is wrong
            for (int i=0; i<3 && elapsedTime>mFrameTime; i++) {
                doUpdateStep();
                elapsedTime -= mFrameTime;
            }
            clamp(elapsedTime, 0.f, mFrameTime);
            doRenderingStep();

            float currentFrameTime = (float)mTimer.getDeltaSeconds();
            elapsedTime += currentFrameTime;
            float sleepTime = mFrameTime - currentFrameTime;
            if (sleepTime > 0.f) {
                Sleep((DWORD)floor(sleepTime * 1000));
            }
        }
    }

    void initFrameTime()
    {
        int refreshRate = getDisplayRefreshRate();
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

    int getDisplayRefreshRate()
    {
        HMONITOR hmon = MonitorFromWindow(mWindow, MONITOR_DEFAULTTONEAREST);
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

    bool mReady;
    bool mDoFinish;

    void* mCallbackArg;
    SysGameFun mUpdateFun;
    SysGameFun mRenderFun;
    SysGameFun mOnCloseFun;
    SysExitCheckFun mExitCheckFun;
    SysResizeFun mResizeFun;

    HighFreqTimer mTimer;
};

static LRESULT CALLBACK wndProc(HWND   hwnd, 
                                UINT   msg, 
                                WPARAM wParam, 
                                LPARAM lParam)
{
    SysAPI* window = (SysAPI*)GetWindowLongPtr(hwnd, 0);
    
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

SysAPI* Sys_OpenWindow(const WindowParams* p)
{
    SysAPI* ctx = new SysAPI();
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

    ctx->mCallbackArg = p->gameObject;
    ctx->mUpdateFun = p->updateFun;
    ctx->mRenderFun = p->renderFun;
    ctx->mOnCloseFun = p->onCloseFun;
    ctx->mResizeFun = p->onResizeFun;
    ctx->mExitCheckFun = p->checkExitFun;

    ctx->initFrameTime();
    ctx->mReady = true;
    return ctx;
}

void Sys_RunApp(SysAPI* sys)
{
    sys->run();
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
