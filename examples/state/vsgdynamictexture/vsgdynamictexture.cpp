#include <iostream>
#include <vsg/all.h>


class UpdateImage : public vsg::Visitor
{
public:

    double value = 0.0;

    template<class A>
    void update(A& image)
    {
        using value_type = typename A::value_type;
        for (size_t r = 0; r < image.height(); ++r)
        {
            float r_ratio = static_cast<float>(r) / static_cast<float>(image.height() - 1);
            for (size_t c = 0; c < image.width(); ++c)
            {
                float c_ratio = static_cast<float>(c) / static_cast<float>(image.width() - 1);

                vsg::vec2 delta((r_ratio - 0.5f), (c_ratio - 0.5f));

                float angle = atan2(delta.x, delta.y);

                float distance_from_center = vsg::length(delta);

                float intensity = (sin(1.0 * angle + 30.0f * distance_from_center + 10.0 * value) + 1.0f) * 0.5f;

                auto& value = image.at(c, r);
                value.r = intensity * intensity;
                value.g = intensity;
                value.b = intensity;

                if constexpr (std::is_same_v<value_type, vsg::vec4>) value.a = 1.0f;
            }
        }
    }

    // use the vsg::Visitor to safely cast to types handled by the UpdateImage class
    void apply(vsg::vec3Array2D& image) override { update(image); }
    void apply(vsg::vec4Array2D& image) override { update(image); }

