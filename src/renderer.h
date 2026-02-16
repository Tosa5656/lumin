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
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(m_device, m_render_finished_semaphores[i], nullptr);
			vkDestroySemaphore(m_device, m_image_available_semaphores[i], nullptr);
			vkDestroyFence(m_device, m_in_flight_fence[i], nullptr);
		}
		vkDestroyDevice(m_device, nullptr);
	};

	void Init();
	void DrawFrame();
	void Idle();
private:
	void CreateInstance();
	void CreateWindowSurface();
	void SelectPhysicalDevice();
	void InitQueues();
	void CreateLogicalDevice();
	void SaveQueues();
	void CreateSwapChain();
	void CreateImageViews();
	void CreateRenderPass();
	void CreateGraphicsPipeline();
	void CreateFrameBuffer();
	void CreateCommandPool();
	void CreateCommandBuffers();
	void CreateSyncObjects();

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

	const int MAX_FRAMES_IN_FLIGHT = 2;

	Window* m_window;
	ShadersManager m_shaders_manager;

	VkInstance m_instance;
	VkApplicationInfo m_application_info{};
	VkInstanceCreateInfo m_create_info{};
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device;
	VkQueue m_graphics_queue;
	VkQueue m_present_queue;
	std::vector<VkDeviceQueueCreateInfo> m_queue_create_infos;
	QueueFamilyIndices m_indices;
	float m_queue_priority = 1.0f;
	VkSurfaceKHR m_surface;
	VkSurfaceFormatKHR m_surface_format;
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
	std::vector<VkSemaphore> m_image_available_semaphores;
	std::vector<VkSemaphore> m_render_finished_semaphores;
	std::vector<VkFence> m_in_flight_fence;
	std::vector<VkFence> m_images_in_flight;
	int m_current_frame = 0;
};