#pragma once

#include "vulkan/VulkanInclude.hpp"
#include <vector>

class Device
{
    public:
        // getters
        // const& functions as the return type, so the callers get direct access to the members and not a copy
        // the second "const" belongs to the function itself (that the body does not change the Device)
        uint32_t getQueueIndex() const { return queueIndex;}
        vk::raii::Device const& getLogical() const { return device; }
        vk::raii::PhysicalDevice const& getPhysical() const {return physicalDevice;}
        vk::raii::Queue const& getQueue() const { return queue; }

        void create(vk::raii::Instance const& instance, vk::raii::SurfaceKHR const& surface);

    private:

        uint32_t         queueIndex = ~0; // ~ represents the negative, and as a uint, it wraps to the max uint value
        vk::raii::Queue  queue = nullptr;
        vk::raii::Device device = nullptr;
        vk::raii::PhysicalDevice physicalDevice = nullptr;
        std::vector<const char*> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

        bool isDeviceSuitable(vk::raii::PhysicalDevice const& candidate) const;
        void pickPhysicalDevice(vk::raii::Instance const& instance);
        void createLogicalDevice(vk::raii::SurfaceKHR const& surface);
};