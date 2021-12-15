#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

// Example that uses gl_ClipDistancep[] to do GPU clipping of primitives
// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_ClipDistance.xhtml
// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_CullDistance.xhtml

class ReplaceState : public vsg::Inherit<vsg::Visitor, ReplaceState>
{
public:
    vsg::ref_ptr<vsg::GraphicsPipeline> graphicsPipeline;

    ReplaceState(vsg::ref_ptr<vsg::GraphicsPipeline> gp) :
        graphicsPipeline(gp) {}

    void apply(vsg::Object& object) override
    {
        object.traverse(*this);
    }

    void apply(vsg::StateGroup& sg) override
    {
        for (auto& sc : sg.stateCommands) sc->accept(*this);

        sg.traverse(*this);
    }

    void apply(vsg::BindGraphicsPipeline& bgp) override
    {
        bgp.pipeline = graphicsPipeline;
    }
};

class IntersectionHandler : public vsg::Inherit<vsg::Visitor, IntersectionHandler>
{
public:
    vsg::ref_ptr<vsg::Camera> camera;
    vsg::ref_ptr<vsg::Node> scenegraph;
    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel;
    vsg::ref_ptr<vsg::vec4Array> world_ClipSetttings;
    double scale;

    IntersectionHandler(vsg::ref_ptr<vsg::Camera> in_camera, vsg::ref_ptr<vsg::Node> in_scenegraph, vsg::ref_ptr<vsg::EllipsoidModel> in_ellipsoidModel, double in_scale) :
        camera(in_camera),
        scenegraph(in_scenegraph),
        ellipsoidModel(in_ellipsoidModel),
        scale(in_scale)
    {
    }

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (keyPress.keyBase == 'i' && lastPointerEvent)
        {
            interesection(*lastPointerEvent);
        }
    }

    void apply(vsg::PointerEvent& pointerEvent) override
    {
        lastPointerEvent = &pointerEvent;
    }

    void apply(vsg::ScrollWheelEvent& scrollWheel) override
    {
        scrollWheel.handled = true;
        scale *= (1.0 + scrollWheel.delta.y * 0.1);
        world_ClipSetttings->at(0).w = scale;
        world_ClipSetttings->dirty();
    }

    void interesection(vsg::PointerEvent& pointerEvent)
    {
        auto intersector = vsg::LineSegmentIntersector::create(*camera, pointerEvent.x, pointerEvent.y);
        scenegraph->accept(*intersector);

        if (intersector->intersections.empty()) return;

        // sort the intersectors front to back
        std::sort(intersector->intersections.begin(), intersector->intersections.end(), [](auto lhs, auto rhs) { return lhs.ratio < rhs.ratio; });

        auto& intersection = intersector->intersections.front();
        world_ClipSetttings->at(0) = vsg::vec4(intersection.worldIntersection.x, intersection.worldIntersection.y, intersection.worldIntersection.z, scale);
        world_ClipSetttings->dirty();
    }

