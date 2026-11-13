#include "vulkan/Device.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>


void Device::create(vk::raii::Instance const& instance, vk::raii::SurfaceKHR const& surface)
{
	pickPhysicalDevice(instance);
	createLogicalDevice(surface);
}

// the later const means that calling this method won't alter the object (Device) 
bool Device::isDeviceSuitable(vk::raii::PhysicalDevice const& candidate) const
{
	// Check if the physicalDevice supports the Vulkan 1.3 API version
	bool supportsVulkan1_3 = candidate.getProperties().apiVersion >= vk::ApiVersion13;

	// Check if any of the queue families support graphics operations
	auto queueFamilies    = candidate.getQueueFamilyProperties();
	// why the double !! are required
	// the return type of the opperation qfp.queueFlags & vk::QueueFlagBits::eGraphics is vk::QueueFlags
	// the conversion to bool has to be made explicit using ! (and the second ! is used to convert it back to the correct bool output)
	bool supportsGraphics = std::ranges::any_of( queueFamilies, []( auto const & qfp ) { return !!( qfp.queueFlags & vk::QueueFlagBits::eGraphics ); } );

	// Check if all required physicalDevice extensions are available
	auto availableDeviceExtensions = candidate.enumerateDeviceExtensionProperties();
	bool supportsAllRequiredExtensions =
		std::ranges::all_of( requiredDeviceExtension,
							[&availableDeviceExtensions]( auto const & requiredDeviceExtension )
							{
							return std::ranges::any_of( availableDeviceExtensions,
														[requiredDeviceExtension]( auto const & availableDeviceExtension )
														{ return strcmp( availableDeviceExtension.extensionName, requiredDeviceExtension ) == 0; } );
							} );

	// Check if the physicalDevice supports the required features (shader draw parameters, dynamic rendering and extended dynamic state)
	auto features                 = candidate.template getFeatures2<vk::PhysicalDeviceFeatures2,
																		vk::PhysicalDeviceVulkan11Features,
																		vk::PhysicalDeviceVulkan13Features,
																		vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
	bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
									features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 && // the order of declaring features is important!! 
									// in PhysicalDeviceVulkan13Features synchronization2 is declared before dynamicRendering, so the order must follow
									features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
									features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

	// Return true if the physicalDevice meets all the criteria
	return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
}

void Device::pickPhysicalDevice(vk::raii::Instance const& instance)
{
	std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
	auto const devIter = std::ranges::find_if( physicalDevices, [&]( auto const & physicalDevice ) { return isDeviceSuitable( physicalDevice ); } );
	if ( devIter == physicalDevices.end() )
	{
		throw std::runtime_error( "failed to find a suitable GPU!" );
	}
	// physicalDevice is found (store it in the member of the class)
	physicalDevice = *devIter;
}


void Device::createLogicalDevice(vk::raii::SurfaceKHR const& surface)
{
	// find the index of the first queue family that supports graphics
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

	// ~ is a bit negation opperation, so all bits in 0 will be flipped to 0xFFFFFFFF 
	// find the first index of the queue family that supports graphics and present a windows surface
	for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
	{
		if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
			physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
			{
				// found a queue family that supports both graphics and present
				queueIndex = qfpIndex;
				break;
			}
	}
	if (queueIndex == ~0)
	{
		throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
	}

	// query for Vulkan 1.3 features
	// chain of features connected automatically by Vulkan using pNext
	vk::StructureChain<vk::PhysicalDeviceFeatures2,
					vk::PhysicalDeviceVulkan11Features,
					vk::PhysicalDeviceVulkan13Features,
					vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
		featureChain = {
			{},                                    // vk::PhysicalDeviceFeatures2
			{.shaderDrawParameters = true},        // vk::PhysicalDeviceVulkan11Features
			{.synchronization2 = true, .dynamicRendering = true},            // vk::PhysicalDeviceVulkan13Features (the order also relevant here)
			{.extendedDynamicState = true}         // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
		};

	// create a Device
	float                     queuePriority = 0.5f;
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};
	vk::DeviceCreateInfo      deviceCreateInfo{.pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
											.queueCreateInfoCount    = 1,
											.pQueueCreateInfos       = &deviceQueueCreateInfo,
											.enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size()),
											.ppEnabledExtensionNames = requiredDeviceExtension.data()};

	device = vk::raii::Device(physicalDevice, deviceCreateInfo);
	queue = vk::raii::Queue(device, queueIndex, 0);

	// only query for surface support after that extension was verified
	// gets the specifications for the surface (min/max number of images and their min/max height and width)
	auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
	// quering surface formats (pixel format, color space)
	std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR( *surface );
	// quering available presentation models
	std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR( *surface );

}