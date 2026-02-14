#include "window.h"

Window::Window()
{

}

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
	init_glfw();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	m_window = glfwCreateWindow(m_size.x, m_size.y, m_title.c_str(), nullptr, nullptr);
	if (!m_window)
		return;
}

bool Window::Update()
{
	if (glfwWindowShouldClose(m_window)) return false;
	glfwPollEvents();

	return true;
}