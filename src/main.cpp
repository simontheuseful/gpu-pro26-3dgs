#include "vulkan/VulkanInclude.hpp"
#include "vulkan/Device.hpp"
#include "vulkan/SwapChain.hpp"

#include <memory>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <map>
#include <limits>
#include <algorithm>
#include <fstream>
#include <filesystem>


const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;

// enable validation layers for debug checking only when on Debug mode (NDEBUG)
const std::vector<char const*> validationLayers={
	"VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else 
constexpr bool enableValidationLayers = true;
#endif

class HelloTriangleApplication
{
  public:
	void run()
	{
		window.initWindow();
		initVulkan();
		mainLoop();
		window.cleanup();
	}

  private:
	//GLFWwindow *window = nullptr;
	Window window;
	vk::raii::Context context;
	vk::raii::Instance instance = nullptr;	
	vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
	vk::raii::SurfaceKHR surface = nullptr;

	// order of declaration matters as the members will be destroyed in the opposite order 
	// and swapchain, buffers before the device
	Device device; 
	SwapChain swapChain;

	vk::raii::PipelineLayout pipelineLayout = nullptr;
	vk::raii::Pipeline graphicsPipeline = nullptr;

	vk::raii::CommandPool commandPool = nullptr;
	vk::raii::CommandBuffer commandBuffer = nullptr;

	vk::raii::Semaphore presentCompleteSemaphore = nullptr;
	vk::raii::Semaphore renderFinishedSemaphore  = nullptr;
	vk::raii::Fence     drawFence                = nullptr;

	void initVulkan()
	{
		createInstance();
		setupDebugMessenger();
		createSurface();
		device.create(instance, surface); // picks physical device and crates logical
		swapChain.create(device, window.getWindow(), surface); //create image Views inside
		createGraphicsPipeline();
		createCommandPool();
		createCommandBuffer();
		createSyncObjects();
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window.getWindow()))
		{
			glfwPollEvents();
			drawFrame();
		}

		device.getLogical().waitIdle();
	}

	void drawFrame()
	{
		auto fenceResult = device.getLogical().waitForFences(*drawFence, vk::True, UINT64_MAX);
		if (fenceResult != vk::Result::eSuccess)
		{
			throw std::runtime_error("failed to wait for fence!");
		}
		device.getLogical().resetFences(*drawFence);

		auto [result, imageIndex] = swapChain.getSwapChain().acquireNextImage(UINT64_MAX, *presentCompleteSemaphore, nullptr);

		recordCommandBuffer(imageIndex);

		device.getQueue().waitIdle();        // NOTE: for simplicity, wait for the queue to be idle before starting the frame
		                         // In the next chapter you see how to use multiple frames in flight and fences to sync

		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		const vk::SubmitInfo   submitInfo{.waitSemaphoreCount   = 1,
		                                  .pWaitSemaphores      = &*presentCompleteSemaphore,
		                                  .pWaitDstStageMask    = &waitDestinationStageMask,
		                                  .commandBufferCount   = 1,
		                                  .pCommandBuffers      = &*commandBuffer,
		                                  .signalSemaphoreCount = 1,
		                                  .pSignalSemaphores    = &*renderFinishedSemaphore};
		device.getQueue().submit(submitInfo, *drawFence);

		const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1, .pWaitSemaphores = &*renderFinishedSemaphore, .swapchainCount = 1, .pSwapchains = &*swapChain.getSwapChain(), .pImageIndices = &imageIndex};
		result = device.getQueue().presentKHR(presentInfoKHR);
		switch (result)
		{
			case vk::Result::eSuccess:
				break;
			case vk::Result::eSuboptimalKHR:
				std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
				break;
			default:
				break;        // an unexpected result is returned!
		}
	}


	void createInstance(){
		constexpr vk::ApplicationInfo appInfo{.pApplicationName = "Hello Triangle",
											  .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
											  .pEngineName = "No Engine",
											  .engineVersion = VK_MAKE_VERSION(1, 0, 0),
											  .apiVersion = vk::ApiVersion14
		};
	
		// For validation Layers
		std::vector<char const*> requiredLayers;
		if(enableValidationLayers){
			requiredLayers.assign(validationLayers.begin(), validationLayers.end());
		}

		auto layerProperties = context.enumerateInstanceLayerProperties();

        auto unsupportedLayerIt = std::ranges::find_if(requiredLayers, [&layerProperties](auto const &requiredLayer){
																						return std::ranges::none_of(layerProperties,
																							[requiredLayer](auto const &layerProperty){
																								return strcmp(layerProperty.layerName, requiredLayer) == 0;
																							});
		});
		if(unsupportedLayerIt != requiredLayers.end()){
			throw std::runtime_error("Required Layer not supported: " + std::string(*unsupportedLayerIt));
		}

		// Create Extensions
		auto requiredExtensions = getRequiredInstanceExtensions();

		// Check if the required extensions are supported by the Vulkan implementation.
		auto extensionProperties = context.enumerateInstanceExtensionProperties();
		auto unsupportedPropertyIt =
			std::ranges::find_if(requiredExtensions,
									[&extensionProperties](auto const &requiredExtension) {
										return std::ranges::none_of(extensionProperties,
																	[requiredExtension](auto const &extensionProperty)
																	 { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
									});

		// find_if returns a pointer to the last element if none satisfied the predicate
		// .end() points one past the last element 
		if (unsupportedPropertyIt != requiredExtensions.end())
		{
			throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
		}


		vk::InstanceCreateInfo createInfo{.pApplicationInfo        = &appInfo,
										.enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
										.ppEnabledLayerNames     = requiredLayers.data(),
										.enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
										.ppEnabledExtensionNames = requiredExtensions.data()};

		instance = vk::raii::Instance(context, createInfo);
	}

	void setupDebugMessenger(){
		if (!enableValidationLayers) return;
		vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
															vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
		vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(
				vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
		vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{.messageSeverity = severityFlags,
																			.messageType     = messageTypeFlags,
																			.pfnUserCallback = &debugCallback};
		debugMessenger = instance.createDebugUtilsMessengerEXT( debugUtilsMessengerCreateInfoEXT );
	}
	
	// For the debug messages
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT       severity,
														vk::DebugUtilsMessageTypeFlagsEXT              type,
														const vk::DebugUtilsMessengerCallbackDataEXT * pCallbackData,
														void *                                         pUserData)
	{
		// with DebuUtilsMessageSeverityFlagBitsEXT one can filter the severity of the messages so the creation of ressources don't clutter the output
		if(severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning ||
			severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
		std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

		return vk::False;
	}

	void createSurface()
	{
		VkSurfaceKHR _surface;
		if (glfwCreateWindowSurface(*instance, window.getWindow(), nullptr, &_surface) != 0)
		{
			throw std::runtime_error("failed to create window surface!");
		}
		surface = vk::raii::SurfaceKHR(instance, _surface);
	}


	void createGraphicsPipeline()
	{
		// VulkanTutorial.exe is inside the Debug folder, so first we need to exit it to get to the shaders folder
		std::cout << "current path" << std::filesystem::current_path() << std::endl;
		vk::raii::ShaderModule shaderModule = createShaderModule(readFile("build/gpu-pro26-3dgs/shaders/shader.spv"));

		// the member pSpecializationInfo allows to specify shader constants
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo{ .stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule,  .pName = "vertMain" };
		vk::PipelineShaderStageCreateInfo fragShaderStageInfo{ .stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain" };

		vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};	

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};
		// scissor rectangels define in which regions pixels will be stored
		vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};

		vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable        = vk::False, // fragments beyond near and far clipping plane are not clamped to them
                                                    .rasterizerDiscardEnable = vk::False, // if true, geometry never passes through rasterizer 
                                                    .polygonMode             = vk::PolygonMode::eFill, // fill the area of polygon with fragments (could also be set to just lines or vertices)
                                                    .cullMode                = vk::CullModeFlagBits::eBack, // type of culling 
                                                    .frontFace               = vk::FrontFace::eClockwise,
                                                    .depthBiasEnable         = vk::False, // rasterizer can not alter depth values
                                                    .lineWidth               = 1.0f};

		vk::PipelineMultisampleStateCreateInfo multisampling{.rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False};

		// contains the color configuration per attached framebuffer 
		vk::PipelineColorBlendAttachmentState colorBlendAttachment{
		.blendEnable    = vk::False,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

		// contains the global color blending settings
		vk::PipelineColorBlendStateCreateInfo colorBlending{
		.logicOpEnable = vk::False, .logicOp = vk::LogicOp::eCopy, .attachmentCount = 1, .pAttachments = &colorBlendAttachment};

		std::vector<vk::DynamicState>      dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
		vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data()};

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 0, .pushConstantRangeCount = 0};
		pipelineLayout = vk::raii::PipelineLayout(device.getLogical(), pipelineLayoutInfo);
	
		// reference each stage of the pipeline
		vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
			{.stageCount          = 2,
			.pStages             = shaderStages,
			.pVertexInputState   = &vertexInputInfo,
			.pInputAssemblyState = &inputAssembly,
			.pViewportState      = &viewportState,
			.pRasterizationState = &rasterizer,
			.pMultisampleState   = &multisampling,
			.pColorBlendState    = &colorBlending,
			.pDynamicState       = &dynamicState,
			.layout              = pipelineLayout,
			.renderPass          = nullptr}, // we are using dynamic render pass and not the traditional
			{.colorAttachmentCount = 1, .pColorAttachmentFormats = &swapChain.getSCSurfaceFormat().format}};

			// create the pipeline
			graphicsPipeline = vk::raii::Pipeline(device.getLogical(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
	}

	void createCommandPool(){
		// eResetCommandBuffer flag allows command buffers to be rerecorded individually 
		vk::CommandPoolCreateInfo poolInfo{.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                   .queueFamilyIndex = device.getQueueIndex()};

		//Each command pool can only allocate command buffers that are submitted on a single type of queue
		// the graphics queue family was chosen to record drawing commands
		commandPool = vk::raii::CommandPool(device.getLogical(), poolInfo);
	}

	void createCommandBuffer()
	{
		// ePrimary allows for submition to a queue for excecution 
		vk::CommandBufferAllocateInfo allocInfo{ .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };

		commandBuffer = std::move(vk::raii::CommandBuffers(device.getLogical(), allocInfo).front());
	}

	void recordCommandBuffer(uint32_t imageIndex)
	{
		commandBuffer.begin({});

		// Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
		transition_image_layout(
		    imageIndex,
		    vk::ImageLayout::eUndefined,
		    vk::ImageLayout::eColorAttachmentOptimal,
		    {},                                                        // srcAccessMask (no need to wait for previous operations)
		    vk::AccessFlagBits2::eColorAttachmentWrite,                // dstAccessMask
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput         // dstStage
		);
		vk::ClearValue              clearColor     = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::RenderingAttachmentInfo attachmentInfo = {
		    .imageView   = swapChain.getSCImageViews()[imageIndex],
		    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		    .loadOp      = vk::AttachmentLoadOp::eClear, // what to do with the image before rendering 
		    .storeOp     = vk::AttachmentStoreOp::eStore,// what to do with the image after rendering
		    .clearValue  = clearColor};
		vk::RenderingInfo renderingInfo = {
		    .renderArea           = {.offset = {0, 0}, .extent = swapChain.getSCExtent()},
		    .layerCount           = 1,
		    .colorAttachmentCount = 1,
		    .pColorAttachments    = &attachmentInfo};

		commandBuffer.beginRendering(renderingInfo);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
		commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChain.getSCExtent().width), static_cast<float>(swapChain.getSCExtent().height), 0.0f, 1.0f));
		commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChain.getSCExtent()));
		commandBuffer.draw(3, 1, 0, 0);
		commandBuffer.endRendering();

		// After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
		transition_image_layout(
		    imageIndex,
		    vk::ImageLayout::eColorAttachmentOptimal,
		    vk::ImageLayout::ePresentSrcKHR,
		    vk::AccessFlagBits2::eColorAttachmentWrite,                // srcAccessMask
		    {},                                                        // dstAccessMask
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
		    vk::PipelineStageFlagBits2::eBottomOfPipe                  // dstStage
		);
		commandBuffer.end();
	}

	void transition_image_layout(
	    uint32_t                imageIndex,
	    vk::ImageLayout         old_layout,
	    vk::ImageLayout         new_layout,
	    vk::AccessFlags2        src_access_mask,
	    vk::AccessFlags2        dst_access_mask,
	    vk::PipelineStageFlags2 src_stage_mask,
	    vk::PipelineStageFlags2 dst_stage_mask)
	{
		vk::ImageMemoryBarrier2 barrier = {
		    .srcStageMask        = src_stage_mask,
		    .srcAccessMask       = src_access_mask,
		    .dstStageMask        = dst_stage_mask,
		    .dstAccessMask       = dst_access_mask,
		    .oldLayout           = old_layout,
		    .newLayout           = new_layout,
		    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .image               = swapChain.getSCImages()[imageIndex],
		    .subresourceRange    = {
		           .aspectMask     = vk::ImageAspectFlagBits::eColor,
		           .baseMipLevel   = 0,
		           .levelCount     = 1,
		           .baseArrayLayer = 0,
		           .layerCount     = 1}};
		vk::DependencyInfo dependency_info = {
		    .dependencyFlags         = {},
		    .imageMemoryBarrierCount = 1,
		    .pImageMemoryBarriers    = &barrier};
		commandBuffer.pipelineBarrier2(dependency_info);
	}

	void createSyncObjects(){
		presentCompleteSemaphore = vk::raii::Semaphore(device.getLogical(), vk::SemaphoreCreateInfo());
		renderFinishedSemaphore  = vk::raii::Semaphore(device.getLogical(), vk::SemaphoreCreateInfo());
		drawFence                = vk::raii::Fence(device.getLogical(), {.flags = vk::FenceCreateFlagBits::eSignaled});
	}

	std::vector<const char*> getRequiredInstanceExtensions(){
		uint32_t glfwExtensionCount = 0;
		auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		// glfwExtensions is a const char** (pointer) so the addition is pointer arithmetic
		std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		// providing an explicit callback for handling debug messages
		if(enableValidationLayers){
			extensions.push_back(vk::EXTDebugUtilsExtensionName);
		}

		return extensions;
	}

	[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
	{
		vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char), .pCode = reinterpret_cast<const uint32_t*>(code.data()) };
		vk::raii::ShaderModule shaderModule{device.getLogical(), createInfo};
		return shaderModule;
	}

	static std::vector<char> readFile(const std::string& filename){
		// std::ios::ate allows to open the file at the end, which allows to determine the size of the file (as the pointer is at the end position)
		std::ifstream file(filename, std::ios::ate, std::ios::binary);

		if(!file.is_open()){
			throw std::runtime_error("Failed to open file");
		}

		std::vector<char> buffer(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

		file.close();
		return buffer;
	}
};

int main()
{
	try
	{
		HelloTriangleApplication app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
