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
    }

    void render()
    {
        if (sys != NULL_PTR) {
            Sys_ClearScreen(sys, r, g, b);
        }
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
        NULL_PTR,
        checkForExit
    };

    game.sys = Sys_OpenWindow(&params);
    Sys_RunApp(game.sys);
    Sys_Release(game.sys);

    return 0;
}
