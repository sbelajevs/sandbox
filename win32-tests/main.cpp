#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/GL.h>

#include <math.h>
#include <stdlib.h>

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

const unsigned char DEFAULT_VERTEX_SHADER[] = {
    0x23, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6F, 0x6E, 0x20, 0x31, 0x32, 0x30, 0x0D, 0x0A, 0x0D, 0x0A, 
    0x2F, 0x2F, 0x20, 0x49, 0x6E, 0x70, 0x75, 0x74, 0x20, 0x76, 0x65, 0x72, 0x74, 0x65, 0x78, 0x20, 
    0x64, 0x61, 0x74, 0x61, 0x2C, 0x20, 0x64, 0x69, 0x66, 0x66, 0x65, 0x72, 0x65, 0x6E, 0x74, 0x20, 
    0x66, 0x6F, 0x72, 0x20, 0x61, 0x6C, 0x6C, 0x20, 0x65, 0x78, 0x65, 0x63, 0x75, 0x74, 0x69, 0x6F, 
    0x6E, 0x73, 0x20, 0x6F, 0x66, 0x20, 0x74, 0x68, 0x69, 0x73, 0x20, 0x73, 0x68, 0x61, 0x64, 0x65, 
    0x72, 0x2E, 0x0D, 0x0A, 0x61, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x20, 0x76, 0x65, 
    0x63, 0x32, 0x20, 0x76, 0x65, 0x72, 0x74, 0x65, 0x78, 0x50, 0x6F, 0x73, 0x69, 0x74, 0x69, 0x6F, 
    0x6E, 0x5F, 0x6D, 0x6F, 0x64, 0x65, 0x6C, 0x73, 0x70, 0x61, 0x63, 0x65, 0x3B, 0x0D, 0x0A, 0x61, 
    0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x20, 0x76, 0x65, 0x63, 0x32, 0x20, 0x76, 0x65, 
    0x72, 0x74, 0x65, 0x78, 0x55, 0x76, 0x3B, 0x0D, 0x0A, 0x0D, 0x0A, 0x76, 0x61, 0x72, 0x79, 0x69, 
    0x6E, 0x67, 0x20, 0x76, 0x65, 0x63, 0x32, 0x20, 0x76, 0x55, 0x76, 0x3B, 0x0D, 0x0A, 0x0D, 0x0A, 
    0x75, 0x6E, 0x69, 0x66, 0x6F, 0x72, 0x6D, 0x20, 0x6D, 0x61, 0x74, 0x34, 0x20, 0x4D, 0x56, 0x50, 
    0x3B, 0x0D, 0x0A, 0x0D, 0x0A, 0x76, 0x6F, 0x69, 0x64, 0x20, 0x6D, 0x61, 0x69, 0x6E, 0x28, 0x29, 
    0x0D, 0x0A, 0x7B, 0x0D, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x67, 0x6C, 0x5F, 0x50, 0x6F, 0x73, 0x69, 
    0x74, 0x69, 0x6F, 0x6E, 0x20, 0x3D, 0x20, 0x20, 0x4D, 0x56, 0x50, 0x20, 0x2A, 0x20, 0x76, 0x65, 
    0x63, 0x34, 0x28, 0x76, 0x65, 0x72, 0x74, 0x65, 0x78, 0x50, 0x6F, 0x73, 0x69, 0x74, 0x69, 0x6F, 
    0x6E, 0x5F, 0x6D, 0x6F, 0x64, 0x65, 0x6C, 0x73, 0x70, 0x61, 0x63, 0x65, 0x2C, 0x20, 0x30, 0x2C, 
    0x20, 0x31, 0x29, 0x3B, 0x0D, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x76, 0x55, 0x76, 0x20, 0x3D, 0x20, 
    0x76, 0x65, 0x72, 0x74, 0x65, 0x78, 0x55, 0x76, 0x3B, 0x0D, 0x0A, 0x7D, 0x0D, 0x0A, 0x00, 
};

