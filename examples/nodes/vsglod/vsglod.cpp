#include <vsg/all.h>

#ifdef Tracy_FOUND
#    include <vsg/utils/TracyInstrumentation.h>
#endif

#include <iostream>

struct ModelSettings
{
    vsg::ref_ptr<vsg::Options> options;
    double lodMinScreenRatio = 0.01;
};

vsg::ref_ptr<vsg::Node> createTestScene(const ModelSettings& settings)
{
    auto builder = vsg::Builder::create();
    builder->options = settings.options;

    auto scene = vsg::Group::create();

    vsg::GeometryInfo geomInfo;
    geomInfo.color.set(1.0f, 1.0f, 0.5f, 1.0f);
    vsg::StateInfo stateInfo;

    auto node = builder->createCone(geomInfo, stateInfo);
    auto node2 = builder->createCylinder(geomInfo, stateInfo);

    vsg::ComputeBounds cb;
    node->accept(cb);
    node2->accept(cb);
    auto bb = cb.bounds;

    // shows empty, then node2, then node when approaching
    auto lod = vsg::LOD::create();
    lod->bound = vsg::dsphere((bb.min + bb.max) * 0.5, vsg::length(bb.max - bb.min));
    lod->addChild(vsg::LOD::Child{settings.lodMinScreenRatio + 1, node});
    lod->addChild(vsg::LOD::Child{settings.lodMinScreenRatio, node2});
    scene->addChild(lod);

    geomInfo.position += geomInfo.dx * 1.5f;
    geomInfo.color.set(1.0f, 0.5f, 1.0f, 1.0f);
    auto node3 = builder->createCone(geomInfo, stateInfo);
    auto node4 = builder->createSphere(geomInfo, stateInfo);

    vsg::ComputeBounds cb2;
    node3->accept(cb2);
    node4->accept(cb2);
    auto bb2 = cb2.bounds;

    // should show node4, then node3 when approaching
    auto lod2 = vsg::LOD::create();
    lod2->bound = vsg::dsphere((bb2.min + bb2.max) * 0.5, vsg::length(bb2.max - bb2.min));
    lod2->addChild(vsg::LOD::Child{settings.lodMinScreenRatio, node3});
    lod2->addChild(vsg::LOD::Child{0.0, node4});
    scene->addChild(lod2);

    // should show empty, then node5 when approaching
    geomInfo.position += geomInfo.dx * 1.5f;
    geomInfo.color.set(0.5f, 1.0f, 1.0f, 1.0f);
    auto node5 = builder->createCone(geomInfo, stateInfo);
    vsg::ComputeBounds cb3;
    node5->accept(cb3);
    auto bb3 = cb3.bounds;

    auto lod3 = vsg::LOD::create();
    lod3->bound = vsg::dsphere((bb3.min + bb3.max) * 0.5, vsg::length(bb3.max - bb3.min));
    lod3->addChild(vsg::LOD::Child{settings.lodMinScreenRatio + 0.5, node5});
    scene->addChild(lod3);

    // should show for node7, then empty when approaching
    geomInfo.position += geomInfo.dx * 1.5f;
    geomInfo.color.set(0.5f, 0.5f, 1.0f, 1.0f);
    auto node6 = vsg::Group::create();
    auto node7 = builder->createSphere(geomInfo, stateInfo);
    vsg::ComputeBounds cb4;
    node6->accept(cb4);
    node7->accept(cb4);
    auto bb4 = cb4.bounds;

    auto lod4 = vsg::LOD::create();
    lod4->bound = vsg::dsphere((bb4.min + bb4.max) * 0.5, vsg::length(bb4.max - bb4.min));
    lod4->addChild(vsg::LOD::Child{settings.lodMinScreenRatio + 1, node6});
    lod4->addChild(vsg::LOD::Child{settings.lodMinScreenRatio, node7});
    scene->addChild(lod4);

    auto bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
    vsg::info("createTestScene() extents = ", bounds);

    return scene;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create(arguments);

    options->readOptions(arguments);

    bool reportAverageFrameRate = arguments.read("--fps");
    auto numFrames = arguments.value(-1, "-f");

    if (arguments.read({"-t", "--test"}))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->fullscreen = true;
        reportAverageFrameRate = true;
    }
    if (arguments.read("--st"))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->width = 192, windowTraits->height = 108;
        windowTraits->decoration = false;
        reportAverageFrameRate = true;
    }

    const double invalid_time = std::numeric_limits<double>::max();
    auto duration = arguments.value(invalid_time, "--duration");

    vsg::ref_ptr<vsg::Instrumentation> instrumentation;
    if (arguments.read({"--gpu-annotation", "--ga"}) && vsg::isExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        windowTraits->debugUtils = true;

        auto gpu_instrumentation = vsg::GpuAnnotation::create();
        if (arguments.read("--func")) gpu_instrumentation->labelType = vsg::GpuAnnotation::SourceLocation_function;

        instrumentation = gpu_instrumentation;
    }
    else if (arguments.read({"--profiler", "--pr"}))
    {
        // set Profiler options
        auto settings = vsg::Profiler::Settings::create();
        arguments.read("--cpu", settings->cpu_instrumentation_level);
        arguments.read("--gpu", settings->gpu_instrumentation_level);
        arguments.read("--log-size", settings->log_size);
        arguments.read("--gpu-size", settings->gpu_timestamp_size);

        // create the profiler
        instrumentation = vsg::Profiler::create(settings);
    }
