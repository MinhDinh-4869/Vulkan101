#pragma once
#define GLFW_INCLUDE_VULKAN
#define VK_LOADER_DEBUG
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <set>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

class TriangleApplication
{
	//define the methods
public:
	void run();

private:
	GLFWwindow* m_window;
	VkInstance m_instance;
	VkPhysicalDevice m_physicalDevice;
	VkDevice m_device; //logical device handler

	VkQueue m_graphicQueue; //handler of the graphic queue
	VkQueue m_presentQueue; //handler of the present queue

	VkSurfaceKHR m_surface;

	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily; //Queue family of the device that allows graphic commands - Input
		std::optional<uint32_t> presentFamily;  //Queue family of the device that allows presenting to the surface - Output
		// A device must supports both input queue and output queue

		bool isComplete()
		{
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	void initWindow();
	void initVulkan();
	void mainLoop();
	void cleanup();

	void createVkInstance();
	bool checkValidationLayerSupport();
	void selectPhysicalDevice();
	bool isDeviceSuitable(VkPhysicalDevice device);
	void createLogicalDevice();
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	void createSurface();
};