#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>

vsg::ref_ptr<vsg::Node> createTestScene(vsg::ref_ptr<vsg::Options> options, vsg::Path textureFile = {})
{
    auto builder = vsg::Builder::create();
    builder->options = options;

    auto scene = vsg::Group::create();

    vsg::GeometryInfo geomInfo;
    vsg::StateInfo stateInfo;

    if (textureFile) stateInfo.image = vsg::read_cast<vsg::Data>(textureFile, options);

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

    vsg::info("createTestScene() extents = ", bounds);

    return scene;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgshadows";


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
    if (arguments.read({"-t", "--test"}))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->fullscreen = true;
    }
    if (arguments.read("--st"))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->width = 192, windowTraits->height = 108;
        windowTraits->decoration = false;
    }

    bool depthClamp = arguments.read({"--dc", "--depthClamp"});
    if (depthClamp)
    {
        std::cout<<"Enabled dpeth clamp."<<std::endl;
        auto deviceFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();
        deviceFeatures->get().samplerAnisotropy = VK_TRUE;
        deviceFeatures->get().depthClamp = VK_TRUE;
    }

    vsg::Path textureFile = arguments.value(vsg::Path{}, {"-i", "--image"});

    auto numShadowMapsPerLight = arguments.value<uint32_t>(1, "--sm");
    auto numLights = arguments.value<uint32_t>(1, "-n");

    vsg::ref_ptr<vsg::ResourceHints> resourceHints;
    if (auto resourceHintsFilename = arguments.value<vsg::Path>("", "--rh"))
    {
        resourceHints = vsg::read_cast<vsg::ResourceHints>(resourceHintsFilename, options);
    }

    if (auto outputResourceHintsFilename = arguments.value<vsg::Path>("", "--orh"))
    {
        if (!resourceHints) resourceHints = vsg::ResourceHints::create();
        vsg::write(resourceHints, outputResourceHintsFilename, options);
        return 0;
    }

    if (arguments.read({"-c", "--custom"}))
    {
        auto phong_vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/standard.vert", options);
        auto phong_fragShader = vsg::read_cast<vsg::ShaderStage>("shaders/standard_phong.frag", options);
        auto phong = vsg::createPhongShaderSet(options);
        if (phong && phong_vertexShader && phong_fragShader)
        {
            // replace shaders
            phong->stages.clear();
            phong->stages.push_back(phong_vertexShader);
            phong->stages.push_back(phong_fragShader);

            if (depthClamp)
            {
                auto rasterizationState = vsg::RasterizationState::create();
                rasterizationState->depthClampEnable = VK_TRUE;
                phong->defaultGraphicsPipelineStates.push_back(rasterizationState);
            }

            vsg::info("phong->defaultGraphicsPipelineStates.size() = ", phong->defaultGraphicsPipelineStates.size());
            for(auto& ps: phong->defaultGraphicsPipelineStates)
            {
                vsg::info("   ", ps->className());
            }

            // clear prebuilt variants
            phong->variants.clear();

            options->shaderSets["phong"] = phong;

            std::cout<<"Replaced phong shader"<<std::endl;
        }
    }

    auto pathFilename = arguments.value<vsg::Path>("", "-p");
    auto outputFilename = arguments.value<vsg::Path>("", "-o");

    vsg::ref_ptr<vsg::Node> scene;
    if (argc>1)
    {
        vsg::Path filename = argv[1];
        auto model = vsg::read_cast<vsg::Node>(filename, options);
        if (!model)
        {
            std::cout<<"Faled to load "<<filename<<std::endl;
            return 1;
        }

        scene = model;
    }
    else
    {
        scene = createTestScene(options, textureFile);
    }

    // compute the bounds of the scene graph to help position camera
    auto bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;

    //auto span = vsg::length(bounds.max - bounds.min);
    auto group = vsg::Group::create();
    group->addChild(scene);


    if (numLights >= 1)
    {
        auto directionalLight = vsg::DirectionalLight::create();
        directionalLight->name = "directional";
        directionalLight->color.set(1.0, 1.0, 1.0);
        directionalLight->intensity = 0.9;
        directionalLight->direction.set(0.0, 0.0, -1.0);
        directionalLight->shadowMaps = numShadowMapsPerLight;
        group->addChild(directionalLight);
    }

    if (numLights >= 2)
    {
        auto ambientLight = vsg::AmbientLight::create();
        ambientLight->name = "ambient";
        ambientLight->color.set(1.0, 1.0, 1.0);
        ambientLight->intensity = 0.1;
        group->addChild(ambientLight);
    }

    if (numLights >= 3)
    {
        auto directionalLight = vsg::DirectionalLight::create();
        directionalLight->name = "2nd directional";
        directionalLight->color.set(1.0, 1.0, 0.0);
        directionalLight->intensity = 0.9;
        directionalLight->direction = vsg::normalize(vsg::vec3(1.0, 1.0, -1.0));
        directionalLight->shadowMaps = numShadowMapsPerLight;
        group->addChild(directionalLight);
    }

    scene = group;

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
    auto view = vsg::View::create();
    view->camera = camera;
    view->addChild(scene);

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));
    if (pathFilename)
    {
        auto animationPath = vsg::read_cast<vsg::AnimationPath>(pathFilename, options);
        if (!animationPath)
        {
            std::cout<<"Warning: unable to read animation path : "<<pathFilename<<std::endl;
            return 1;
        }

        auto animationPathHandler = vsg::AnimationPathHandler::create(camera, animationPath, viewer->start_point());
        animationPathHandler->printFrameStatsToConsole = true;
        viewer->addEventHandler(animationPathHandler);
    }
    else
    {
        viewer->addEventHandler(vsg::Trackball::create(camera));
    }

    auto renderGraph = vsg::RenderGraph::create(window, view);
    auto commandGraph = vsg::CommandGraph::create(window, renderGraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile(resourceHints);

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
