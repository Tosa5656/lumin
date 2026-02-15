#pragma once

#include <iostream>
#include <vector>
#include <cstring>
#include <optional>
#include <set>
#include <cstdint>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "window.h"
#include "shaders.h"

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

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool IsComplete()
	{
		return graphicsFamily.has_value();
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class Renderer
{
public:
	Renderer() {};
	Renderer(Window* window);
	~Renderer()
	{
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		vkDestroyInstance(m_instance, nullptr);
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
		for (auto framebuffer : m_framebuffers)
			vkDestroyFramebuffer(m_device, framebuffer, nullptr);
		for (auto imageView : m_swapchain_image_views)
        	vkDestroyImageView(m_device, imageView, nullptr);
		vkDestroyPipeline(m_device, m_pipeline, nullptr);
		vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
		vkDestroyRenderPass(m_device, m_render_pass, nullptr);
		vkDestroyCommandPool(m_device, m_cmd_pool, nullptr);
		vkDestroyFence(m_device, m_in_flight_fence, nullptr);
		vkDestroySemaphore(m_device, m_render_finished_semaphore, nullptr);
		vkDestroySemaphore(m_device, m_image_available_semaphore, nullptr);
		vkDestroyDevice(m_device, nullptr);
	};

	void Init();
	void DrawFrame();
private:
	bool CheckValidationLayerSupport();
	std::vector<const char*> GetRequiredExtensions();
	bool IsDeviceSuitable(VkPhysicalDevice device);
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

	Window* m_window;
	ShadersManager m_shaders_manager;

	VkInstance m_instance;
	VkApplicationInfo m_application_info{};
	VkInstanceCreateInfo m_create_info{};
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device;
	VkQueue m_graphics_queue;
	VkQueue m_present_queue;
	VkSurfaceKHR m_surface;
	VkSwapchainKHR m_swapchain;
	std::vector<VkImage> m_swapchain_images;
	VkFormat m_swapchain_image_format;
	VkExtent2D m_swapchain_extent;
	std::vector<VkImageView> m_swapchain_image_views;
	VkViewport m_viewport;
	VkRect2D m_scissor;
	VkPipelineLayout m_pipeline_layout;
	VkRenderPass m_render_pass;
	VkPipeline m_pipeline;
	std::vector<VkFramebuffer> m_framebuffers;
	VkCommandPool m_cmd_pool;
	std::vector<VkCommandBuffer> m_cmdbuffers;
	VkSemaphore m_image_available_semaphore;
	VkSemaphore m_render_finished_semaphore;
	VkFence m_in_flight_fence;
};