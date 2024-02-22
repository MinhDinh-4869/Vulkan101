#include "TriangleApplication.h"

void TriangleApplication::run()
{
	initWindow();
	initVulkan();
	mainLoop();
	cleanup();
}

void TriangleApplication::initWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	//Create window
	m_window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
}

void TriangleApplication::initVulkan()
{
	createVkInstance();
	createSurface(); //Should be call right after create vulkan instance;
	selectPhysicalDevice(); //Select a suitable physical device
	createLogicalDevice(); //Create a logical device to interface with the physical device
	createSwapChain(); //Create swapchain after checking for GPU extension and surface capabilities
	getSwapChainImages();
	createImageView(); //To use the image in the swapchain, we have to create the image view
	createGraphicsPipeline(); //To create a graphic pipeline to bring image to screen. Input --> pipeline --> screen
}

void TriangleApplication::mainLoop()
{
	//Keep the window alive
	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();
	}
}

void TriangleApplication::cleanup()
{
	for (const auto& imageView : m_imageViews)
	{
		vkDestroyImageView(m_device, imageView, nullptr);
	}
	vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	vkDestroyDevice(m_device, nullptr);
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkDestroyInstance(m_instance, nullptr);//Instance should be destroyed last
	
	
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

void TriangleApplication::createVkInstance()
{
	if (!checkValidationLayerSupport())
	{
		throw std::runtime_error("Validation layer requested, but not found!");
	}

	VkApplicationInfo appInfo{};//A struct containing setting for a vulkan application

	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangled";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo{};

	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	/*
	System needs interface (extension) to interact with Vulkan --> GLFW will return the extension(s) it needs from Vulkan
	*/
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);//get the vulkan instance extension required by the GLFW

	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = glfwExtensions;

	//createInfo.enabledLayerCount = 0;

	createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
	createInfo.ppEnabledLayerNames = validationLayers.data();//Leave the custom error message for later

	if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}
}

void TriangleApplication::createSurface()
{
	if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create window surface");
	}

}

void TriangleApplication::selectPhysicalDevice()
{
	//Select physical device stored in VkPhysicalDevice
	m_physicalDevice = VK_NULL_HANDLE; //Set a default init value

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	if (deviceCount == 0)
	{
		throw std::runtime_error("failed to find GPUs with Vulkan support");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());//allocate an array to hold all of the physical device handler

	//Check for suitability
	for (const auto& device : devices)
	{
		if (isDeviceSuitable(device))
		{
			m_physicalDevice = device;//select the first one
			break;
		}
	}
	//Check for all devices, select the one that satisfies the requirement defined in isDeviceSuitable
	//A map could be used but leave it for later

	if (m_physicalDevice == VK_NULL_HANDLE)//no suitable device
	{
		throw std::runtime_error("failed to select a suitable GPU");
	}
}

void TriangleApplication::createLogicalDevice()
{
	//Prepare queue info
	vkAppStruct::QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

	// As we are having 2 queues: 1 for input and 1 for output
	std::vector<VkDeviceQueueCreateInfo> queueInfos;
	std::set<uint32_t> uniqueQueueFamilies{
		indices.graphicsFamily.value(),
		indices.presentFamily.value()
	};

	float queuePriority = 1.0f;
	for (uint32_t index : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueInfo;

		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = index;
		queueInfo.queueCount = 1;//Uses 1 queue only
		queueInfo.pNext = nullptr;
		queueInfo.pQueuePriorities = &queuePriority;

		queueInfos.push_back(queueInfo);
	}


	//Prepare device features
	VkPhysicalDeviceFeatures deviceFeatures{};

	//Prepare logical device creation info
	VkDeviceCreateInfo deviceInfo;
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
	deviceInfo.pQueueCreateInfos = queueInfos.data();

	deviceInfo.pEnabledFeatures = &deviceFeatures;

	//set up validation layer
	deviceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
	deviceInfo.ppEnabledLayerNames = validationLayers.data();
	deviceInfo.pNext = nullptr;

	//set up extension
	deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

	VkResult result = vkCreateDevice(m_physicalDevice, &deviceInfo, nullptr, &m_device);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create logical device");
	}
	else
	{
		std::cout << "Success";
	}

	//Get handlers for the queue
	vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicQueue);
	vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
}

