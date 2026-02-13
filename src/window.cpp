#include "window.h"

Window::Window()
{

}

Window::Window(std::string title, int width, int height)
{
	m_title = title;
	m_width = width;
	m_height = height;

	Init();
}

void Window::Init()
{
	init_glfw();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
	if (!m_window)
		return;
}

bool Window::Update()
{
	if (glfwWindowShouldClose(m_window)) return false;
	glfwPollEvents();

	return true;
}