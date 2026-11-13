#include "vulkan/SwapChain.hpp"

void SwapChain::create(Device const& device, GLFWwindow *window, vk::raii::SurfaceKHR const& surface)
{
    vk::SurfaceCapabilitiesKHR surfaceCapabilities = device.getPhysical().getSurfaceCapabilitiesKHR( *surface );
    swapChainExtent                                = chooseSwapExtent(surfaceCapabilities, window);
    uint32_t minImageCount                         = chooseSwapMinImageCount(surfaceCapabilities);

    std::vector<vk::SurfaceFormatKHR> availableFormats = device.getPhysical().getSurfaceFormatsKHR(*surface);
    swapChainSurfaceFormat                             = chooseSwapSurfaceFormat(availableFormats);

    std::vector<vk::PresentModeKHR> availablePresentModes = device.getPhysical().getSurfacePresentModesKHR(*surface);
    vk::PresentModeKHR              presentMode           = chooseSwapPresentMode(availablePresentModes);

    vk::SwapchainCreateInfoKHR swapChainCreateInfo{.surface          = *surface,
                                            .minImageCount    = minImageCount,
                                            .imageFormat      = swapChainSurfaceFormat.format,
                                            .imageColorSpace  = swapChainSurfaceFormat.colorSpace,
                                            .imageExtent      = swapChainExtent,
                                            .imageArrayLayers = 1, // always 1 unless developing stereoscopic images
                                            .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment, // images are going to be rendered directly (no post processing)
                                            .imageSharingMode = vk::SharingMode::eExclusive, // the queue has ownership of the image and must explicitly transfer it before using in other queue family
                                            .preTransform     = surfaceCapabilities.currentTransform, // we don't want any transformation of the image
                                            .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                                            .presentMode      = chooseSwapPresentMode(availablePresentModes),
                                            .clipped          = true};
    
    swapChain = vk::raii::SwapchainKHR(device.getLogical(), swapChainCreateInfo);
    swapChainImages = swapChain.getImages();


    // create the image views
    createImageViews(device);
}


vk::SurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const &availableFormats)
{
    assert(!availableFormats.empty());

    const auto formatIt = std::ranges::find_if(
        availableFormats,
        [](const auto &format){return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;});
    // if the first test fails, then rank the other available formats 
    return formatIt != availableFormats.end()? *formatIt : availableFormats[0];
}


vk::PresentModeKHR SwapChain::chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
{
    assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
    // if the Mailbox mode is available then choose it over the FIFO
    return std::ranges::any_of(availablePresentModes,
                            [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
            vk::PresentModeKHR::eMailbox :
            vk::PresentModeKHR::eFifo;
}


vk::Extent2D SwapChain::chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities, GLFWwindow *window)
{
    // currentExtent is only set to the special "undefined" value described above
    // when the window manager lets us choose the extent ourselves; any other value
    // means the surface already dictates a fixed extent that we must use as-is.
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {
        std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
}

uint32_t SwapChain::chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
{
    auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
    if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
    {
        minImageCount = surfaceCapabilities.maxImageCount;
    }
    return minImageCount;
}

void SwapChain::createImageViews(Device const& device)
{
    assert(swapChainImageViews.empty());

    vk::ImageViewCreateInfo imageViewCreateInfo{ .viewType         = vk::ImageViewType::e2D,
                                                .format           = swapChainSurfaceFormat.format,
                                                .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } };

    // this allows to change the color channels, but now they are set in default mode	
    imageViewCreateInfo.components = {
        vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity};

    // images are color target without any mipmap levels
    // levelCount varies in the rendering of VR as it commonly uses 4 rendered images 
    imageViewCreateInfo.subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1};

    for (auto &image : swapChainImages)
    {
        imageViewCreateInfo.image = image;
        swapChainImageViews.emplace_back(device.getLogical(), imageViewCreateInfo );
    }
}