#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>
#include <stdexcept>

class TriangleApplication
{
	//define the methods
public:
	void run();

private:
	GLFWwindow* m_window;
	VkInstance m_instance;


	void initWindow();
	void initVulkan();
	void mainLoop();
	void cleanup();

	void createVkInstance();
};