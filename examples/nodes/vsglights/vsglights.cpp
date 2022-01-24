#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>

vsg::ref_ptr<vsg::Node> createTestScene(vsg::ref_ptr<vsg::Options> options, vsg::ref_ptr<vsg::DescriptorSetLayout> viewDescriptorSetLayout)
{
    auto builder = vsg::Builder::create();
    builder->options = options;

    auto scene = vsg::Group::create();

    vsg::GeometryInfo geomInfo;
    vsg::StateInfo stateInfo;
    stateInfo.viewDescriptorSetLayout = viewDescriptorSetLayout;

    scene->addChild(builder->createBox(geomInfo, stateInfo));
    geomInfo.position += geomInfo.dx * 1.5f;

    scene->addChild(builder->createSphere(geomInfo, stateInfo));
    geomInfo.position += geomInfo.dx * 1.5f;

    scene->addChild(builder->createCylinder(geomInfo, stateInfo));
    geomInfo.position += geomInfo.dx * 1.5f;

    scene->addChild(builder->createCapsule(geomInfo, stateInfo));
    geomInfo.position += geomInfo.dx * 1.5f;


    auto bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
    double diameter = vsg::length(bounds.max - bounds.min);
    geomInfo.position.set((bounds.min.x + bounds.max.x)*0.5, (bounds.min.y + bounds.max.y)*0.5, bounds.min.z);
    geomInfo.dx.set(diameter, 0.0, 0.0);
    geomInfo.dy.set(0.0, diameter, 0.0);

    scene->addChild(builder->createQuad(geomInfo, stateInfo));

    return scene;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->objectCache = vsg::ObjectCache::create();

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsglights";


    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});

    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    auto numFrames = arguments.value(-1, "-f");
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--double-buffer")) windowTraits->swapchainPreferences.imageCount = 2;
    if (arguments.read("--triple-buffer")) windowTraits->swapchainPreferences.imageCount = 3; // default
    if (arguments.read("-t"))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->width = 192, windowTraits->height = 108;
        windowTraits->decoration = false;
    }

    auto outputFilename = arguments.value<std::string>("", "-o");


    auto view = vsg::View::create();
    auto viewDependentState = view->viewDependentState;
    auto viewDescriptorSetLayout = viewDependentState->descriptorSetLayout;

    auto scene = vsg::Group::create();

    if (argc>1)
    {
        vsg::Path filename = argv[1];
        auto model = vsg::read_cast<vsg::Node>(filename, options);
        if (!model)
        {
            std::cout<<"Faled to load "<<filename<<std::endl;
            return 1;
        }

        scene->addChild(model);
    }
    else
    {
        auto model = createTestScene(options, viewDescriptorSetLayout);
        scene->addChild(model);
    }


    // compute the bounds of the scene graph to help position camera
    auto bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
#if 1
    // ambient light
    {
        auto ambientLight = vsg::AmbientLight::create();
        ambientLight->name = "ambient";
        ambientLight->color.set(1.0, 1.0, 1.0);
        ambientLight->intensity = 0.05;
        //scene->addChild(ambientLight);
    }

    // directional light
    {
        auto directionalLight = vsg::DirectionalLight::create();
        directionalLight->name = "directional";
        directionalLight->color.set(1.0, 1.0, 1.0);
        directionalLight->intensity = 0.1;
        directionalLight->direction.set(0.0, 0.0, -1.0);
        scene->addChild(directionalLight);
    }

    // point light
    {
        auto pointLight = vsg::PointLight::create();
        pointLight->name = "point";
        pointLight->color.set(0.0, 0.0, 1.0);
        pointLight->intensity = 0.5;
        pointLight->position.set((bounds.max.x+bounds.min.x)*0.5, (bounds.max.y+bounds.min.y)*0.5, (bounds.max.z*0.75+bounds.min.z*0.25));

        // enable culling of the point light by decorating with a CullGroup
        auto cullGroup = vsg::CullGroup::create();
        cullGroup->bound.center = pointLight->position;
        cullGroup->bound.radius = vsg::length(bounds.max-bounds.min)*0.5;

        cullGroup->addChild(pointLight);

        scene->addChild(cullGroup);
    }
#endif

    // spot light
    {
        auto spotLight = vsg::SpotLight::create();
        spotLight->name = "spot";
        spotLight->color.set(1.0, 1.0, 1.0);
        spotLight->intensity = 2.0;
        spotLight->position.set(bounds.min.x, (bounds.max.y+bounds.min.y)*0.5, bounds.max.z+1.0);
        spotLight->direction.set(1.0, 1.0, -1.0);
        spotLight->innerAngle = vsg::radians(30.0);
        spotLight->outerAngle = vsg::radians(45.0);

        // enable culling of the spot light by decorating with a CullGroup
        auto cullGroup = vsg::CullGroup::create();
        cullGroup->bound.center = spotLight->position;
        cullGroup->bound.radius = vsg::length(bounds.max-bounds.min)*0.5;

        cullGroup->addChild(spotLight);

        scene->addChild(cullGroup);
    }

    // write out scene if required
    if (!outputFilename.empty())
    {
        vsg::write(scene, outputFilename, options);
        return 0;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    vsg::ref_ptr<vsg::LookAt> lookAt;

    vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;
    double radius = vsg::length(bounds.max - bounds.min) * 0.6;

    // set up the camera
    lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    double nearFarRatio = 0.001;
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.0);

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add the camera and scene graph to View
    view->camera = camera;
    view->addChild(scene);

    // set up the compilation support in builder to allow us to interactively create and compile subgraphs from within the IntersectionHandler
    // builder->setup(window, camera->viewportState);

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(vsg::Trackball::create(camera));

    auto renderGraph = vsg::RenderGraph::create(window);
    renderGraph->addChild(view);

    auto commandGraph = vsg::CommandGraph::create(window);
    commandGraph->addChild(renderGraph);

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile();

    auto startTime = vsg::clock::now();
    double numFramesCompleted = 0.0;

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();

        numFramesCompleted += 1.0;
    }

    auto duration = std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - startTime).count();
    if (numFramesCompleted > 0.0)
    {
        std::cout << "Average frame rate = " << (numFramesCompleted / duration) << std::endl;
    }

    return 0;
}
