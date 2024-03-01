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
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

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
	createImageView(); //To use the image in the swapchain, we have to create the image view
	
	createRenderPass();//To describe the framebuffer attachments (color, depth, sampling, ... attached to the framebuffer) to Vulkan
	createGraphicsPipeline(); //To create a graphic pipeline to bring image to screen. Input --> pipeline --> screen
	createFrameBuffer(); //To create a big wrapper of what to be drawn to the screen
	createCommandPool(); //To create a command pool to store and allocate memory for the command buffers
	createCommandBuffer(); //To create a command buffer
	createSyncObjects();

}

void TriangleApplication::mainLoop()
{
	//Keep the window alive
	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();
		drawFrames();
		currentFrame = (currentFrame + 1) % MAX_IN_FLIGHT_FRAMES;
	}
	vkDeviceWaitIdle(m_device);
}

void TriangleApplication::cleanup()
{
	cleanup();

	for (int i = 0; i < MAX_IN_FLIGHT_FRAMES; i++)
	{
		vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
		vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
	}

	vkDestroyCommandPool(m_device, m_commandPool, nullptr);
	for (const auto& frameBuffer : m_swapChainFramebuffers)
	{
		vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
	}
	vkDestroyShaderModule(m_device, m_vertextShaderModule, nullptr);
	vkDestroyShaderModule(m_device, m_fragmentShaderModule, nullptr);
	vkDestroyPipeline(m_device, m_pipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
	vkDestroyRenderPass(m_device, m_renderPass, nullptr);
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

void TriangleApplication::drawFrames()
{
	//draw frames
	/*
	On GPU:
	Accquire the image from the swapchain <-----|
				|								|
				|								|
				V								|
	execute commands that draw the image		|
				|								|
				|								|
				V								|
	present the image to the screen				|
				|								|
				|								|
				V								|
	return the image to the swapchain-----------|

	All of them are executed asynchronously --> need synchronization

	Use Semaphore --> Wait on GPU --> does not block execution (on CPU)
	Fences --> Wait on CPU --> does block executions

	e.g: 
	Semaphore S;
	Task A, B;
	vkQueueSubmit(A, signal: S, wait: None);
	vkQueueSubmit(B, signal: None, wait: S); wait for signal S from A. A and B are executed quite the same time, but B waits for A on GPU

	Fence F;
	Task A, B;
	vkQueueSubmit(A, fence: F);
	vkWaitForFences(F); blocks the next execution until A done and F is signaled

	vkQueueSubmit(B, fence: F);
	*/

	vkWaitForFences(m_device, 1, &m_inFlightFences[currentFrame], VK_TRUE, UINT64_MAX); //wait for an in flight image to be rendered
	//Wait for previous frame to finish first, then the fence and the semaphore can be used
	//As wait for fence is waiting until fence is in signaled state, then first frame could be blocked infinitely --> set the state of the fence tobe signaled when it is created
	
	vkResetFences(m_device, 1, &m_inFlightFences[currentFrame]); //Reset the fence to the unsignaled state before reuse it again
	
	//Aquire next image from the swapchain
	uint32_t imageIndex;
	VkResult getImageResult = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
	if (getImageResult == VK_ERROR_OUT_OF_DATE_KHR)
	{
		recreateSwapchain();
		return;
	}
	else if (getImageResult != VK_SUCCESS && getImageResult != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("failed to acquire next image from swapchain");
	}
	//VK_ERROR_OUT_OF_DATE_KHR: the swapchain is out of date and can not perform presentation to the surface
	//VK_SUBOPTIMAL_KHR: the swapchain is still able to present the image to the surface, but the setting of the surface is not match --> wrongly display the image, but no error

	//record command buffer
	vkResetCommandBuffer(m_commandBuffers[currentFrame], 0);
	recordCommandBuffer(m_commandBuffers[currentFrame], imageIndex);

	//submit command buffer
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[currentFrame]}; //Which semaphores to wait on before execution begins
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; //Which pipeline state to wait on before execution begins
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandBuffers[currentFrame];

	VkSemaphore signalSemaphores[]{ m_renderFinishedSemaphores[currentFrame]}; //Which semaphores to signal once the command buffer has finished execution
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(m_graphicQueue, 1, &submitInfo, m_inFlightFences[currentFrame]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit draw command buffer");
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapchains[] = { m_swapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &imageIndex;

	presentInfo.pResults = nullptr;


	if (vkQueuePresentKHR(m_presentQueue, &presentInfo) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to present image");
	};
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
	auto result = glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface);
	if (result != VK_SUCCESS)
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
	m_extent = selectSurfaceDisplayExtent(m_swapchainDetails.capabilities);

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

	createInfo.imageExtent = m_extent;

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

	getSwapChainImages();
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

void TriangleApplication::createRenderPass()
{
	//Function to create a render pass
	VkAttachmentDescription attachmentDescriptor{};
	attachmentDescriptor.format = m_format.format;
	attachmentDescriptor.samples = VK_SAMPLE_COUNT_1_BIT;

	attachmentDescriptor.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; 
	// Defines what to do with the data in the attachment before rendering and after rendering.
	// It will clear the framebuffer before rendering and make it black before draw a new frame.
	attachmentDescriptor.storeOp = VK_ATTACHMENT_STORE_OP_STORE; //rendered content will be stored and can be read later on.

	attachmentDescriptor.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptor.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	//set up layout for the image. images need to be transitioned to specific layouts
	attachmentDescriptor.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //Which layout the image has before the render pass begin
	attachmentDescriptor.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; //After rendering (previous image), the current image should be ready for presentation via swapchain

	//Subpasss
	//Instead of a big render pass, we can divided it into several subpasses with separated logical phases, like a chain
	// subpass 1 --> subpass 2 --> subpass 3 ... subpass n: geometry pass -> lighting pass ...
	// Better for GPU to perform various optimization for each subpass
	// Subpasses need a reference to the attachment, not own them
	VkAttachmentReference attachmentRef{};
	attachmentRef.attachment = 0; //Index of the attachment in VkRenderPassCreateInfo::pAttachments. 1 attachment only --> index = 0
	attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //During the subpass the image will be move to this layout. Optimization here

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &attachmentRef;

	//Create render pass
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &attachmentDescriptor; //list of attachment descriptors
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass; //list of subpass


	//subpass dependencies
	//Controlling the image layout transition
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; //stage before this stage
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create renderpass");
	}
}

void TriangleApplication::createGraphicsPipeline()
{
	//Read the shaders: vertext shader and fragment shader
	std::vector<char> vertShaderCode = readFile("vert.spv");
	std::vector<char> fragShaderCode = readFile("frag.spv");

	//After having the code, wrap it in to a vKShaderModule
	m_vertextShaderModule = createShaderModule(vertShaderCode);
	m_fragmentShaderModule = createShaderModule(fragShaderCode);

	//Create a shader stage for the pipeline: Vertex shader and fragment shader
	//Wrapper of the code
	VkPipelineShaderStageCreateInfo vertextShaderStageInfo{};
	VkPipelineShaderStageCreateInfo fragmentShaderStageInfo{};

	vertextShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertextShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertextShaderStageInfo.module = m_vertextShaderModule;
	vertextShaderStageInfo.pName = "main"; //The entry point to call the shader function

	fragmentShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageInfo.module = m_fragmentShaderModule;
	fragmentShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStageInfos[] = { vertextShaderStageInfo, fragmentShaderStageInfo };
	
	/*SET UP FIXED-FUNCTION*/
	
	//Set up dynamic states
	//VK_DYNAMIC_STATE_VIEWPORT: make the viewport changeable in runtime while does not have to recreate the pipeline 
	//VK_DYNAMIC_STATE_SCISSOR: same
	VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateInfo.pDynamicStates = dynamicStates.data();

	//Set up vertex input
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	//We hard code the vertex input in the shader then these are not needed
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;

	//Set up input assembly
	//Defines how the vertices will be comsumed: point (1 vertex each), line (2 vertices each) or triangle (3 vertices each)
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
	inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyInfo.primitiveRestartEnable = VK_FALSE; //Not possible to break up lines and triangles in the _STRIP topology

	//Viewport and Scissor
	//Viewport: Describe the region of the framebuffer that the output will be rendered to --> the region on the screen, scaling the image to the viewport
	//Scissor: Describe the actual pixels on the screen that will be used to render the framebuffer.---> cut the viewport to the defined scissor
	VkViewport viewPort{};
	viewPort.x = 0.0f;
	viewPort.y = 0.0f;
	viewPort.width = (float) m_extent.width;
	viewPort.height = (float)m_extent.height;
	viewPort.minDepth = 0.0f;
	viewPort.maxDepth = 1.0f;

	VkRect2D scissor{}; //Rectangle2D
	scissor.offset = { 0,0 }; 
	scissor.extent = m_extent; //is extent is different to the current extent, the image will be cut to match the extent

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewPort;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	//Set up rasterize
	//Rasterization: transfrom shape created by vertices to fragments combined by pixels
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE; //If enable --> discard the fragments that are beyond the near and far plane. requires enabling a GPU feature 
	rasterizer.rasterizerDiscardEnable = VK_FALSE; //if true --> discard any output to the framebuffer
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	/*
	- VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments
	- VK_POLYGON_MODE_LINE: draw the polygon edges as line
	- VK_POLYGON_MODE_POINT: draw the polygon vertices as point
	*/
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; //What is culling? Droping objects that are theoretically not visible
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	//Set up multisampling
	//for Anti-aliasing
	//combining the fragment shader results of multiple polygons that rasterize to the same pixel: input ---fragment shader --> pixels
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	//Depth and stencil testing
	//Disable for now

	//Color blending
	/*
	- Mix old value and new value to create a final color
	- Combine old and new value using bitwise operation
	*/
	
	/* There are 2 structs for this
	- VkPipelineColorBlendAttachmentState: configuration per attached framebuffer (1 attached framebuffer only) --> attached to a framebuffer
	- VkPipelineColorBlendStateCreateInfo: GLOBAL configuration (all framebuffers)
	*/
	VkPipelineColorBlendAttachmentState colorBlendingAttachment{};
	colorBlendingAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT; //1111 for rgba
	colorBlendingAttachment.blendEnable = VK_FALSE; //No blending
	colorBlendingAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendingAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendingAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	colorBlendingAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendingAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendingAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

	VkPipelineColorBlendStateCreateInfo colorBlend{};
	colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlend.attachmentCount = 1;
	colorBlend.pAttachments = &colorBlendingAttachment;
	colorBlend.blendConstants[0] = 0.0f;
	colorBlend.blendConstants[1] = 0.0f;
	colorBlend.blendConstants[2] = 0.0f;
	colorBlend.blendConstants[3] = 0.0f;


	//Create pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0; //Optional
	pipelineLayoutInfo.pSetLayouts = nullptr; //Optional
	pipelineLayoutInfo.pushConstantRangeCount = 0; //Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; //Optional
	if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout");
	}

	//Create the pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStageInfos; // vertex shader stage and fragment shader stage

	pipelineInfo.pVertexInputState = &vertexInputInfo; 
	pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlend;
	pipelineInfo.pDynamicState = &dynamicStateInfo;

	pipelineInfo.layout = m_pipelineLayout;

	pipelineInfo.renderPass = m_renderPass;
	pipelineInfo.subpass = 0; //Index of the subpass

	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create graphics pipeline");
	}
}

