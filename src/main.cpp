#include <iostream>

#include <lumin.h>

int main()
{
    Window window("Lumin Engine", 800, 600);
    Window window2("Lumin Engine", 800, 600);

    WindowManager::AddWindow(&window);
    WindowManager::AddWindow(&window2);

    bool is_working = true;
    WindowManager::SetSpectator(&is_working);

    while(is_working)
    {
        WindowManager::UpdateWindows();
    }

    DestroyGLFW();
    return 0;
}