#pragma once
#include "vulkan/VulkanInclude.hpp"
#include "vulkan/Window.hpp"
#include "vulkan/Device.hpp"

class SwapChain
{
    public:
        vk::raii::SwapchainKHR const& getSwapChain() const { return swapChain; }
        std::vector<vk::Image> const& getSCImages() const { return swapChainImages; }
        vk::SurfaceFormatKHR const& getSCSurfaceFormat() const { return swapChainSurfaceFormat; }
        vk::Extent2D const& getSCExtent() const { return swapChainExtent; }
        std::vector<vk::raii::ImageView>&  getSCImageViews() { return swapChainImageViews; } // write access 
        std::vector<vk::raii::ImageView> const&  getSCImageViews() const { return swapChainImageViews; } // only read access

        void create(Device const& device, GLFWwindow *window, vk::raii::SurfaceKHR const& surface);

    private:
        vk::raii::SwapchainKHR swapChain = nullptr;
        std::vector<vk::Image> swapChainImages;
        vk::SurfaceFormatKHR   swapChainSurfaceFormat;
        vk::Extent2D           swapChainExtent;
        std::vector<vk::raii::ImageView> swapChainImageViews;

        vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats);
        vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes);
        vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities, GLFWwindow *window);
        uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities);
        void createImageViews(Device const& device); 
    };