void TriangleApplication::createFrameBuffer()
{
	m_swapChainFramebuffers.resize(m_imageViews.size());
	
	for (size_t i = 0; i < m_imageViews.size(); i++)
	{
		VkImageView attachments[] = { m_imageViews[i] }; 

		VkFramebufferCreateInfo frameBufferInfo{};
		frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferInfo.renderPass = m_renderPass;
		frameBufferInfo.attachmentCount = 1; //1 attachment in the framebuffer
		frameBufferInfo.pAttachments = attachments;
		frameBufferInfo.width = m_extent.width;
		frameBufferInfo.height = m_extent.height;
		frameBufferInfo.layers = 1;
		
		if (vkCreateFramebuffer(m_device, &frameBufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create framebuffer");
		}
	}
}

void TriangleApplication::createCommandPool()
{
	VkCommandPoolCreateInfo commandPoolInfo{};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; //individually reset the command buffers instead of all of them
	commandPoolInfo.queueFamilyIndex = m_queueIndices.graphicsFamily.value(); //Command buffers will be submitted to the input queue

	if (vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool");
	}
}

void TriangleApplication::createCommandBuffer()
{
	m_commandBuffers.resize(MAX_IN_FLIGHT_FRAMES);
	VkCommandBufferAllocateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	createInfo.commandBufferCount = 1;
	createInfo.commandPool = m_commandPool;
	createInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; //can be submitted directly to a queue for execution, secondary ones is called from primary(s)
	createInfo.commandBufferCount = MAX_IN_FLIGHT_FRAMES;

	if (vkAllocateCommandBuffers(m_device, &createInfo, m_commandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffer");
	}
}

void TriangleApplication::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to begin recording command buffer");
	}

	//Starting a render pass
	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_renderPass;
	renderPassBeginInfo.framebuffer = m_swapChainFramebuffers[imageIndex];
	
	renderPassBeginInfo.renderArea.extent = m_extent;
	renderPassBeginInfo.renderArea.offset = { 0,0 };

	VkClearValue clearColorValue = { {{0.0f, 0.0f, 0.0f, 1.0f}} }; //The color used to clear the frambuffer at the beginning of the renderpass
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = &clearColorValue;

	//begin the render pass
	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE); //The renderpass can now begin

	//Bind the command buffer to a pipeline
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline); //The second param defines if it is a graphics or compute pipeline

	//set the viewport and scissor
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.height = (float)m_extent.height;
	viewport.width = (float)m_extent.width;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.extent = m_extent;
	scissor.offset = { 0,0 };

	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	

	//Perform draw
	/*
	- vertexCount: number of vertices to be drawn
	- instanceCount: used for instanced rendering (draw an instance repeatedly). Set as 1 if dont use
	- firstVertex: first vertex to be drawn, usually the lowest value of gl_VertexIndex = index 0
	- firstInstance: same
	*/
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	//end writing to render pass
	vkCmdEndRenderPass(commandBuffer);
	//end writing to command buffer
	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to end the command buffer");
	}
}

