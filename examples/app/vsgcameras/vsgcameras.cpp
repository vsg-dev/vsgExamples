#include <vsg/all.h>

#include <chrono>
#include <iostream>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

class CameraSelector : public vsg::Inherit<vsg::Visitor, CameraSelector>
{
public:
    using Cameras = decltype(vsg::FindCameras::cameras);

    CameraSelector(vsg::ref_ptr<vsg::Camera> in_camera, const Cameras& in_cameras) :
        main_camera(in_camera),
        cameras(in_cameras)
    {
        auto lookAt = main_camera->viewMatrix.cast<vsg::LookAt>();
        if (lookAt) original_viewMatrix = vsg::LookAt::create(*lookAt);
    }

    vsg::ref_ptr<vsg::Camera> main_camera;
    vsg::ref_ptr<vsg::LookAt> original_viewMatrix;
    Cameras cameras;
    int currentCameraIndex = 0;

    void apply(vsg::ButtonPressEvent& buttonPress) override
    {
        if (cameras.empty()) return;

        if (buttonPress.button == 4)
        {
            // next camera
            currentCameraIndex++;
            currentCameraIndex %= cameras.size();

            selectCameraByIndex(currentCameraIndex);
        }
        else if (buttonPress.button == 5)
        {
            // previous camera
            currentCameraIndex--;
            currentCameraIndex %= cameras.size();

            selectCameraByIndex(currentCameraIndex);
        }
    }

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (keyPress.keyBase == '0')
        {
            if (original_viewMatrix)
            {
                auto lookAt = main_camera->viewMatrix.cast<vsg::LookAt>();
                if (lookAt)
                {
                    *(lookAt) = *original_viewMatrix;
                }
            }
        }
        else if ((keyPress.keyBase >= '1') && (keyPress.keyBase <= '9'))
        {
            currentCameraIndex = keyPress.keyBase - '1';

            selectCameraByIndex(currentCameraIndex);
        }
    }

