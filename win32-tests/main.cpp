#include <windows.h>

int __stdcall WinMain(HINSTANCE__*, HINSTANCE__*, char*, int)
{
    MessageBox(NULL, "Hello, world!", "Important!", MB_OK | MB_ICONWARNING);
    return 0;
}
