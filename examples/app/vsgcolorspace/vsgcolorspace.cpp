#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

vsg::ref_ptr<vsg::Node> createTextureQuad(vsg::ref_ptr<vsg::Data> sourceData, vsg::ref_ptr<vsg::Options> options)
{
    auto builder = vsg::Builder::create();
    builder->options = options;

    vsg::StateInfo state;
    state.image = sourceData;
    state.lighting = false;

    vsg::GeometryInfo geom;
    geom.dy.set(0.0f, 0.0f, 1.0f);
    geom.dz.set(0.0f, -1.0f, 0.0f);

    return builder->createQuad(geom, state);
}

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // if we want to redirect std::cout and std::cerr to the vsg::Logger call vsg::Logger::redirect_stdout()
        if (arguments.read({"--redirect-std", "-r"})) vsg::Logger::instance()->redirect_std();

        // set up vsg::Options to pass in filepaths, ReaderWriters and other IO related options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->sharedObjects = vsg::SharedObjects::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());
#endif

        arguments.read(options);

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        windowTraits->synchronizationLayer = arguments.read("--sync");
        bool reportAverageFrameRate = arguments.read("--fps");
        if (arguments.read("--double-buffer")) windowTraits->swapchainPreferences.imageCount = 2;
        if (arguments.read("--triple-buffer")) windowTraits->swapchainPreferences.imageCount = 3; // default
        if (arguments.read("--IMMEDIATE")) { windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; }
        if (arguments.read("--FIFO")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        if (arguments.read("--FIFO_RELAXED")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        if (arguments.read("--MAILBOX")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        if (arguments.read({"-t", "--test"}))
        {
            windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            windowTraits->fullscreen = true;
            reportAverageFrameRate = true;
        }
        if (arguments.read({"--st", "--small-test"}))
        {
            windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            windowTraits->width = 192, windowTraits->height = 108;
            windowTraits->decoration = false;
            reportAverageFrameRate = true;
        }

        if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
        if (arguments.read({"--no-frame", "--nf"})) windowTraits->decoration = false;
        if (arguments.read("--or")) windowTraits->overrideRedirect = true;
        auto maxTime = arguments.value(std::numeric_limits<double>::max(), "--max-time");

        if (arguments.read("--d32")) windowTraits->depthFormat = VK_FORMAT_D32_SFLOAT;

        windowTraits->windowTitle = vsg::make_string("Default swapchain preferences VkSurfaceFormatKHR{ VkFormat format = ", windowTraits->swapchainPreferences.surfaceFormat.format, ", VkColorSpaceKHR colorSpace = ", windowTraits->swapchainPreferences.surfaceFormat.colorSpace,"}");

        arguments.read("--screen", windowTraits->screenNum);
        arguments.read("--display", windowTraits->display);
        arguments.read("--samples", windowTraits->samples);
        if (int log_level = 0; arguments.read("--log-level", log_level)) vsg::Logger::instance()->level = vsg::Logger::Level(log_level);
        auto numFrames = arguments.value(-1, "-f");
        auto pathFilename = arguments.value<vsg::Path>("", "-p");
        auto horizonMountainHeight = arguments.value(0.0, "--hmh");
        auto nearFarRatio = arguments.value<double>(0.001, "--nfr");
        if (arguments.read("--rgb")) options->mapRGBtoRGBAHint = false;

        bool depthClamp = arguments.read({"--dc", "--depthClamp"});
        if (depthClamp)
        {
            std::cout << "Enabled depth clamp." << std::endl;
            auto deviceFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();
            deviceFeatures->get().samplerAnisotropy = VK_TRUE;
            deviceFeatures->get().depthClamp = VK_TRUE;
        }

        if (int log_level = 0; arguments.read("--log-level", log_level)) vsg::Logger::instance()->level = vsg::Logger::Level(log_level);
        auto logFilename = arguments.value<vsg::Path>("", "--log");

        // should animations be automatically played
        auto autoPlay = !arguments.read({"--no-auto-play", "--nop"});

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        // create the basic window so can then get the various Vulkan related connections like vkSurface + vkPhysicalDevice
        auto initial_window = vsg::Window::create(windowTraits);
        if (!initial_window)
        {
            std::cout << "Could not create window." << std::endl;
            return 1;
        }

        auto physicalDevice = initial_window->getOrCreatePhysicalDevice();
        auto surface = initial_window->getOrCreateSurface();

        // https://registry.khronos.org/vulkan/specs/latest/man/html/VkFormat.html
        // https://registry.khronos.org/vulkan/specs/latest/man/html/VkColorSpaceKHR.html
        std::cout<<"SwapChain support details: "<<std::endl;
        auto swapChainSupportDetails = vsg::querySwapChainSupport(physicalDevice->vk(), surface->vk());
        for(auto& format : swapChainSupportDetails.formats)
        {
            std::cout<<"    VkSurfaceFormatKHR{ VkFormat format = "<<format.format<<", VkColorSpaceKHR colorSpace = "<<format.colorSpace<<"}"<<std::endl;
        }

        if (argc <= 1)
        {
            std::cout << "Please specify a 3d model or image file on the command line." << std::endl;
            return 1;
        }


        std::vector<vsg::ref_ptr<vsg::Window>> windows;

        windows.push_back(initial_window);

        for(auto& format : swapChainSupportDetails.formats)
        {
            if (vsg::compare_memory(windowTraits->swapchainPreferences.surfaceFormat, format) != 0)
            {
                auto local_windowTraits = vsg::WindowTraits::create();
                local_windowTraits->windowTitle = vsg::make_string("Alternate swapchain preferences VkSurfaceFormatKHR{ VkFormat format = ", format.format, ", VkColorSpaceKHR colorSpace = ", format.colorSpace,"}");
                local_windowTraits->swapchainPreferences.surfaceFormat = format;
                local_windowTraits->device = initial_window->getOrCreateDevice();

                auto local_window = vsg::Window::create(local_windowTraits);
                if (local_window) windows.push_back(local_window);
            }
        }

        std::cout<<"windows.size() = "<<windows.size()<<std::endl;

        vsg::Path filename = arguments[1];
        auto object = vsg::read(filename, options);

        vsg::ref_ptr<vsg::Node> vsg_scene;
        if (auto data = object.cast<vsg::Data>())
        {
            if (auto textureGeometry = createTextureQuad(data, options))
            {
                vsg_scene = textureGeometry;
            }
        }
        else
        {
            vsg_scene = object.cast<vsg::Node>();
        }

        if (!vsg_scene)
        {
            std::cout<<"No model loaded, please specify a model to load on command line."<<std::endl;
            return 1;
        }


        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();


        vsg::CommandGraphs commandGraphs;

        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

        // For each created window assign the each window assocaited views and trackball camera manipulator to the viewer
        for(auto& window : windows)
        {
            viewer->addWindow(window);

            // set up the camera
            auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

            vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
            auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
            if (ellipsoidModel)
            {
                perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
            }
            else
            {
                perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
            }

            auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

            auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
            commandGraphs.push_back(commandGraph);

            auto trackball = vsg::Trackball::create(camera, ellipsoidModel);
            trackball->addWindow(window);

            viewer->addEventHandler(trackball);
        }

        // add close handler to respond to the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        viewer->assignRecordAndSubmitTaskAndPresentation(commandGraphs);

        viewer->compile();

        if (autoPlay)
        {
            // find any animation groups in the loaded scene graph and play the first animation in each of the animation groups.
            auto animationGroups = vsg::visit<vsg::FindAnimations>(vsg_scene).animationGroups;
            for (auto ag : animationGroups)
            {
                if (!ag->animations.empty()) viewer->animationManager->play(ag->animations.front());
            }
        }

        viewer->start_point() = vsg::clock::now();

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0) && (viewer->getFrameStamp()->simulationTime < maxTime))
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->update();

            viewer->recordAndSubmit();

            viewer->present();
        }

        if (reportAverageFrameRate)
        {
            auto fs = viewer->getFrameStamp();
            double fps = static_cast<double>(fs->frameCount) / std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - viewer->start_point()).count();
            std::cout << "Average frame rate = " << fps << " fps" << std::endl;
        }
    }
    catch (const vsg::Exception& ve)
    {
        for (int i = 0; i < argc; ++i) std::cerr << argv[i] << " ";
        std::cerr << "\n[Exception] - " << ve.message << " result = " << ve.result << std::endl;
        return 1;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