void TriangleApplication::createSyncObjects()
{
	m_imageAvailableSemaphores.resize(MAX_IN_FLIGHT_FRAMES);
	m_renderFinishedSemaphores.resize(MAX_IN_FLIGHT_FRAMES);
	m_inFlightFences.resize(MAX_IN_FLIGHT_FRAMES);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //for the first frame to be not blocked

	for (int i=0; i<MAX_IN_FLIGHT_FRAMES; i++)
	{ 
		if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i])
			|| vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i])
			|| vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create synchronization objects");
		}
	}
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
		deviceFeatures.geometryShader && //App needs device has discrete GPU and available for geometryShader
		indices.isComplete() && 
		isExtensionSupported &&
		isSwapChainSurfaceSupported; 
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

std::vector<char> TriangleApplication::readFile(const std::string& fileName)
{
	std::ifstream file(fileName, std::ios::ate | std::ios::binary);
	//std::ios::ate: reading at the end of file --> can retrieve the size of the buffer
	//std::ios::binary: read the file as binary file --> avoid text transformation
	if (!file.is_open())
	{
		throw std::runtime_error("failed to open file");
	}
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize); //allocate the buffer

	file.seekg(0); //go back to the starting point of the file: 0
	file.read(buffer.data(), fileSize);

	file.close();
	return buffer;
}

VkShaderModule TriangleApplication::createShaderModule(std::vector<char> code)
{
	VkShaderModuleCreateInfo createInfo{};

	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create shader module");
	}

	return shaderModule;
}

void TriangleApplication::cleanupSwapchain()
{
	for (const auto& framebuffer : m_swapChainFramebuffers)
	{
		vkDestroyFramebuffer(m_device, framebuffer, nullptr);
	}

	for (const auto& imageView : m_imageViews)
	{
		vkDestroyImageView(m_device, imageView, nullptr);
	}

	vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
}
void TriangleApplication::recreateSwapchain()
{
	vkDeviceWaitIdle(m_device);

	cleanupSwapchain();

	createSwapChain(); //create new swapchain
	createImageView(); //create image views for the image in the new swapchain
	createFrameBuffer(); //create the frambuffer for the image views
}