void TriangleApplication::createSwapChain()
{
	//Get swapchain support detail
	m_swapchainDetails = findSwapChainSupportDetails(m_physicalDevice);

	//Select the properties from available ones
	m_format = selectSurfaceDisplayFormat(m_swapchainDetails.formats);
	m_presentMode = selectSurfaceDisplayMode(m_swapchainDetails.presentModes);
	VkExtent2D extent = selectSurfaceDisplayExtent(m_swapchainDetails.capabilities);

	uint32_t imageCount = m_swapchainDetails.capabilities.minImageCount + 1; //Avoid GPU internal waiting
	if (m_swapchainDetails.capabilities.maxImageCount > 0 && imageCount > m_swapchainDetails.capabilities.maxImageCount)
	{
		imageCount = m_swapchainDetails.capabilities.maxImageCount; // min(maxImageCount, minImageCount + 1)
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = m_format.format;
	createInfo.imageColorSpace = m_format.colorSpace;

	createInfo.imageExtent = extent;

	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	//[Device] in-->[queue families]-->out [Surface]
	//Select how to handle the image in the swap chain

	m_queueIndices = findQueueFamilies(m_physicalDevice);
	uint32_t QueueFamilyIndices[] = { m_queueIndices.graphicsFamily.value(), m_queueIndices.presentFamily.value() };

	if (m_queueIndices.graphicsFamily.value() == m_queueIndices.presentFamily.value()) //using one families for both in and out --> image should be exclusively belonging to that
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = QueueFamilyIndices;
	}
	else //different
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 0; //optional
		createInfo.pQueueFamilyIndices = nullptr; //optional
	}

	createInfo.preTransform = m_swapchainDetails.capabilities.currentTransform; //Rotating for example

	createInfo.presentMode = m_presentMode;
	createInfo.clipped = VK_TRUE; //dont care about the color pixels that are obscured. E.g: another window is in front of them
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	createInfo.oldSwapchain = VK_NULL_HANDLE; //handle the old swapchain in case it is recreated

	if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create swapchain");
	}
}

void TriangleApplication::createImageView() {
	
	m_imageViews.resize(m_swapchainImages.size()); // Reserve maximum capacity = all images in the swapchain
	for (int i = 0; i < m_swapchainImages.size(); i++)
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = m_swapchainImages[i];

		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = m_format.format;

		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1; //3D apps

		if (vkCreateImageView(m_device, &createInfo, nullptr, &m_imageViews[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create image view");
		}
	}
}

void TriangleApplication::createGraphicsPipeline()
{
	//Read the shaders: vertext shader and fragment shader
}
//Function to evaluate the suitability of the device
bool TriangleApplication::isDeviceSuitable(VkPhysicalDevice device){
	//return true; //for first simple application

	//Get properties and feature of the physical device
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	//Check if the physical device is suitable for our own application requirement
	
	//Find the queue family
	vkAppStruct::QueueFamilyIndices indices = findQueueFamilies(device);

	//Check if Swapchain is supported by both GPU and Surface
	bool isExtensionSupported = checkDeviceExtensionSupport(device);
	bool isSwapChainSurfaceSupported = false;
	if (isExtensionSupported)
	{
		vkAppStruct::SwapChainSupportDetails details = findSwapChainSupportDetails(device);
		isSwapChainSurfaceSupported = !details.formats.empty() && !details.presentModes.empty();
	}

	//Find the suitable device
	return 
		deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && 
		deviceFeatures.geometryShader && 
		indices.isComplete() && 
		isExtensionSupported &&
		isSwapChainSurfaceSupported; //App needs device has discrete GPU and available for geometryShader
}
//Function to check if ALL of the requested layers are available
bool TriangleApplication::checkValidationLayerSupport()
{
	// Get all available layers
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);//get the size

	std::vector<VkLayerProperties> availableLayers(layerCount);//pass the size to the vector
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	//Check if all requested layers are contained in the available layers
	//for (const char* layerName : validationLayers)
	//{
	//	bool layerFound = false;

	//	for (const auto& layerProperties : availableLayers)
	//	{
	//		if (strcmp(layerName, layerProperties.layerName) == 0)
	//		{
	//			layerFound = true;
	//			break;
	//		}
	//	}

	//	if (!layerFound)
	//	{
	//		return false; //Return false if exist 1 layer not found in the available layers
	//	}
	//}
	std::set<std::string> requiredLayers(validationLayers.begin(), validationLayers.end());
	for (const auto& layer : availableLayers)
	{
		requiredLayers.erase(layer.layerName);
	}
	return requiredLayers.empty();
}

