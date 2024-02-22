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

#include <cstdint>
#include <limits>
#include <algorithm> //std::clamp

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace vkAppStruct {
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

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities; // Capabilities of the surface: min/max number of images in a swapchain - min/max size of an image
		std::vector<VkSurfaceFormatKHR> formats; // Format of the image that the surface can display
		std::vector<VkPresentModeKHR> presentModes; // Present modes available for the surface
	};
}//end vkAppStruct
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
	VkSwapchainKHR m_swapchain;

	VkQueue m_graphicQueue; //handler of the graphic queue
	VkQueue m_presentQueue; //handler of the present queue

	VkSurfaceKHR m_surface;
	
	vkAppStruct::QueueFamilyIndices m_queueIndices;
	vkAppStruct::SwapChainSupportDetails m_swapchainDetails;

	VkSurfaceFormatKHR m_format;
	VkPresentModeKHR m_presentMode;

	VkPipeline m_pipeline;

	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	std::vector<VkImage> m_swapchainImages;
	std::vector<VkImageView> m_imageViews; //Each image requires an image view to be presented. Describes: 1. How to access image 2. Which part to access

	void initWindow();
	void initVulkan();
	void mainLoop();
	void cleanup();

	void createVkInstance();
	void createSurface();
	void selectPhysicalDevice();
	void createLogicalDevice();
	void createSwapChain();
	void createImageView();
	void createGraphicsPipeline(); //a pipelline is created to draw an image to screen

	// helper function to check the suitability of the physical devices
	bool isDeviceSuitable(VkPhysicalDevice device);
	bool checkValidationLayerSupport();
	bool checkDeviceExtensionSupport(VkPhysicalDevice device); //GPU should support swapchain : Device ---> |  swap chain | ---> screen / surface
	
	
	// helper function to query the required features
	vkAppStruct::QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	vkAppStruct::SwapChainSupportDetails findSwapChainSupportDetails(VkPhysicalDevice device);

	// helper functions to select the appropriated display format and mode
	VkSurfaceFormatKHR selectSurfaceDisplayFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR selectSurfaceDisplayMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D selectSurfaceDisplayExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	// helper functions to retrieve the images in the swapchain
	void getSwapChainImages();
};