    // provide convinient way to invoke the UpdateImage as a functor
    void operator() (vsg::Data* image, double v) { value = v; image->accept(*this); }
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--double-buffer")) windowTraits->swapchainPreferences.imageCount = 2;
    if (arguments.read("--triple-buffer")) windowTraits->swapchainPreferences.imageCount = 3; // default
    arguments.read("--window", windowTraits->width, windowTraits->height);
    auto numFrames = arguments.value(-1, "-f");
    auto workgroupSize = arguments.value(32, "-w");
    bool useRGB = arguments.read("--rgb");
    bool useCompute = useRGB ? arguments.read("--compute") : false;

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // load shaders
    vsg::ref_ptr<vsg::ShaderStage> vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    vsg::ref_ptr<vsg::ShaderStage> fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return 1;
    }

    // setup texture image
    vsg::ref_ptr<vsg::Data> textureData;

    if (useRGB)
    {
        textureData = vsg::vec3Array2D::create(256, 256);
        textureData->getLayout().format = VK_FORMAT_R32G32B32_SFLOAT;

        if (useCompute) windowTraits->queueFlags |= VK_QUEUE_COMPUTE_BIT;
    }
    else
    {
        textureData = vsg::vec4Array2D::create(256, 256);
        textureData->getLayout().format = VK_FORMAT_R32G32B32A32_SFLOAT;
    }

    // initialize the image
    UpdateImage updateImage;
    updateImage(textureData, 0.0);

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
    };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // tex coord data
    };

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create()};

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    // create texture image and associated DescriptorSets and binding
    auto texture = vsg::DescriptorImage::create(vsg::Sampler::create(), textureData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texture});
    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->layout, 0, descriptorSet);

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSet);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT

    // add transform to root of the scene graph
    scenegraph->addChild(transform);

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
        {{-0.5f, -0.5f, 0.0f},
         {0.5f, -0.5f, 0.0f},
         {0.5f, 0.5f, 0.0f},
         {-0.5f, 0.5f, 0.0f},
         {-0.5f, -0.5f, -0.5f},
         {0.5f, -0.5f, -0.5f},
         {0.5f, 0.5f, -0.5f},
         {-0.5f, 0.5f, -0.5f}}); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec3Array::create(vertices->size(), vsg::vec3(1.0f, 1.0f, 1.0f));

    auto texcoords = vsg::vec2Array::create(
        {{0.0f, 0.0f},
         {1.0f, 0.0f},
         {1.0f, 1.0f},
         {0.0f, 1.0f},
         {0.0f, 0.0f},
         {1.0f, 0.0f},
         {1.0f, 1.0f},
         {0.0f, 1.0f}}); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
         2, 3, 0,
         4, 5, 6,
         6, 7, 4}); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, colors, texcoords}));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(12, 1, 0, 0, 0));

    // add drawCommands to transform
    transform->addChild(drawCommands);

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // camera related details
    auto viewport = vsg::ViewportState::create(window->extent2D());
    auto perspective = vsg::Perspective::create(60.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), 0.1, 10.0);
    auto lookAt = vsg::LookAt::create(vsg::dvec3(1.0, 1.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    auto copyImageCmd = vsg::CopyAndReleaseImage::create();
    copyImageCmd->stagingMemoryBufferPools = vsg::MemoryBufferPools::create("Staging_MemoryBufferPool", window->getOrCreateDevice(), vsg::BufferPreferences{});

    auto copyBufferCmd = vsg::CopyAndReleaseBuffer::create();
    //copyBufferCmd->stagingMemoryBufferPools = vsg::MemoryBufferPools::create("Staging_MemoryBufferPool", window->getOrCreateDevice(), vsg::BufferPreferences{});

    if (useCompute)
    {
        std::cout<<"Using compute path"<<std::endl;

        auto physicalDevice = window->getOrCreatePhysicalDevice();
        auto device = window->getOrCreateDevice();

        vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");
        auto computeStage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", vsg::findFile("shaders/RGBtoRGBA.spv", searchPaths));
        if (!computeStage)
        {
            std::cout << "Error : No shader loaded." << std::endl;
            return 1;
        }

        auto width = textureData->width();
        auto height = textureData->height();

        computeStage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
            {0, vsg::intValue::create(width)},
            {1, vsg::intValue::create(height)},
            {2, vsg::intValue::create(workgroupSize)}};

        // binding 0 source buffer
        // binding 1 image2d

        // allocate output storage buffer
        VkDeviceSize bufferSize = sizeof(vsg::vec4) * width * height;

        auto buffer = vsg::createBufferAndMemory(device, bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        auto bufferMemory = buffer->getDeviceMemory(device->deviceID);

        // set up DescriptorSetLayout, DecriptorSet and BindDescriptorSets
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
        };
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

        vsg::Descriptors descriptors{
            vsg::DescriptorBuffer::create(vsg::BufferInfoList{vsg::BufferInfo(buffer, 0, bufferSize)}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
            texture // need to be a VK_DESCRIPTOR_TYPE_STORAGE_IMAGE rather than VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER as defined above,
        };
        auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, descriptors);
        auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);

        // set up the compute pipeline
        auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeStage);
        auto bindPipeline = vsg::BindComputePipeline::create(pipeline);

        int computeQueueFamily = physicalDevice->getQueueFamily(VK_QUEUE_COMPUTE_BIT);
        auto compute_commandGraph = vsg::CommandGraph::create(device, computeQueueFamily);
        compute_commandGraph->addChild(copyBufferCmd);
        compute_commandGraph->addChild(bindPipeline);
        compute_commandGraph->addChild(bindDescriptorSet);
        compute_commandGraph->addChild(vsg::Dispatch::create(uint32_t(ceil(float(width) / float(workgroupSize))), uint32_t(ceil(float(height) / float(workgroupSize))), 1));


        auto grahics_commandGraph = vsg::createCommandGraphForView(window, camera, {copyBufferCmd, scenegraph});

        viewer->assignRecordAndSubmitTaskAndPresentation({compute_commandGraph, grahics_commandGraph});
    }
    else
    {
        auto grahics_commandGraph = vsg::createCommandGraphForView(window, camera, {copyImageCmd, scenegraph});

        //grahics_commandGraph->getChildren().insert(grahics_commandGraph->getChildren().begin(), copyCmd);

        viewer->assignRecordAndSubmitTaskAndPresentation({grahics_commandGraph});
    }

    // add event handlers
    viewer->addEventHandler(vsg::Trackball::create(camera));
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    // compile the Vulkan objects
    viewer->compile();

    // texture has been filled in so it's now safe to get the ImageInfo that holds the handles to the texture's
    vsg::ImageInfo textureImageInfo;
    if (!texture->imageInfoList.empty()) textureImageInfo = texture->imageInfoList[0]; // contextID=0, and only one imageData

    auto startTime = vsg::clock::now();
    double numFramesCompleted = 0.0;

    // main frame loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        double time = std::chrono::duration<double, std::chrono::seconds::period>(viewer->getFrameStamp()->time - viewer->start_point()).count();

        // update texture data
        updateImage(textureData, time);

        // copy data to staging buffer and isse a copy command to transfer to the GPU texture image
        copyImageCmd->copy(textureData, textureImageInfo);

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();

        numFramesCompleted += 1.0;
    }

    auto duration = std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - startTime).count();
    if (numFramesCompleted > 0.0)
    {
        std::cout<<"Average frame rate = "<<(numFramesCompleted / duration)<<std::endl;
    }




    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
