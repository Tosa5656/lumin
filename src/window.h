#pragma once

#include <iostream>
#include <vector>
#include <algorithm>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "vectors.h"
#include "renderer.h"

class Window
{
public:
    Window() {};
    Window(std::string title, int width, int height);
    Window(std::string title, Vector2 size);

    ~Window() {};

    GLFWwindow* GetNativeWindow() { return m_window; }
    std::string GetTitle() { return m_title; }
    Vector2 GetSize() { return m_size; }

    bool Update();
    void Destroy() { glfwDestroyWindow(m_window); }
private:
    void Init();

    Renderer m_renderer;

    GLFWwindow* m_window;

    std::string m_title;
    Vector2 m_size;
};

class WindowManager
{
public:
    WindowManager() {};
    ~WindowManager() {};

    static void AddWindow(Window* window) { m_windows.push_back(window); }
    
    static void UpdateWindows();

    static void SetSpectator(bool* spectator);
private:
    static std::vector<Window*> m_windows;
    static bool* m_spectator;
};