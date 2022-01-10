#include <vsg/all.h>

#include <chrono>
#include <iostream>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

vsg::ref_ptr<vsg::Camera> createCameraForScene(vsg::Node* scenegraph, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0),
                                      centre, vsg::dvec3(0.0, 0.0, 1.0));

    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(width) / static_cast<double>(height),
                                                nearFarRatio * radius, radius * 4.5);

    auto viewportstate = vsg::ViewportState::create(x, y, width, height);

    return vsg::Camera::create(perspective, lookAt, viewportstate);
}

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
            uint16_t keyForCamera{'1'};
            for(auto& [nodePath, camera] : cameras)
            {
                if (keyForCamera == keyPress.keyBase)
                {
                    auto begin = nodePath.begin();
                    auto end = nodePath.end();
                    if (begin != end) --end;

                    // auto matrix = vsg::computeTransform(nodePath);
                    auto matrix = vsg::visit<vsg::ComputeTransform>(begin, end).matrix;

                    std::cout<<"Matched: "<<camera->name<<" "<<matrix<<std::endl;

                    auto selected_lookAt = camera->viewMatrix.cast<vsg::LookAt>();
                    auto main_lookAt = main_camera->viewMatrix.cast<vsg::LookAt>();
                    if (main_lookAt)
                    {
                        *main_lookAt = *selected_lookAt;
                        main_lookAt->transform(matrix);
                    }
                    break;
                }
                ++keyForCamera;
            }
        }
    }
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "Multiple Views";
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    vsg::Path filename = (argc > 1) ? arguments[1] : vsg::Path("models/lz.vsgt");
    auto scenegraph = vsg::read_cast<vsg::Node>(filename, options);
    if (!scenegraph)
    {
        std::cout << "Please specify a valid model on command line" << std::endl;
        return 1;
    }

    auto scene_cameras = vsg::visit<vsg::FindCameras>(scenegraph).cameras;
    for(auto& [nodePath, camera] : scene_cameras)
    {
        std::cout<<"\ncamera = "<<camera<<", "<<camera->name<<" :";
        for(auto& node : nodePath) std::cout<<" "<<node;
        std::cout<<std::endl;
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

    uint32_t width = window->extent2D().width;
    uint32_t height = window->extent2D().height;

    // create the vsg::RenderGraph and associated vsg::View
    auto main_camera = createCameraForScene(scenegraph, 0, 0, width, height);
    auto main_view = vsg::View::create(main_camera, scenegraph);

    // create an RenderinGraph to add an secondary vsg::View on the top right part of the window.
    auto secondary_camera = createCameraForScene(scenegraph, (width * 3) / 4, 0, width / 4, height / 4);
    auto secondary_view = vsg::View::create(secondary_camera, scenegraph);

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // add event handlers, in the order we wish event to be handled.
    viewer->addEventHandler(vsg::Trackball::create(secondary_camera));
    viewer->addEventHandler(vsg::Trackball::create(main_camera));

    viewer->addEventHandler(CameraSelector::create(main_camera, scene_cameras));


    auto main_RenderGraph = vsg::RenderGraph::create(window, main_view);
    auto secondary_RenderGraph = vsg::RenderGraph::create(window, secondary_view);
    secondary_RenderGraph->clearValues[0].color = {{0.2f, 0.2f, 0.2f, 1.0f}};

    auto commandGraph = vsg::CommandGraph::create(window);
    commandGraph->addChild(main_RenderGraph);
    commandGraph->addChild(secondary_RenderGraph);

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

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
