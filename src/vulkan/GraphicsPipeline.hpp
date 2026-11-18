#pragma once 

#include <iostream>
#include <fstream>
#include <filesystem>

#include "vulkan/VulkanInclude.hpp"
#include "vulkan/Device.hpp"
#include "vulkan/SwapChain.hpp"

class GraphicsPipeline
{
    public:
        
        vk::raii::PipelineLayout const& getPipelineLayout() const { return pipelineLayout; }
        vk::raii::Pipeline const& getGraphicsPipeline() const {return  graphicsPipeline;}
        void create(Device const& device, SwapChain const& swapChain);

    private:
        vk::raii::PipelineLayout pipelineLayout = nullptr;
        vk::raii::Pipeline graphicsPipeline = nullptr;

        // [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code, Device const& device) ;
        // static std::vector<char> readFile(const std::string& filename); // move later to Utils file
};