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

//Function to check if ALL of the requested layers are available
bool TriangleApplication::checkValidationLayerSupport()
{
	// Get all available layers
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);//get the size

	std::vector<VkLayerProperties> availableLayers(layerCount);//pass the size to the vector
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	//Check if all requested layers are contained in the available layers
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
		{
			return false; //Return false if exist 1 layer not found in the available layers
		}
	}

	return true;
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

//Function to evaluate the suitability of the device
bool TriangleApplication::isDeviceSuitable(VkPhysicalDevice device){
	return true; //for first simple application

	//Get properties and feature of the physical device
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	//Check if the physical device is suitable for our own application requirement
	
	//Find the queue family
	QueueFamilyIndices indices = findQueueFamilies(device);

	//Find the suitable device
	return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader && indices.isComplete(); //App needs device has discrete GPU and available for geometryShader
}

void TriangleApplication::createLogicalDevice()
{
	//Prepare queue info
	QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

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

	deviceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
	deviceInfo.ppEnabledLayerNames = validationLayers.data();
	deviceInfo.pNext = nullptr;

	deviceInfo.enabledExtensionCount = 0;
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

TriangleApplication::QueueFamilyIndices TriangleApplication::findQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;
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
		vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, index, m_surface, &isPresentSupported);
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

void TriangleApplication::createSurface()
{
	if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create window surface");
	}

}
