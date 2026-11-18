#pragma once 
#include "vulkan/VulkanInclude.hpp"
#include "vulkan/Device.hpp"
#include "vulkan/SwapChain.hpp"
#include "vulkan/GraphicsPipeline.hpp"

class CommandBuffer
{
    public: 
        vk::raii::CommandPool const& getCommandPool() const { return commandPool; }
        vk::raii::CommandBuffer const& getCommandBuffer() const { return commandBuffer; }

        void create(Device const& device);
        void recordCommandBuffer(uint32_t imageIndex, SwapChain const& swapChain, GraphicsPipeline const& graphicsPipeline);

    private:   
        vk::raii::CommandPool commandPool = nullptr;
        vk::raii::CommandBuffer commandBuffer = nullptr;
    
        void createCommandPool(Device const& device);
        void createCommandBuffer(Device const& device);
        void transition_image_layout(
            uint32_t                imageIndex,
            vk::ImageLayout         old_layout,
            vk::ImageLayout         new_layout,
            vk::AccessFlags2        src_access_mask,
            vk::AccessFlags2        dst_access_mask,
            vk::PipelineStageFlags2 src_stage_mask,
            vk::PipelineStageFlags2 dst_stage_mask,
            SwapChain const& swapChain);
};