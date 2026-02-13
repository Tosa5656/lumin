#pragma once

#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <algorithm>

#include "window.h"

class WindowManager
{
public:
    WindowManager() {};
    ~WindowManager() {};

    static void AddWindow(Window* window);
    static void RemoveWindow(Window* window);
    static void UpdateWindows();

    static bool IsEmpty() { return is_empty; }
private:
    static std::vector<Window*> m_windows;
    static bool is_empty;
};