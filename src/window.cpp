#include "window.h"

Window::Window(std::string title, int width, int height)
{
    m_title = title;
    m_size = Vector2(width, height);

    Init();
}

Window::Window(std::string title, Vector2 size)
{
    m_title = title;
    m_size = size;

    Init();
}

void Window::Init()
{
    if(!InitGLFW())
        return;

    m_window = glfwCreateWindow(m_size.x, m_size.y, m_title.c_str(), NULL, NULL);

    glfwMakeContextCurrent(m_window);
    int gl_version = gladLoadGL(glfwGetProcAddress);
}

bool Window::Update()
{
    if(glfwWindowShouldClose(m_window))
        return false;

    glfwMakeContextCurrent(m_window);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.5f, 0.1f, 0.1f, 1.0f);
    glfwSwapBuffers(m_window);

    return true;
}

std::vector<Window*> WindowManager::m_windows;
bool* WindowManager::m_spectator;

void WindowManager::UpdateWindows()
{
    for (auto it = m_windows.begin(); it != m_windows.end(); )
    {
        if (!(*it)->Update()) 
        {
            (*it)->Destroy();
            it = m_windows.erase(it);
        }
        else 
        {
            ++it;
        }
    }

    if(m_windows.size() == 0)
        *m_spectator = false;

    glfwPollEvents(); 
}

void WindowManager::SetSpectator(bool* spectator)
{
    m_spectator = spectator;
}