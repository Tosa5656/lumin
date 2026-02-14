#include "renderer.h"

Renderer::Renderer(Window* window)
{
	m_window = window;
}

void Renderer::Init()
{
	m_application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	m_application_info.pApplicationName = m_window->GetTitle().c_str();
	m_application_info.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
	m_application_info.pEngineName = "Lumin Engine";
	m_application_info.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
	m_application_info.apiVersion = VK_API_VERSION_1_2;


	auto extensions = getRequiredExtensions();

	m_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	m_create_info.pApplicationInfo = &m_application_info;
	m_create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	m_create_info.ppEnabledExtensionNames = extensions.data();

	if (enableValidationLayers && !checkValidationLayerSupport())
	{
		// Validation not supported
	}

	if (enableValidationLayers)
	{
		m_create_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		m_create_info.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		m_create_info.enabledLayerCount = 0;
	}

	VkResult result = vkCreateInstance(&m_create_info, nullptr, &m_instance);

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

	if (deviceCount == 0)
		return;
		// No devices founded

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	for (const auto& device : devices)
	{
		if (isDeviceSuitable(device))
		{
			m_physicalDevice = device;
			break;
		}
	}

	if (m_physicalDevice == VK_NULL_HANDLE)
	{
		// Not founded suitable GPU
	}
}

bool Renderer::checkValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
			return false;
	}
	return true;
}

std::vector<const char*> Renderer::getRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers)
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	return extensions;
}

bool Renderer::isDeviceSuitable(VkPhysicalDevice device)
{
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	return deviceFeatures.geometryShader;
}