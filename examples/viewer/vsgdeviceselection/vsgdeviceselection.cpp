#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

int main(int argc, char** argv)
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
    arguments.read(options);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgdeviceslection";
    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    if (arguments.read("--list"))
    {
        // use the Window implementation to create the device and surface
        auto instance = window->getOrCreateInstance();
        auto surface = window->getOrCreateSurface();

        auto physicalDevices = instance->getPhysicalDevices();

        std::cout << "\nphysicalDevices.size() = " << physicalDevices.size() << std::endl;
        for (auto& physicalDevice : physicalDevices)
        {
            auto properties = physicalDevice->getProperties();

            auto [graphicsFamily, presentFamily] = physicalDevice->getQueueFamily(windowTraits->queueFlags, surface);
            if (graphicsFamily >= 0 && presentFamily >= 0)
                std::cout << "    matched " << physicalDevice << " " << properties.deviceName << " deviceType = " << properties.deviceType << std::endl;
            else
                std::cout << "    not matched " << physicalDevice << " " << properties.deviceName << " deviceType = " << properties.deviceType << std::endl;
        }
        return 0;
    }

    if (arguments.read({"--PhysicalDevice", "--pd"}))
    {
        // use the Window implementation to create the Instance and Surface
        auto instance = window->getOrCreateInstance();
        auto surface = window->getOrCreateSurface();

        // create a vk/vsg::PhysicalDevice, prefer discrete GPU over integrated GPUs when they area available.
        auto physicalDevice = instance->getPhysicalDevice(windowTraits->queueFlags, surface, {VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU});

        std::cout << "Created our own vsg::PhysicalDevice " << physicalDevice << std::endl;

        window->setPhysicalDevice(physicalDevice);
    }
    else if (arguments.read({"--Device", "--device"}))
    {
        // use the Window implementation to create the Instance and Surface
        auto instance = window->getOrCreateInstance();
        auto surface = window->getOrCreateSurface();

        vsg::Names requestedLayers;
        if (windowTraits->debugLayer)
        {
            requestedLayers.push_back("VK_LAYER_LUNARG_standard_validation");
            if (windowTraits->apiDumpLayer) requestedLayers.push_back("VK_LAYER_LUNARG_api_dump");
        }

        vsg::Names validatedNames = vsg::validateInstancelayerNames(requestedLayers);

        vsg::Names deviceExtensions;
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        deviceExtensions.insert(deviceExtensions.end(), windowTraits->deviceExtensionNames.begin(), windowTraits->deviceExtensionNames.end());

        // set up vk/vsg::Device
        auto [physicalDevice, queueFamily, presentFamily] = instance->getPhysicalDeviceAndQueueFamily(windowTraits->queueFlags, surface);
        if (!physicalDevice || queueFamily < 0 || presentFamily < 0)
        {
            std::cout << "Error: vsg::Window::create(...) failed to create Window, no Vulkan PhysicalDevice supported." << VK_ERROR_INVALID_EXTERNAL_HANDLE << std::endl;
            return 1;
        }

        vsg::QueueSettings queueSettings{vsg::QueueSetting{queueFamily, {1.0}}, vsg::QueueSetting{presentFamily, {1.0}}};
        auto device = vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions, windowTraits->deviceFeatures, instance->getAllocationCallbacks());

        std::cout << "Created our own vsg::Device " << device << std::endl;

        window->setDevice(device);
    }
    else
    {
        std::cout << "Using vsg::Window default behavior to create the required vsg::Device." << std::endl;
    }

    // load the scene graph to render
    vsg::Path filename = "https://raw.githubusercontent.com/robertosfield/TestData/master/Earth_VSG/earth.vsgb";
    if (argc > 1) filename = arguments[1];
    vsg::ref_ptr<vsg::Node> vsg_scene = vsg::read_cast<vsg::Node>(filename, options);
    if (!vsg_scene) return 0;

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel(vsg_scene->getObject<vsg::EllipsoidModel>("EllipsoidModel"));
    if (ellipsoidModel)
    {
        double horizonMountainHeight = 0.0;
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // add trackball to control the Camera
    viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

    // add the CommandGraph to render the scene
    auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // compile all Vulkan objects and transfer image, vertex and primitive data to GPU
    viewer->compile();

    // rendering main loop
    while (viewer->advanceToNextFrame())
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
