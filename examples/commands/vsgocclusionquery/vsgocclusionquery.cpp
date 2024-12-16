#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <chrono>
#include <iostream>

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

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
    windowTraits->windowTitle = "vsgocclusionquery";
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read("--double-buffer")) windowTraits->swapchainPreferences.imageCount = 2;
    if (arguments.read("--triple-buffer")) windowTraits->swapchainPreferences.imageCount = 3; // default
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
    if (arguments.read("--d32")) windowTraits->depthFormat = VK_FORMAT_D32_SFLOAT;
    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    arguments.read("--samples", windowTraits->samples);
    auto numFrames = arguments.value(-1, "-f");
    auto pathFilename = arguments.value<vsg::Path>("", "-p");
    if (arguments.read("--rgb")) options->mapRGBtoRGBAHint = false;

    if (int log_level = 0; arguments.read("--log-level", log_level)) vsg::Logger::instance()->level = vsg::Logger::Level(log_level);

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    if (argc <= 1)
    {
        std::cout << "Please specify a 3d model or image file on the command line." << std::endl;
        return 1;
    }

    auto group = vsg::Group::create();

    // read any vsg files and assign to the scene group
    for (int i = 1; i < argc; ++i)
    {
        vsg::Path filename = arguments[i];
        if (auto node = vsg::read_cast<vsg::Node>(filename, options))
        {
            group->addChild(node);
        }
        else
        {
            std::cout << "Unable to load file " << filename << std::endl;
        }
    }

    vsg::ref_ptr<vsg::Node> vsg_scene = group;
    if (group->children.size() == 1) vsg_scene = group->children[0];

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create window." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
    if (ellipsoidModel)
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, 0.0);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    if (pathFilename)
    {
        auto cameraAnimation = vsg::CameraAnimationHandler::create(camera, pathFilename, options);
        viewer->addEventHandler(cameraAnimation);
        if (cameraAnimation->animation) cameraAnimation->play();
    }

    viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

    auto commandGraph = vsg::CommandGraph::create(window);

    // create the query pool to collect occlusion query info
    auto query_pool = vsg::QueryPool::create();
    query_pool->queryType = VK_QUERY_TYPE_OCCLUSION;
    query_pool->queryCount = 1;

    // reset the query pool
    auto reset_query = vsg::ResetQueryPool::create(query_pool);
    commandGraph->addChild(reset_query);

    // begin query
    commandGraph->addChild(vsg::BeginQuery::create(query_pool, 0, 0));

    // add RenderGraph to render the main scene graph
    commandGraph->addChild(vsg::createRenderGraphForView(window, camera, vsg_scene));

    // add end query
    commandGraph->addChild(vsg::EndQuery::create(query_pool, 0));

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile();

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();

        std::vector<uint64_t> results(1);
        if (query_pool->getResults(results) == VK_SUCCESS)
        {
            std::cout << "results[0] = " << results[0] << std::endl;
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
