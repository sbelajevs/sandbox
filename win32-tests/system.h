#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct SysAPI;

int  Sys_LoadTexture(SysAPI* sys, const unsigned char* data, int w, int h);
void Sys_SetTexture(SysAPI* sys, int hTexture);
void Sys_ClearScreen(SysAPI* sys, float r, float g, float b);
void Sys_Render(SysAPI* sys, 
                float sx, float sy, 
                float sw, float sh, 
                float tx, float ty, 
                float tw, float th);

enum MouseButtonState
{
    MOUSE_BUTTON_NONE  = 0,
    MOUSE_BUTTON_LEFT  = 1,
    MOUSE_BUTTON_RIGHT = 2,
    MOUSE_BUTTON_BACK  = 4,
    MOUSE_BUTTON_FWRD  = 8,
};

int  Sys_GetMouseButtonState(SysAPI* sys);
void Sys_GetMousePos(SysAPI* sys, int* x, int* y);

#ifdef __cplusplus
}
#endif
