#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef int  (*SysExitCheckFun)(void* data);
typedef void (*SysGameFun)(void* data);
typedef void (*SysResizeFun)(void* data, int newW, int newH);

struct SysAPI;

struct WindowParams
{
    int width;
    int height;
    const char* windowTitle;
    void* gameObject;
    SysGameFun updateFun;
    SysGameFun renderFun;
    SysGameFun onCloseFun;
    SysResizeFun onResizeFun;
    SysExitCheckFun checkExitFun;
};

SysAPI* Sys_OpenWindow(const WindowParams* params);

void Sys_SetMinClientSize(SysAPI* sys, int minW, int minH);
void Sys_SetClientSize(SysAPI* sys, int w, int h);
void Sys_GetDisplayRes(SysAPI* sys, int* w, int* h);
float Sys_GetFrameTime(SysAPI* sys);

void Sys_RunApp(SysAPI* sys);
void Sys_Release(SysAPI* sys);

int Sys_LoadTexture(SysAPI* sys, const unsigned char* data, int w, int h);

void Sys_SetTexture(SysAPI* sys, int hTexture);
void Sys_ClearScreen(SysAPI* sys, float r, float g, float b);
void Sys_Render(SysAPI* sys, 
                float sx, float sy, 
                float sw, float sh, 
                float tx, float ty, 
                float tw, float th);

#ifdef __cplusplus
}
#endif
