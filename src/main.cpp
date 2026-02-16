#include <window.h>
#include <window_manager.h>

int main()
{
	Window window("Lumin Engine", 1280, 720);

	WindowManager::AddWindow(&window);

	while (!WindowManager::IsEmpty())
	{
		WindowManager::UpdateWindows();
	}

	destroy_glfw();

	return 0;
}