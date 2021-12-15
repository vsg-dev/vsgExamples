#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <chrono>
#include <iostream>
#include <thread>

vsg::ref_ptr<vsg::Node> createScene(const std::string& filename, vsg::ref_ptr<vsg::Options> options)
{
    if (!filename.empty())
    {
        std::cout<<"createScene("<<filename<<", "<<options<<")"<<std::endl;
        return vsg::read_cast<vsg::Node>(filename, options);
    }

    // load shaders
    vsg::ref_ptr<vsg::ShaderStage> vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", "shaders/vert_PushConstants.spv", options);
    vsg::ref_ptr<vsg::ShaderStage> fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", "shaders/frag_PushConstants.spv", options);
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return {};
    }

    // read texture image
    vsg::Path textureFile("textures/lz.vsgb");
    auto textureData = vsg::read_cast<vsg::Data>(textureFile, options);
    if (!textureData)
    {
        std::cout << "Could not read texture file : " << textureFile << std::endl;
        return {};
    }

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}            // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

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

    auto uniformValue = vsg::vec3Value::create(1.0f, 2.0f, 3.0f);
    auto uniform = vsg::DescriptorBuffer::create(uniformValue, 1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texture, uniform});
    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->layout, 0, vsg::DescriptorSets{descriptorSet});

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSets);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT

    // add transform to root of the scene graph
    scenegraph->addChild(transform);

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
        {{-0.5f, -0.5f, 0.0f},
         {0.5f, -0.5f, 0.05f},
         {0.5f, 0.5f, 0.0f},
         {-0.5f, 0.5f, 0.0f},
         {-0.5f, -0.5f, -0.5f},
         {0.5f, -0.5f, -0.5f},
         {0.5f, 0.5f, -0.5},
         {-0.5f, 0.5f, -0.5}}); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec3Array::create(
        {
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
        }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

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

    return scenegraph;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgmultigpu";

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

    options->readOptions(arguments);

    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--FIFO")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (arguments.read("--FIFO_RELAXED")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    if (arguments.read("--MAILBOX")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
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
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    if (arguments.read({"--no-frame", "--nf"})) windowTraits->decoration = false;
    if (arguments.read("--or")) windowTraits->overrideRedirect = true;
    arguments.read("--display", windowTraits->display);
    auto numFrames = arguments.value(-1, "-f");
    auto pathFilename = arguments.value(std::string(), "-p");
    auto horizonMountainHeight = arguments.value(-1.0, "--hmh");
    auto powerWall = arguments.read({"--power-wall", "--pw"});
    auto sharedScene = !arguments.read({"--no-shared"});

    bool multiThreading = arguments.read("--mt");

    std::vector<int> screensToUse;
    int screen = -1;
    while (arguments.read({"--screen", "-s"}, screen))
    {
        screensToUse.push_back(screen);
    }

    // if now screens are assign use screen 0
    if (screensToUse.empty()) screensToUse.push_back(0);

    if (screensToUse.size() > vsg::Device::maxNumDevices())
    {
        std::cout << "VulkanSceneGraph built with VSG_MAX_DEVICES = " << VSG_MAX_DEVICES << ", "
                  << "which is insufficient to handle the number of screens desired.\n"
                  << "Please rebuild the VulkanSceneGraph with set VSG_MAX_DEVICES, via CMake, to the required size." << std::endl;
        return 1;
    }

    vsg::Affinity affinity;
    uint32_t cpu = 0;
    while (arguments.read({"--cpu", "-c"}, cpu))
    {
        affinity.cpus.insert(cpu);
    }

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    vsg::Path filename;
    if (argc > 1) filename = arguments[1];

    auto vsg_scene = createScene(filename, options);
    if (!vsg_scene)
    {
        std::cout << "Unable to load model." << std::endl;
        return 1;
    }

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // create master camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    double aspectRatio = windowTraits->fullscreen ? (1920.0 / 1080.0) : static_cast<double>(windowTraits->width) / static_cast<double>(windowTraits->height);
    if (horizonMountainHeight >= 0.0)
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, vsg::EllipsoidModel::create(), 30.0, aspectRatio, nearFarRatio, horizonMountainHeight);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, aspectRatio, nearFarRatio * radius, radius * 4.5);
    }

    auto master_camera = vsg::Camera::create(perspective, lookAt);

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    size_t numScreens = screensToUse.size();
    for (size_t i = 0; i < screensToUse.size(); ++i)
    {
        int screenNum = screensToUse[i];

        // create a local copy of the main WindowTraits
        auto local_windowTraits = vsg::WindowTraits::create(*windowTraits);

        // set the screenNum relative to the base one
        local_windowTraits->screenNum = screenNum;

        auto window = vsg::Window::create(local_windowTraits);
        if (!window)
        {
            std::cout << "Could not create window." << std::endl;
            return 1;
        }

        vsg::ref_ptr<vsg::Camera> camera;
        if (powerWall)
        {
            // assume a power wall layout
            auto relative_perspective = vsg::RelativeProjection::create(perspective, vsg::translate(double(numScreens - 1) - 2.0 * double(i), 0.0, 0.0));
            camera = vsg::Camera::create(relative_perspective, lookAt, vsg::ViewportState::create(window->extent2D()));
        }
        else
        {
            // assume monitor are rotate around the viewer
            double fovY = 30.0;
            double fovX = atan(tan(vsg::radians(fovY) * 0.5) * aspectRatio) * 2.0;
            double angle = fovX * (double(i) - double(numScreens - 1) / 2.0);
            auto relative_view = vsg::RelativeView::create(lookAt, vsg::rotate(angle, 0.0, 1.0, 0.0));
            camera = vsg::Camera::create(perspective, relative_view, vsg::ViewportState::create(window->extent2D()));
        }

        auto local_scene = sharedScene ? vsg_scene : createScene(filename, options);

        viewer->assignRecordAndSubmitTaskAndPresentation({vsg::createCommandGraphForView(window, camera, local_scene)});
        viewer->addWindow(window);
    }

    if (multiThreading)
    {
        viewer->setupThreading();

        if (affinity)
        {
            auto cpu_itr = affinity.cpus.begin();

            // set affinity of main thread
            if (cpu_itr != affinity.cpus.end())
            {
                std::cout << "vsg::setAffinity() " << *cpu_itr << std::endl;
                vsg::setAffinity(vsg::Affinity(*cpu_itr++));
            }

            for (auto& thread : viewer->threads)
            {
                if (thread.joinable() && cpu_itr != affinity.cpus.end())
                {
                    std::cout << "vsg::setAffinity(" << thread.get_id() << ") " << *cpu_itr << std::endl;
                    vsg::setAffinity(thread, vsg::Affinity(*cpu_itr++));
                }
            }
        }
    }
    else if (affinity)
    {
        vsg::setAffinity(affinity);
    }


    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    if (pathFilename.empty())
    {
        auto trackball = vsg::Trackball::create(master_camera);

        int32_t x = 0;
        int32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;

        for(auto& window : viewer->windows())
        {
            trackball->addWindow(window, vsg::ivec2(width, 0));
            width += window->extent2D().width;
            if (window->extent2D().height > height) height = window->extent2D().height;
        }

        master_camera->viewportState = vsg::ViewportState::create(x, y, width, height);

        viewer->addEventHandler(trackball);
    }
    else
    {
        auto animationPath = vsg::read_cast<vsg::AnimationPath>(pathFilename, options);
        if (!animationPath)
        {
            std::cout<<"Warning: unable to read animation path : "<<pathFilename<<std::endl;
            return 1;
        }
        auto aph = vsg::AnimationPathHandler::create(master_camera, animationPath, viewer->start_point());
        aph->printFrameStatsToConsole = true;
        viewer->addEventHandler(aph);
    }

    viewer->compile();

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