bool TriangleApplication::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	//Get the number of extension supported by the device
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtension(deviceExtensions.begin(), deviceExtensions.end()); //copy the name of the extension from the device extension vector
	for (const auto& extension : availableExtensions)
	{
		requiredExtension.erase(extension.extensionName); //if the required one is contained in the available -> remove it
	}
	return requiredExtension.empty(); //if all required extensions are available, they will be removed
}

vkAppStruct::QueueFamilyIndices TriangleApplication::findQueueFamilies(VkPhysicalDevice device)
{
	vkAppStruct::QueueFamilyIndices indices;
	//logic to find queue family indices

	//Get the queue families of a device
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyProperties.data());

	//Select the appropriate queue
	int index = 0;
	for (const auto& queueFamily : queueFamilyProperties)
	{
		// Check for input queue available
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = index;
		}

		// Check for output queue available
		VkBool32 isPresentSupported = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, index, m_surface, &isPresentSupported);
		if (isPresentSupported)
		{
			indices.presentFamily = index;
		}

		if (indices.isComplete())
		{
			break; //break when a suitable family is found
		}
		index++;
	}
	return indices;
}

vkAppStruct::SwapChainSupportDetails TriangleApplication::findSwapChainSupportDetails(VkPhysicalDevice device)
{
	//Required data before a swapchain could be created.
	vkAppStruct::SwapChainSupportDetails details;
	//logic to find the detail

	//Get the surface capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities); 

	uint32_t formatCounts = 0;
	uint32_t presentModeCounts = 0;

	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCounts, nullptr);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCounts, nullptr);

	if (formatCounts != 0)
	{
		details.formats.resize(formatCounts);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCounts, details.formats.data());
	}

	if (presentModeCounts != 0)
	{
		details.presentModes.resize(presentModeCounts);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCounts, details.presentModes.data());
	}

	return details;
}

VkSurfaceFormatKHR TriangleApplication::selectSurfaceDisplayFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	// Each VkSurfaceFormatKHR struct contains: format and color space
	for (const auto& format : availableFormats)
	{
		if (format.format == VK_FORMAT_R8G8B8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR TriangleApplication::selectSurfaceDisplayMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	for (const auto& mode : availablePresentModes)
	{
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return mode;
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR; //Default
}

VkExtent2D TriangleApplication::selectSurfaceDisplayExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	//Select the resolution between the available range
	//Usually equal to the resolution of the window - in pixels
	//GLFW has 2 measurements for resolution: pixels and screen coordinate
	//Vulkan works with pixels - use glfwGetFrameBufferSize to get the pixels of the window/buffer
	if (capabilities.currentExtent.width != /*std::numeric_limits<uint32_t>::max()*/UINT32_MAX)
	{
		return capabilities.currentExtent; //the current resolution of the window is indicated in currentExtent
	}
	else //if the window managers do allow us to modify the extent to best match with the min/max possible extent -> the wd managers will set the currentExtent to max
	{
		int width, height;
		glfwGetFramebufferSize(m_window, &width, &height); //Get the window resolution in pixels

		VkExtent2D actualExtent
		{
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		//clamp the actual width and height of the window to be bounded by min/maxImageExtent
		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

void TriangleApplication::getSwapChainImages()
{
	uint32_t imageCount = 0;
	vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);

	if (imageCount > 0)
	{
		m_swapchainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());
		std::cout << "Swapchain has " << imageCount << " images" << std::endl;
	}
	else
	{
		throw std::runtime_error("Empty swapchain");
	}
}