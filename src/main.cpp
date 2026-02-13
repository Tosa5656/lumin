#include <window.h>
#include <window_manager.h>

int main()
{
	Window window("Lumin Engine", 800, 600);
	Window window2("Lumin Engine", 800, 600);

	WindowManager::AddWindow(&window);
	WindowManager::AddWindow(&window2);

	while (!WindowManager::IsEmpty())
	{
		WindowManager::UpdateWindows();
	}

	destroy_glfw();
}