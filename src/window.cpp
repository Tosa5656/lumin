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

	m_window = glfwCreateWindow(m_size.x, m_size.y, m_title.c_str(), nullptr, nullptr);
	if (!m_window)
		return;

	glfwSetWindowUserPointer(m_window, this);

	glfwSetFramebufferSizeCallback(m_window, m_framebuffer_resized_callback);

	m_renderer = new Renderer(this);
	m_renderer->Init();
}

bool Window::Update()
{
	if (glfwWindowShouldClose(m_window)) return false;
	glfwPollEvents();

	m_renderer->DrawFrame();

	return true;
}

// Callbacks
void Window::m_framebuffer_resized_callback(GLFWwindow* window, int width, int height)
{
	auto renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	renderer->SetFramebufferResized(true);
}