const unsigned char DEFAULT_FRAG_SHADER[] = {
    0x23, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6F, 0x6E, 0x20, 0x31, 0x32, 0x30, 0x0D, 0x0A, 0x0D, 0x0A, 
    0x76, 0x61, 0x72, 0x79, 0x69, 0x6E, 0x67, 0x20, 0x76, 0x65, 0x63, 0x32, 0x20, 0x76, 0x55, 0x76, 
    0x3B, 0x0D, 0x0A, 0x0D, 0x0A, 0x75, 0x6E, 0x69, 0x66, 0x6F, 0x72, 0x6D, 0x20, 0x73, 0x61, 0x6D, 
    0x70, 0x6C, 0x65, 0x72, 0x32, 0x44, 0x20, 0x73, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x72, 0x3B, 0x0D, 
    0x0A, 0x0D, 0x0A, 0x76, 0x6F, 0x69, 0x64, 0x20, 0x6D, 0x61, 0x69, 0x6E, 0x28, 0x29, 0x0D, 0x0A, 
    0x7B, 0x0D, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x67, 0x6C, 0x5F, 0x46, 0x72, 0x61, 0x67, 0x43, 0x6F, 
    0x6C, 0x6F, 0x72, 0x20, 0x3D, 0x20, 0x74, 0x65, 0x78, 0x74, 0x75, 0x72, 0x65, 0x32, 0x44, 0x28, 
    0x73, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x72, 0x2C, 0x20, 0x76, 0x55, 0x76, 0x29, 0x3B, 0x2F, 0x2F, 
    0x20, 0x2A, 0x20, 0x30, 0x2E, 0x38, 0x20, 0x2B, 0x20, 0x76, 0x65, 0x63, 0x34, 0x28, 0x30, 0x2E, 
    0x38, 0x38, 0x2C, 0x20, 0x30, 0x2E, 0x36, 0x36, 0x2C, 0x20, 0x30, 0x2E, 0x36, 0x35, 0x2C, 0x20, 
    0x31, 0x2E, 0x30, 0x29, 0x20, 0x2A, 0x20, 0x30, 0x2E, 0x32, 0x3B, 0x0D, 0x0A, 0x7D, 0x0D, 0x0A, 
    0x00, 
};

struct Graphics
{
    Graphics()
        : initialized(false)
        , textureLen(0)
    {
    }

    ~Graphics()
    {
        if (initialized == false) {
            return;
        }

        DeleteVertexArrays(1, &vertexArray);
        glDeleteTextures((GLsizei)textureLen, textures);
    }

    void init()
    {
        GenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)wglGetProcAddress("glGenVertexArrays");
        BindVertexArray = (PFNGLBINDVERTEXARRAYPROC)wglGetProcAddress("glBindVertexArray");
        DeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)wglGetProcAddress("glDeleteVertexArrays");
        CreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
        ShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
        CompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
        GetShaderiv = (PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv");
        GetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");
        CreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
        AttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
        LinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
        DeleteShader = (PFNGLDELETESHADERPROC)wglGetProcAddress("glDeleteShader");
        GetProgramiv = (PFNGLGETPROGRAMIVPROC)wglGetProcAddress("glGetProgramiv");
        GetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)wglGetProcAddress("glGetProgramInfoLog");
        GetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
        // TODO: add sanity checks for obtained procedures

        GenVertexArrays(1, &vertexArray);
        BindVertexArray(vertexArray);

        texShader.id = buildShaderProgram((char*)DEFAULT_VERTEX_SHADER, (char*)DEFAULT_FRAG_SHADER);
        texShader.uniforms[UNIFORM_MVP] = GetUniformLocation(texShader.id, "MVP");
        texShader.uniforms[UNIFORM_TEX] = GetUniformLocation(texShader.id, "sampler");

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        initialized = true;
    }

    void setScreen(int w, int h)
    {
        static const float BASE_ORTHO[] = {
           2.f,  0.f, 0.f, 0.f,
           0.f, -2.f, 0.f, 0.f,
           0.f,  0.f, 0.f, 0.f,
          -1.f,  1.f, 0.f, 1.f,
        };
        memcpy(orthoProj, BASE_ORTHO, sizeof(BASE_ORTHO));
        orthoProj[0] /= w;
        orthoProj[5] /= h;

        glViewport(0, 0, w, h);
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

        textures[textureLen] = id;
        return textureLen++;
    }

