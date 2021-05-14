#include <vsg/all.h>

#include <chrono>
#include <iostream>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "Anaglyphic Sterep";
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

    vsg::Path filename = "models/lz.osgt";
    if (argc > 1) filename = arguments[1];

    auto vsg_scene = vsg::read_cast<vsg::Node>(filename, options);
    if (!vsg_scene)
    {
        std::cout << "Unable to load model." << std::endl;
        return 1;
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

    auto master_camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // create the left eye camera
    auto left_relative_perspective = vsg::RelativeProjection::create(perspective, vsg::translate(-0.1, 0.0, 0.0));
    auto left_relative_view = vsg::RelativeView::create(lookAt, vsg::rotate(vsg::radians(5.0), 0.0, 1.0, 0.0));
    auto left_camera = vsg::Camera::create(left_relative_perspective, left_relative_view, vsg::ViewportState::create(window->extent2D()));

    // create the left eye camera
    auto right_relative_perspective = vsg::RelativeProjection::create(perspective, vsg::translate(0.1, 0.0, 0.0));
    auto right_relative_view = vsg::RelativeView::create(lookAt, vsg::rotate(vsg::radians(-5.0), 0.0, 1.0, 0.0));
    auto right_camera = vsg::Camera::create(right_relative_perspective, right_relative_view, vsg::ViewportState::create(window->extent2D()));


    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // add event handlers, in the order we wish event to be handled.
    viewer->addEventHandler(vsg::Trackball::create(master_camera));

    auto renderGraph = vsg::RenderGraph::create(window);

    auto left_view = vsg::View::create(left_camera, vsg_scene);
    renderGraph->addChild(left_view);

    // clear the depth buffer before view2 gets rendered
    VkClearAttachment depth_attachment{VK_IMAGE_ASPECT_DEPTH_BIT, 1, VkClearValue{1.0f, 0.0f}};
    VkClearRect rect{right_camera->getRenderArea(), 0, 1};
    auto clearAttachments = vsg::ClearAttachments::create(vsg::ClearAttachments::Attachments{depth_attachment}, vsg::ClearAttachments::Rects{rect});
    renderGraph->addChild(clearAttachments);

    auto right_view = vsg::View::create(right_camera, vsg_scene);
    renderGraph->addChild(right_view);

    auto commandGraph = vsg::CommandGraph::create(window);
    commandGraph->addChild(renderGraph);
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
