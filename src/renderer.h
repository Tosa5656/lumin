#pragma once

#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

static bool is_glfw_inited = false;

inline void init_glfw()
{
	if (!is_glfw_inited)
	{
		glfwInit();
		is_glfw_inited = true;
	}
}

inline void destroy_glfw()
{
	if (is_glfw_inited)
	{
		glfwTerminate();
		is_glfw_inited = false;
	}
}