#pragma once

#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <Vectors/Vectors.h>
#include <Renderer/Renderer.h>

class Window
{
public:
	Window(std::string title, int width, int height, int x, int y);
	~Window() {};
private:
	void Init();

	GLFWwindow* m_Window;

	std::string m_Title;
	int m_Width;
	int m_Height;
	int m_X;
	int m_Y;
};