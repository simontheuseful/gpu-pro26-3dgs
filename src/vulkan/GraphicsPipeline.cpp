#include "vulkan/GraphicsPipeline.hpp"


[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code, Device const& device) 
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

void GraphicsPipeline::create(Device const& device, SwapChain const& swapChain)
{
    // VulkanTutorial.exe is inside the Debug folder, so first we need to exit it to get to the shaders folder
    std::cout << "current path" << std::filesystem::current_path() << std::endl;
    vk::raii::ShaderModule shaderModule = createShaderModule(readFile("build/gpu-pro26-3dgs/shaders/shader.spv"), device);

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

