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
	vkDestroyInstance(m_instance, nullptr);
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

void TriangleApplication::createVkInstance()
{
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

	createInfo.enabledLayerCount = 0;

	if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}
}