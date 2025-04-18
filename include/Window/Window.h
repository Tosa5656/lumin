#pragma once

#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include <Vectors/Vectors.h>
#include <Shaders/Shaders.h>
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