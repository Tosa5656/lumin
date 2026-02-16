#include "window_manager.h"

bool WindowManager::is_empty;
std::vector<Window*> WindowManager::m_windows;

void WindowManager::AddWindow(Window* window)
{
	m_windows.push_back(window);
}

void WindowManager::RemoveWindow(Window* window)
{
	glfwDestroyWindow(window->GetNativeWindow());
	m_windows.erase(std::remove(m_windows.begin(), m_windows.end(), window), m_windows.end());
}

void WindowManager::UpdateWindows()
{
	if (m_windows.empty())
	{
		is_empty = true;
		return;
	}

	for (Window* window : m_windows)
	{
		if (!window->Update())
		{
			window->GetRenderer()->Idle();
			RemoveWindow(window);
		}
	}
}