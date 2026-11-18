#include "vulkan/VulkanInclude.hpp"
#include "vulkan/Device.hpp"
#include "vulkan/SwapChain.hpp"
#include "vulkan/GraphicsPipeline.hpp"
#include "vulkan/CommandBuffer.hpp"

#include <memory>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <map>
#include <limits>
#include <algorithm>


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
	Window window;
	vk::raii::Context context;
	vk::raii::Instance instance = nullptr;	
	vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
	vk::raii::SurfaceKHR surface = nullptr;

	// order of declaration matters as the members will be destroyed in the opposite order 
	// and swapchain, buffers before the device
	Device device; 
	SwapChain swapChain;
	GraphicsPipeline graphicsPipeline;
	CommandBuffer commandBuffer;

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
		graphicsPipeline.create(device, swapChain);
		commandBuffer.create(device);
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

		commandBuffer.recordCommandBuffer(imageIndex, swapChain, graphicsPipeline);

		device.getQueue().waitIdle();        // NOTE: for simplicity, wait for the queue to be idle before starting the frame
		                         // In the next chapter you see how to use multiple frames in flight and fences to sync

		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		const vk::SubmitInfo   submitInfo{.waitSemaphoreCount   = 1,
		                                  .pWaitSemaphores      = &*presentCompleteSemaphore,
		                                  .pWaitDstStageMask    = &waitDestinationStageMask,
		                                  .commandBufferCount   = 1,
		                                  .pCommandBuffers      = &*commandBuffer.getCommandBuffer(),
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