private:
    void selectCameraByIndex(int index)
    {
        int cameraIndex = 0;
        for (auto& [nodePath, camera] : cameras)
        {
            if (cameraIndex == index)
            {
                auto begin = nodePath.begin();
                auto end = nodePath.end();
                if (begin != end) --end;

                // auto matrix = vsg::computeTransform(nodePath);
                auto matrix = vsg::visit<vsg::ComputeTransform>(begin, end).matrix;

                std::cout << "Matched: " << camera->name << " " << matrix << std::endl;

                auto selected_lookAt = camera->viewMatrix.cast<vsg::LookAt>();
                auto main_lookAt = main_camera->viewMatrix.cast<vsg::LookAt>();
                if (main_lookAt)
                {
                    *main_lookAt = *selected_lookAt;
                    main_lookAt->transform(matrix);
                }
                break;
            }
            ++cameraIndex;
        }
    }
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);


    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgcameras - Multiple Views with different ways of configuring/tracking Cameras";
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }

    // set up instrumentation if required
    bool reportAverageFrameRate = arguments.read("--fps");
    auto logFilename = arguments.value<vsg::Path>("", "--log");
    vsg::ref_ptr<vsg::Instrumentation> instrumentation;
    if (arguments.read({"--gpu-annotation", "--ga"}) && vsg::isExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        windowTraits->debugUtils = true;

        auto gpu_instrumentation = vsg::GpuAnnotation::create();
        if (arguments.read("--name"))
            gpu_instrumentation->labelType = vsg::GpuAnnotation::SourceLocation_name;
        else if (arguments.read("--className"))
            gpu_instrumentation->labelType = vsg::GpuAnnotation::Object_className;
        else if (arguments.read("--func"))
            gpu_instrumentation->labelType = vsg::GpuAnnotation::SourceLocation_function;

        instrumentation = gpu_instrumentation;
    }
    else if (arguments.read({"--profiler", "--pr"}))
    {
        // set Profiler options
        auto settings = vsg::Profiler::Settings::create();
        arguments.read("--cpu", settings->cpu_instrumentation_level);
        arguments.read("--gpu", settings->gpu_instrumentation_level);
        arguments.read("--log-size", settings->log_size);

        // create the profiler
        instrumentation = vsg::Profiler::create(settings);
    }

    if (arguments.read({"-t", "--test"}))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        reportAverageFrameRate = true;
    }

    vsg::ref_ptr<vsg::ResourceHints> resourceHints;
    if (auto resourceHintsFilename = arguments.value<vsg::Path>("", "--rh"))
    {
        resourceHints = vsg::read_cast<vsg::ResourceHints>(resourceHintsFilename, options);
    }

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    vsg::Path filename = (argc > 1) ? arguments[1] : vsg::Path("models/lz.vsgt");
    auto scenegraph = vsg::read_cast<vsg::Node>(filename, options);
    if (!scenegraph)
    {
        std::cout << "Please specify a valid model on command line" << std::endl;
        return 1;
    }

    // compute the bounds of the scene graph to help position camera
    auto bounds = vsg::visit<vsg::ComputeBounds>(scenegraph).bounds;
    vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;
    double radius = vsg::length(bounds.max - bounds.min) * 0.5;
    double nearFarRatio = 0.001;

    auto scene_cameras = vsg::visit<vsg::FindCameras>(scenegraph).cameras;

    if (scene_cameras.empty())
    {
        // no cameras are present in the scene graph so add them
        auto root = vsg::Group::create();
        root->addChild(scenegraph);

        auto perspective = vsg::Perspective::create(90.0, 1.0, nearFarRatio * radius, radius * 4.0);

        // left
        {
            auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(radius * 2.0, 0.0, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
            auto camera = vsg::Camera::create(perspective, lookAt);
            camera->name = "Left";
            root->addChild(camera);
        }

        // right
        {
            auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(-radius * 2.0, 0.0, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
            auto camera = vsg::Camera::create(perspective, lookAt);
            camera->name = "Right";
            root->addChild(camera);
        }

        // front
        {
            auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 2.0, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
            auto camera = vsg::Camera::create(perspective, lookAt);
            camera->name = "Front";
            root->addChild(camera);
        }

        // top
        {
            auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, 0.0, radius * 2.0), centre, vsg::dvec3(0.0, 1.0, 0.0));
            auto camera = vsg::Camera::create(perspective, lookAt);
            camera->name = "Top";
            root->addChild(camera);
        }

        scenegraph = root;

        // refresh the scene_cameras list.
        scene_cameras = vsg::visit<vsg::FindCameras>(scenegraph).cameras;
    }

    for (auto& [nodePath, camera] : scene_cameras)
    {
        std::cout << "\ncamera = " << camera << ", " << camera->name << " :";
        for (auto& node : nodePath) std::cout << " " << node;
        std::cout << std::endl;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create window." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    uint32_t width = window->extent2D().width;
    uint32_t height = window->extent2D().height;

    // CommandGraph to hold the different RenderGraphs used to render each view
    auto commandGraph = vsg::CommandGraph::create(window);


    // add headlights to views to make sure any objects that need lighting have it.
    auto headlight = vsg::createHeadlight();

    // set up main interactive view
    {
        auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0),
                                          centre, vsg::dvec3(0.0, 0.0, 1.0));

        auto perspective = vsg::Perspective::create(30.0, static_cast<double>(width) / static_cast<double>(height),
                                                    nearFarRatio * radius, radius * 4.5);

        auto viewportState = vsg::ViewportState::create(0, 0, width, height);

        // create the vsg::RenderGraph and associated vsg::View
        auto main_camera = vsg::Camera::create(perspective, lookAt, viewportState);
        auto main_view = vsg::View::create(main_camera, scenegraph);
        main_view->addChild(headlight);

        auto main_RenderGraph = vsg::RenderGraph::create(window, main_view);

        commandGraph->addChild(main_RenderGraph);

        viewer->addEventHandler(vsg::Trackball::create(main_camera));
        viewer->addEventHandler(CameraSelector::create(main_camera, scene_cameras));
    }

    // set up secondary views, one per camera found in the scene graph
    uint32_t margin = 10;
    uint32_t division = static_cast<uint32_t>(scene_cameras.size());
    if (division < 3) division = 3;

    uint32_t secondary_width = width / division;
    uint32_t secondary_height = ((height - margin) / division) - margin;

    uint32_t x = width - secondary_width - margin;
    uint32_t y = margin;

    for (auto& [nodePath, camera] : scene_cameras)
    {
        // create a RenderGraph to add a secondary vsg::View on the top right part of the window.
        auto projectionMatrix = vsg::clone(camera->projectionMatrix);
        auto viewMatrix = vsg::TrackingViewMatrix::create(nodePath);
        auto viewportState = vsg::ViewportState::create(x, y, secondary_width, secondary_height);

        auto secondary_camera = vsg::Camera::create(projectionMatrix, viewMatrix, viewportState);

        auto secondary_view = vsg::View::create(secondary_camera, scenegraph);
        secondary_view->addChild(headlight);

        auto secondary_RenderGraph = vsg::RenderGraph::create(window, secondary_view);
        secondary_RenderGraph->clearValues[0].color = vsg::sRGB_to_linear(0.2f, 0.2f, 0.2f, 1.0f);
        commandGraph->addChild(secondary_RenderGraph);


        y += secondary_height + margin;
    }

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    if (instrumentation) viewer->assignInstrumentation(instrumentation);

    viewer->compile(resourceHints);

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    if (reportAverageFrameRate)
    {
        double fps = static_cast<double>(viewer->getFrameStamp()->frameCount) / std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - viewer->start_point()).count();
        std::cout << "Average frame rate = " << fps << " fps" << std::endl;
    }

    if (auto profiler = instrumentation.cast<vsg::Profiler>())
    {
        instrumentation->finish();
        if (logFilename)
        {
            std::ofstream fout(logFilename);
            profiler->log->report(fout);
        }
        else
        {
            profiler->log->report(std::cout);
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
