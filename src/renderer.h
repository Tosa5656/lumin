#pragma once

#include <iostream>
#include <vector>
#include <cstring>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "window.h"
class Window;

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

class Renderer
{
public:
	Renderer() {};
	Renderer(Window* window);
	~Renderer() { vkDestroyInstance(m_instance, nullptr); };

	void Init();
private:
	bool checkValidationLayerSupport();
	std::vector<const char*> getRequiredExtensions();
	bool isDeviceSuitable(VkPhysicalDevice device);

	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

	Window* m_window;

	VkInstance m_instance;
	VkApplicationInfo m_application_info{};
	VkInstanceCreateInfo m_create_info{};
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
};