private:
    GLuint compileShader(const char* shaderSrc, GLuint type)
    {
        GLuint shaderId = CreateShader(type);
        ShaderSource(shaderId, 1, &shaderSrc, NULL);
        CompileShader(shaderId);

        GLint result = GL_FALSE;
        GetShaderiv(shaderId, GL_COMPILE_STATUS, &result);
        if (result != GL_TRUE)
        {
            int infoLogLen = 0;
            GetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infoLogLen);
            clamp(infoLogLen, 0, 1023);
            char errorMsg[1024];
            GetShaderInfoLog(shaderId, infoLogLen, NULL, errorMsg);
            //fprintf(stderr, "Error compiling shader:\n%s\n", shaderSrc);
            //fprintf(stderr, "%s\n", errorMsg);
            exit(EXIT_FAILURE);
        }

        return shaderId;
    }
    
    GLuint buildShaderProgram(const char* vertexShaderSrc, const char* fragmentShaderSrc)
    {
        GLuint vertexShaderId = compileShader(vertexShaderSrc, GL_VERTEX_SHADER);
        GLuint fragmentShaderId = compileShader(fragmentShaderSrc, GL_FRAGMENT_SHADER);

        // Link
        GLuint programId = CreateProgram();
        AttachShader(programId, vertexShaderId);
        AttachShader(programId, fragmentShaderId);
        LinkProgram(programId);

        // Free resources
        DeleteShader(vertexShaderId);
        DeleteShader(fragmentShaderId);

        // Check for errors
        GLint result = GL_FALSE;
        GetProgramiv(programId, GL_LINK_STATUS, &result);
        if (result != GL_TRUE)
        {
            int infoLogLen = 0;
            GetProgramiv(programId, GL_INFO_LOG_LENGTH, &infoLogLen);
            clamp(infoLogLen, 0, 1023);
            char errorMsg[1024];
            GetProgramInfoLog(programId, infoLogLen, NULL, errorMsg);
            //fprintf(stderr, "%s\n", errorMsg);
            exit(EXIT_FAILURE);
        }
  
        return programId;
    }

    static const unsigned int UNIFORMS_MAX = 3;
    static const unsigned int UNIFORM_MVP = 0;
    static const unsigned int UNIFORM_TEX = 1;

    struct ShaderProgram
    {
        unsigned int id;
        unsigned int uniforms[UNIFORMS_MAX];
    };

    bool initialized;
    GLuint vertexArray;
    GLuint textures[16];
    int textureLen;

    float orthoProj[16];
    ShaderProgram texShader;

    #define GLAPIENTRY __stdcall
    typedef char GLchar;

    typedef void (GLAPIENTRY * PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint* arrs);
    typedef void (GLAPIENTRY * PFNGLBINDVERTEXARRAYPROC)(GLuint arr);
    typedef void (GLAPIENTRY * PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint* arrays);
    typedef GLuint (GLAPIENTRY * PFNGLCREATESHADERPROC)(GLenum type);
    typedef void (GLAPIENTRY * PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar** strings, const GLint* lengths);
    typedef void (GLAPIENTRY * PFNGLCOMPILESHADERPROC)(GLuint shader);
    typedef void (GLAPIENTRY * PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint* param);
    typedef void (GLAPIENTRY * PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
    typedef GLuint (GLAPIENTRY * PFNGLCREATEPROGRAMPROC)(void);
    typedef void (GLAPIENTRY * PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
    typedef void (GLAPIENTRY * PFNGLLINKPROGRAMPROC)(GLuint program);
    typedef void (GLAPIENTRY * PFNGLDELETESHADERPROC)(GLuint shader);
    typedef void (GLAPIENTRY * PFNGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint* param);
    typedef void (GLAPIENTRY * PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
    typedef GLint (GLAPIENTRY * PFNGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar* name);

    PFNGLGENVERTEXARRAYSPROC GenVertexArrays;
    PFNGLBINDVERTEXARRAYPROC BindVertexArray;
    PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays;
    PFNGLCREATESHADERPROC CreateShader;
    PFNGLSHADERSOURCEPROC ShaderSource;
    PFNGLCOMPILESHADERPROC CompileShader;
    PFNGLGETSHADERIVPROC GetShaderiv;
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;
    PFNGLCREATEPROGRAMPROC CreateProgram;
    PFNGLATTACHSHADERPROC AttachShader;
    PFNGLLINKPROGRAMPROC LinkProgram;
    PFNGLDELETESHADERPROC DeleteShader;
    PFNGLGETPROGRAMIVPROC GetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
    PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation;

    static const int GL_GENERATE_MIPMAP = 0x8191;
    static const int GL_TEXTURE_FILTER_CONTROL = 0x8500;
    static const int GL_TEXTURE_LOD_BIAS = 0x8501;
    static const int GL_FRAGMENT_SHADER = 0x8B30;
    static const int GL_VERTEX_SHADER = 0x8B31;
    static const int GL_COMPILE_STATUS = 0x8B81;
    static const int GL_INFO_LOG_LENGTH = 0x8B84;
    static const int GL_LINK_STATUS = 0x8B82;
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
        gfx.setScreen(w, h);
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
        gfx.setScreen(newW, newH);
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
    ctx->gfx.init();
    ctx->gfx.setScreen(p->width, p->height);

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