#ifdef Tracy_FOUND
    else if (arguments.read("--tracy"))
    {
        windowTraits->deviceExtensionNames.push_back(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);

        auto tracy_instrumentation = vsg::TracyInstrumentation::create();
        arguments.read("--cpu", tracy_instrumentation->settings->cpu_instrumentation_level);
        arguments.read("--gpu", tracy_instrumentation->settings->gpu_instrumentation_level);
        instrumentation = tracy_instrumentation;
    }
#endif

    ModelSettings settings;
    settings.options = options;

    vsg::ref_ptr<vsg::ResourceHints> resourceHints;
    if (auto resourceHintsFilename = arguments.value<vsg::Path>("", "--rh"))
    {
        resourceHints = vsg::read_cast<vsg::ResourceHints>(resourceHintsFilename, options);
    }

    if (!resourceHints) resourceHints = vsg::ResourceHints::create();

    if (auto outputResourceHintsFilename = arguments.value<vsg::Path>("", "--orh"))
    {
        if (!resourceHints) resourceHints = vsg::ResourceHints::create();
        vsg::write(resourceHints, outputResourceHintsFilename, options);
        return 0;
    }

    auto shaderHints = vsg::ShaderCompileSettings::create();
    vsg::ref_ptr<vsg::ShadowSettings> shadowSettings;

    auto pathFilename = arguments.value<vsg::Path>("", "-p");
    auto outputFilename = arguments.value<vsg::Path>("", "-o");

    auto inherit = arguments.read("--inherit");
    auto direction = arguments.value(vsg::dvec3(0.0, 0.0, -1.0), "--direction");
    auto angleSubtended = arguments.value<float>(0.0090f, "--angleSubtended");

    vsg::ref_ptr<vsg::StateGroup> stateGroup;
    if (inherit)
    {
        auto shaderSet = vsg::createPhongShaderSet(options);
        auto layout = shaderSet->createPipelineLayout({}, {0, 1});

        stateGroup = vsg::StateGroup::create();

        uint32_t vds_set = 0;
        stateGroup->add(vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, vds_set));

        vsg::info("Added state to inherit ");
        options->inheritedState = stateGroup->stateCommands;
    }

    vsg::ref_ptr<vsg::Node> scene;
    scene = createTestScene(settings);

    if (stateGroup)
    {
        // if setup place the StateGroup at the root of the scene graph
        stateGroup->addChild(scene);
        scene = stateGroup;
    }

    // compute the bounds of the scene graph to help position camera
    auto bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
    auto viewingDistance = vsg::length(bounds.max - bounds.min) * 2.0;

    vsg::ref_ptr<vsg::LookAt> lookAt;

    {
        vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;

        // set up the camera
        lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -viewingDistance, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    }

    //auto span = vsg::length(bounds.max - bounds.min);
    auto group = vsg::Group::create();
    group->addChild(scene);

    vsg::ref_ptr<vsg::DirectionalLight> directionalLight;
    directionalLight = vsg::DirectionalLight::create();
    directionalLight->name = "directional";
    directionalLight->color.set(1.0f, 1.0f, 1.0f);
    directionalLight->intensity = 0.95f;
    directionalLight->direction = direction;
    directionalLight->angleSubtended = angleSubtended;
    directionalLight->shadowSettings = shadowSettings;

    group->addChild(directionalLight);

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

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    perspective = vsg::Perspective::create(30, 1.0, 1.0, 1000);

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add the camera and scene graph to View
    auto view = vsg::View::create();
    view->camera = camera;
    view->addChild(scene);

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    auto cameraAnimation = vsg::CameraAnimationHandler::create(camera, pathFilename, options);
    viewer->addEventHandler(cameraAnimation);
    if (cameraAnimation->animation)
    {
        cameraAnimation->play();
        if (reportAverageFrameRate && duration == invalid_time) duration = cameraAnimation->animation->maxTime();
    }

    viewer->addEventHandler(vsg::Trackball::create(camera));

    auto renderGraph = vsg::RenderGraph::create(window, view);
    auto commandGraph = vsg::CommandGraph::create(window, renderGraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    if (instrumentation) viewer->assignInstrumentation(instrumentation);

    viewer->compile(resourceHints);

    auto startTime = vsg::clock::now();
    double numFramesCompleted = 0.0;

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0) && (viewer->getFrameStamp()->simulationTime < duration))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();
        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();

        numFramesCompleted += 1.0;
    }

    if (reportAverageFrameRate)
    {
        auto elapesedTime = std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - startTime).count();
        if (numFramesCompleted > 0.0)
        {
            std::cout << "Average frame rate = " << (numFramesCompleted / elapesedTime) << std::endl;
        }
    }

    if (auto profiler = instrumentation.cast<vsg::Profiler>())
    {
        instrumentation->finish();
        profiler->log->report(std::cout);
    }

    return 0;
}
