#pragma once

#include <iostream>

#include <GLFW/glfw3.h>

#include <renderer.h>

class Window
{
public:
	Window();
	Window(std::string title, int width, int height);

	~Window() { };

	bool Update();

	GLFWwindow* GetNativeWindow() { return m_window; }
private:
	void Init();

	GLFWwindow* m_window;
	
	std::string m_title = "";
	int m_width = 0;
	int m_height = 0;
};