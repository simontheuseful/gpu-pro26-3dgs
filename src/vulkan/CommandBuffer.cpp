#include "vulkan/CommandBuffer.hpp"

void CommandBuffer::create(Device const& device)
{
    createCommandPool(device);
    createCommandBuffer(device);
}


void CommandBuffer::createCommandPool(Device const& device){
    // eResetCommandBuffer flag allows command buffers to be rerecorded individually 
    vk::CommandPoolCreateInfo poolInfo{.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                .queueFamilyIndex = device.getQueueIndex()};

    //Each command pool can only allocate command buffers that are submitted on a single type of queue
    // the graphics queue family was chosen to record drawing commands
    commandPool = vk::raii::CommandPool(device.getLogical(), poolInfo);
}

void CommandBuffer::createCommandBuffer(Device const& device)
{
    // ePrimary allows for submition to a queue for excecution 
    vk::CommandBufferAllocateInfo allocInfo{ .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };

    commandBuffer = std::move(vk::raii::CommandBuffers(device.getLogical(), allocInfo).front());
}

void CommandBuffer::recordCommandBuffer(uint32_t imageIndex, SwapChain const& swapChain, GraphicsPipeline const& graphicsPipeline)
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
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,         // dstStage
        swapChain
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
    //commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline.getGraphicsPipeline());
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
        vk::PipelineStageFlagBits2::eBottomOfPipe,                  // dstStage
        swapChain
    );
    commandBuffer.end();
}

void CommandBuffer::transition_image_layout(
    uint32_t                imageIndex,
    vk::ImageLayout         old_layout,
    vk::ImageLayout         new_layout,
    vk::AccessFlags2        src_access_mask,
    vk::AccessFlags2        dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask,
    SwapChain const& swapChain)
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