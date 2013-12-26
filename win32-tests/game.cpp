#include "system.h"

static const int NULL_PTR = 0;

class Game
{
public:
    Game()
        : r(0.f), g(0.f), b(0.f)
        , rd(0.001f), gd(0.005f), bd(0.0025f)
        , finished(false), askCount(0)
        , sys(NULL_PTR)
    {
    }

    void init()
    {
        typedef unsigned char byte;
        static const int WIDTH = 256;
        static const int HEIGHT = 256;

        byte* bitmap = new byte[WIDTH*HEIGHT*4];

        for (int y=0; y<HEIGHT; y++) {
            for (int x=0; x<WIDTH; x++) 
            {
                int pos = (y*WIDTH+x)*4;
                bitmap[pos+0] = (x+y)/2;
                bitmap[pos+1] = (x+y)/2;
                bitmap[pos+2] = (x+y)/2;
                bitmap[pos+3] = 255;
            }
        }

        Sys_LoadTexture(sys, bitmap, WIDTH, HEIGHT);

        delete[] bitmap;
    }

    void update()
    {
        if (r > 1.0f || r < 0.f) {
            rd = -rd;
        }
        if (g > 1.0f || g < 0.f) {
            gd = -gd;
        }
        if (b > 1.0f || b < 0.f) {
            bd = -bd;
        }
        r += rd;
        g += gd;
        b += bd;

        int mbs = Sys_GetMouseButtonState(sys);
        if (mbs & (int)MOUSE_BUTTON_LEFT) {
            r = 1.f; g = 0.f; b = 0.f;
        }
        if (mbs & (int)MOUSE_BUTTON_RIGHT) {
            r = 0.f; g = 1.f; b = 0.f;
        }
        if (mbs & (int)MOUSE_BUTTON_BACK) {
            r = 0.f; g = 0.f; b = 1.f;
        }
        if (mbs & (int)MOUSE_BUTTON_FWRD) {
            r = 1.f; g = 1.f; b = 0.f;
        }
    }

    void render()
    {
        if (sys == NULL_PTR) {
            return;
        }

        Sys_ClearScreen(sys, r, g, b);
        float baseX = r * width;
        float baseY = g * height;
        for (int i=0; i<10; i++) {
            Sys_Render(sys, baseX+i*10.f, baseY+i*10.f, 50.f, 50.f, 0.f, 0.f, 1.f, 1.f);
        }
    }

    void resize(int w, int h)
    {
        width = w;
        height = h;
    }

    void askForExit()
    {
        askCount++;
        if (askCount >= 2) {
            finished = true;
        }
    }

    bool gameFinished()
    {
        return finished;
    }

private:
    float r;
    float g;
    float b;

    float rd;
    float gd;
    float bd;

    bool finished;
    int askCount;

    int width;
    int height;

public:
    SysAPI* sys;
};

void update(void* arg)
{
    if (arg != NULL_PTR) {
        ((Game*)arg)->update();
    }
}

void render(void* arg)
{
    if (arg != NULL_PTR) {
        ((Game*)arg)->render();
    }
}

void onClose(void* arg)
{
    if (arg != NULL_PTR) {
        ((Game*)arg)->askForExit();
    }
}

void onResize(void* arg, int w, int h)
{
    if (arg != NULL_PTR) {
        ((Game*)arg)->resize(w, h);
    }
}

int checkForExit(void* arg)
{
    if (arg != NULL_PTR) {
        return ((Game*)arg)->gameFinished() ? 1 : 0;
    }
    return 0;
}

extern "C"
int Game_Run()
{
    Game game;

    WindowParams params = {
        640, 480, 
        "My window",
        &game,
        update,
        render,
        onClose,
        onResize,
        checkForExit
    };

    game.sys = Sys_OpenWindow(&params);

    game.init();

    Sys_RunApp(game.sys);
    Sys_Release(game.sys);

    return 0;
}
