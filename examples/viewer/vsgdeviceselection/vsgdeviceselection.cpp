#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    // set up vsg::Options to pass in filepaths and ReaderWriter's and other IO realted options to use when reading and writing files.
#ifdef vsgXchange_all
    auto options = vsg::Options::create(vsgXchange::all::create());
#endif
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_FILE");
    arguments.read(options);

    vsg::Path filename = "https://raw.githubusercontent.com/robertosfield/TestData/master/Earth_VSG/earth.vsgb";
    if (argc > 1) filename = arguments[1];

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // load the scene graph
    vsg::ref_ptr<vsg::Node> vsg_scene = vsg::read_cast<vsg::Node>(filename, options);
    if (!vsg_scene) return 0;

    // create the viewer and assign window(s) to it
    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "Hello World";
    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    viewer->addWindow(window);


    std::cout<<"Before initialization"<<std::endl;
    std::cout<<"    instance = "<<window->getInstance()<<std::endl;
    std::cout<<"    surface = "<<window->getSurface()<<std::endl;
    std::cout<<"    device = "<<window->getDevice()<<std::endl;

    // custom device setup based on Window::_initDevice()
    {
        // use the Window implementation to create the device and surface
        auto instance = window->getOrCreateInstance();
        auto surface = window->getOrCreateSurface();

        auto physicalDevices = instance->getPhysicalDevices();

        std::cout<<"\nphysicalDevices.size() = "<<physicalDevices.size()<<std::endl;
        for(auto& physicalDevice : physicalDevices)
        {
            std::cout<<"   "<<physicalDevice<<std::endl;
        }

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

        // set up device
        auto [physicalDevice, queueFamily, presentFamily] = instance->getPhysicalDeviceAndQueueFamily(windowTraits->queueFlags, surface);
        if (!physicalDevice || queueFamily < 0 || presentFamily < 0) throw vsg::Exception{"Error: vsg::Window::create(...) failed to create Window, no Vulkan PhysicalDevice supported.", VK_ERROR_INVALID_EXTERNAL_HANDLE};

        vsg::QueueSettings queueSettings{vsg::QueueSetting{queueFamily, {1.0}}, vsg::QueueSetting{presentFamily, {1.0}}};
        auto device = vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions, windowTraits->deviceFeatures, instance->getAllocationCallbacks());

        std::cout<<"\ncreated our own device "<<device<<std::endl;

        windowTraits->device = device;
    }

    std::cout<<"\nAfter custom device initialization"<<std::endl;

    std::cout<<"    instance = "<<window->getInstance()<<std::endl;
    std::cout<<"    surface = "<<window->getSurface()<<std::endl;
    std::cout<<"    device = "<<window->getOrCreateDevice()<<std::endl;

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
