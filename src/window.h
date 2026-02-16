#pragma once

#include <iostream>

#include <GLFW/glfw3.h>

#include <vectors.h>
#include "renderer.h"
class Renderer;

class Window
{
public:
	Window();
	Window(std::string title, int width, int height);
	Window(std::string title, Vector2 size);

	~Window() { };

	bool Update();

	void SetTitle(std::string title);

	GLFWwindow* GetNativeWindow() { return m_window; }
	std::string GetTitle() { return m_title; }
	Vector2 GetSize() { return m_size; }
	Renderer* GetRenderer() { return m_renderer; }
private:
	void Init();

	//Callbacks
	static void m_framebuffer_resized_callback(GLFWwindow* window, int width, int height);

	GLFWwindow* m_window;
	
	std::string m_title = "";
	Vector2 m_size = Vector2(800, 600);

	Renderer* m_renderer;
};