protected:
    vsg::ref_ptr<vsg::PointerEvent> lastPointerEvent;
};

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // set up vsg::Options to pass in filepaths and ReaderWriter's and other IO related options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());
#endif

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "vsgclip";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if (arguments.read("--double-buffer")) windowTraits->swapchainPreferences.imageCount = 2;
        if (arguments.read("--triple-buffer")) windowTraits->swapchainPreferences.imageCount = 3; // default
        if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
        arguments.read("--screen", windowTraits->screenNum);
        if (arguments.read({"-t", "--test"}))
        {
            windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            windowTraits->fullscreen = true;
        }
        if (arguments.read({"--st", "--small-test"}))
        {
            windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            windowTraits->width = 192, windowTraits->height = 108;
            windowTraits->decoration = false;
        }
        auto outputFilename = arguments.value<vsg::Path>("", "-o");

        auto numFrames = arguments.value(-1, "-f");

        bool useStagingBuffer = !arguments.read({"--no-staging-buffer", "-n"});
        size_t frameToWait = useStagingBuffer ? 2 : 1;
        arguments.read({"--frames-to-wait", "--f2w"}, frameToWait);
        auto waitTimeout = arguments.value<uint64_t>(50000000, {"--timeout", "--to"});

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        if (argc <= 1)
        {
            std::cout << "Please specify a 3d model on the command line." << std::endl;
            return 1;
        }

        vsg::Path filename = arguments[1];
        auto model = vsg::read_cast<vsg::Node>(filename, options);
        if (!model)
        {
            std::cout << "Unable to load file " << filename << std::endl;
            return 1;
        }

        // load shaders
        vsg::Path vertexShaderFilename("shaders/clip.vert");
        vsg::Path fragmentShaderFilename("shaders/clip.frag");

        auto vertexShader = vsg::read_cast<vsg::ShaderStage>(vertexShaderFilename, options);
        auto fragmentShader = vsg::read_cast<vsg::ShaderStage>(fragmentShaderFilename, options);

        if (!vertexShader || !fragmentShader)
        {
            std::cout << "Could not read shader files " << vertexShaderFilename << " and/or " << fragmentShaderFilename << std::endl;
            std::cout << "Please set VSG_FILE_PATH environmental variable to your vsgExamples/data directory." << std::endl;
            return 1;
        }

        std::cout << "windowTraits->swapchainPreferences.imageCount = " << windowTraits->swapchainPreferences.imageCount << std::endl;

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        // set up features
        auto requestFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();

        requestFeatures->get().samplerAnisotropy = VK_TRUE;
        requestFeatures->get().shaderClipDistance = VK_TRUE;

        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }

        viewer->addWindow(window);

        // get the Window to create the Instance & PhysicalDevice for us.
        auto& availableFeatures = window->getOrCreatePhysicalDevice()->getFeatures(); // VkPhysicalDeviceFeatures
        auto& limits = window->getOrCreatePhysicalDevice()->getProperties().limits;   // VkPhysicalDeviceLimits

        std::cout << "availableFeatures.samplerAnisotropy = " << availableFeatures.samplerAnisotropy << ", limits.maxSamplerAnisotropy = " << limits.maxSamplerAnisotropy << std::endl;
        std::cout << "availableFeatures.shaderClipDistance = " << availableFeatures.shaderClipDistance << ", limits.maxClipDistances = " << limits.maxClipDistances << std::endl;
        std::cout << "availableFeatures.shaderCullDistance = " << availableFeatures.shaderCullDistance << ", limits.maxCullDistances = " << limits.maxCullDistances << std::endl;
        std::cout << "limits.maxCombinedClipAndCullDistances = " << limits.maxCombinedClipAndCullDistances << std::endl;

        if (!availableFeatures.samplerAnisotropy || !availableFeatures.shaderClipDistance)
        {
            std::cout << "Required features not supported." << std::endl;
            return 1;
        }

        auto shaderStages = vsg::ShaderStages{vertexShader, fragmentShader};

        // set up graphics pipeline
        vsg::DescriptorSetLayoutBindings baseTexture_descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
        };

        auto baseTexture_descriptorSetLayout = vsg::DescriptorSetLayout::create(baseTexture_descriptorBindings);

        vsg::DescriptorSetLayoutBindings clipSettings_descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}, // ClipSettings uniform
        };

        auto clipSettings_descriptorSetLayout = vsg::DescriptorSetLayout::create(clipSettings_descriptorBindings);

        vsg::PushConstantRanges pushConstantRanges{
            {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls automatically provided by the VSG's DispatchTraversal
        };

        vsg::VertexInputState::Bindings vertexBindingsDescriptions{
            VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
            VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
            VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
        };

        vsg::VertexInputState::Attributes vertexAttributeDescriptions{
            VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
            VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
            VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0}     // tex coord data
        };

        vsg::GraphicsPipelineStates pipelineStates{
            vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
            vsg::InputAssemblyState::create(),
            vsg::RasterizationState::create(),
            vsg::MultisampleState::create(),
            vsg::ColorBlendState::create(),
            vsg::DepthStencilState::create()};

        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{baseTexture_descriptorSetLayout, clipSettings_descriptorSetLayout}, pushConstantRanges);
        auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);

        // replace the GraphicsPipeline in the loaded scene graph with the one created for clipping.
        auto replaceState = ReplaceState::create(graphicsPipeline);
        model->accept(*replaceState);

        auto worldClipSettings = vsg::vec4Array::create(1);
        worldClipSettings->set(0, vsg::vec4(0.0, 0.0, 0.0, 10.0));

        auto eyeClipSettings = vsg::vec4Array::create(1);
        eyeClipSettings->set(0, vsg::vec4(0.0, 0.0, 0.0, 0.0));

        auto device = window->getOrCreateDevice();

        vsg::ref_ptr<vsg::CopyAndReleaseBuffer> copyBufferCmd;
        vsg::ref_ptr<vsg::DescriptorBuffer> clipSettings_buffer;
        auto bufferInfo = vsg::BufferInfo::create();

        if (useStagingBuffer)
        {
            std::cout << "Using Staging Buffer DescriptorBuffer" << std::endl;
            auto memoryBufferPools = vsg::MemoryBufferPools::create("Staging_MemoryBufferPool", device);
            copyBufferCmd = vsg::CopyAndReleaseBuffer::create(memoryBufferPools);

            // allocate output storage buffer
            VkDeviceSize bufferSize = sizeof(vsg::vec4) * 1;
            auto buffer = vsg::createBufferAndMemory(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            bufferInfo->buffer = buffer;
            bufferInfo->offset = 0;
            bufferInfo->range = bufferSize;

            clipSettings_buffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{bufferInfo}, 0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        }
        else
        {
            std::cout << "Using HOST_VISIBLE DescriptorBuffer" << std::endl;
            clipSettings_buffer = vsg::DescriptorBuffer::create(eyeClipSettings, 0);
        }

        auto clipSettings_descriptorSet = vsg::DescriptorSet::create(clipSettings_descriptorSetLayout, vsg::Descriptors{clipSettings_buffer});
        auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, clipSettings_descriptorSet);
        bindDescriptorSet->slot = 2;

        auto vsg_scene = vsg::StateGroup::create();
        vsg_scene->add(bindDescriptorSet);
        vsg_scene->addChild(model);

        if (!outputFilename.empty())
        {
            vsg::write(vsg_scene, outputFilename);
            return 1;
        }

        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
        double nearFarRatio = 0.001;

        // set up the camera
        auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel(model->getObject<vsg::EllipsoidModel>("EllipsoidModel"));
        if (ellipsoidModel)
        {
            perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, 0.0);
        }
        else
        {
            perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        // add intersection handler to track the mouse position
        auto interesectionHandler = IntersectionHandler::create(camera, vsg_scene, ellipsoidModel, radius * 0.3);
        viewer->addEventHandler(interesectionHandler);
        interesectionHandler->world_ClipSetttings = worldClipSettings;

        // add trackball to control the Camera
        viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

        auto renderGraph = vsg::createRenderGraphForView(window, camera, vsg_scene);

        // set up commandGraph for rendering
        auto commandGraph = vsg::CommandGraph::create(window);

        if (useStagingBuffer)
        {
            commandGraph->addChild(copyBufferCmd);
            commandGraph->addChild(renderGraph);
        }
        else
        {
            commandGraph->addChild(renderGraph);
        }

        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        viewer->compile();

        worldClipSettings->set(0, vsg::vec4(centre.x, centre.y, centre.z, radius * 0.3));

        auto startTime = std::chrono::steady_clock::now();
        double frameCount = 0.0;

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
        {
            ++frameCount;

            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->update();

            for (size_t i = 0; i < worldClipSettings->size(); ++i)
            {
                auto& world_sphere = worldClipSettings->at(i);
                auto& eye_sphere = eyeClipSettings->at(i);

                vsg::dmat4 viewMatrix = camera->viewMatrix->transform();

                vsg::dvec3 world_center(world_sphere.x, world_sphere.y, world_sphere.z);
                vsg::dvec3 eye_center = viewMatrix * world_center;

                eye_sphere.set(eye_center.x, eye_center.y, eye_center.z, world_sphere.w);
            }

            if (frameToWait > 0 && waitTimeout > 0)
            {
                viewer->waitForFences(frameToWait, waitTimeout);
            }

            if (useStagingBuffer)
            {
                // copy data to staging buffer and issue a copy command to transfer to the GPU texture image
                copyBufferCmd->copy(eyeClipSettings, bufferInfo);
            }
            else
            {
                clipSettings_buffer->copyDataListToBuffers();
            }

            viewer->recordAndSubmit();
            viewer->present();
        }

        auto fps = frameCount / (std::chrono::duration<double, std::chrono::seconds::period>(std::chrono::steady_clock::now() - startTime).count());
        std::cout << "Average fps = " << fps << std::endl;
    }
    catch (const vsg::Exception& exception)
    {
        std::cout << exception.message << " VkResult = " << exception.result << std::endl;
        return 0;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
