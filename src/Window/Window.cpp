#include <Window/Window.h>

Window::Window(std::string title, int width, int height, int x, int y)
{
    m_Window = nullptr;

	m_Title = title;
	m_Width = width;
	m_Height = height;
	m_X = x;
	m_Y = y;

    Init();
}

void Window::Init()
{
    if (!glfwInit())
        return;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_Window = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), NULL, NULL);
    glfwSetWindowPos(m_Window, m_X, m_Y);

    if (!m_Window)
    {
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(m_Window);

    int version = gladLoadGL();
    if (version == 0) {
        std::cout << "Failed to initialize OpenGL context" << std::endl;
        return;
    }

    SetBackgroundColor(Color(0.2f, 0.3f, 0.7f, 1.0f));

    while (!glfwWindowShouldClose(m_Window))
    {
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(m_Window);

        glfwPollEvents();
    }

    glfwTerminate();
}