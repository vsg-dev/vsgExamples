#include <vsg/all.h>

#include <chrono>
#include <iostream>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

// A rotation tracking view matrix. It returns the rotation component
// of another viewMatrix.
class RotationTrackingMatrix : public vsg::Inherit<vsg::ViewMatrix, RotationTrackingMatrix>
{
public:
    RotationTrackingMatrix(vsg::ref_ptr<vsg::ViewMatrix> parentTransform) :
        parentTransform_(parentTransform) {}

    // The transform() returns the rotation part of the tracked matrix
    vsg::dmat4 transform(const vsg::dvec3&) const override
    {

        vsg::dvec3 translation, scale;
        vsg::dquat rotation;
        vsg::decompose(parentTransform_->transform(),
                       // output
                       translation,
                       rotation,
                       scale);

        return vsg::rotate(rotation);
    }

private:
    vsg::ref_ptr<vsg::ViewMatrix> parentTransform_;
};

// Create an arrow with the back at pos and pointing in the direction of dir
// Place a cone at the end of the arrow with the color color
vsg::ref_ptr<vsg::Node> createArrow(vsg::vec3 pos, vsg::vec3 dir, vsg::vec4 color)
{
    vsg::ref_ptr<vsg::Group> arrow = vsg::Group::create();

    vsg::Builder builder;
    vsg::GeometryInfo geomInfo;
    vsg::StateInfo stateInfo;

    geomInfo.color = vsg::vec4{1, 1, 1, 1};
    geomInfo.position = pos;
    geomInfo.transform = vsg::translate(0.0f, 0.0f, 0.5f);

    // If we don't point in the z-direction, then rotate the arrow
    if (vsg::length(vsg::cross(vsg::vec3{0, 0, 1}, dir)) > 0.0001)
    {
        vsg::vec3 axis = vsg::cross(vsg::vec3{0, 0, 1}, dir);
        float angle = acos(vsg::dot(vsg::vec3{0, 0, 1}, dir));
        geomInfo.transform = vsg::rotate(angle, axis) * geomInfo.transform;
    }

    auto axisTransform = geomInfo.transform;
    geomInfo.transform = geomInfo.transform * vsg::scale(0.1f, 0.1f, 1.0f);

    // Rotate geomInfo from pos in the direction of dir
    auto node = builder.createCylinder(geomInfo, stateInfo);
    arrow->addChild(node);

    // The cone
    geomInfo.color = color;
    // This would have been cleaner with a pre_translate transform
    geomInfo.transform = vsg::scale(0.3f, 0.3f, 0.3f) * axisTransform * vsg::translate(0.0f, 0.0f, 1.0f / 0.3f);
    node = builder.createCone(geomInfo, stateInfo);
    arrow->addChild(node);

    return arrow;
}

// Create three arrows of the coordinate axes
vsg::ref_ptr<vsg::Node> createGizmo()
{
    vsg::ref_ptr<vsg::Group> gizmo = vsg::Group::create();

    gizmo->addChild(createArrow(vsg::vec3{0, 0, 0}, vsg::vec3{1, 0, 0}, vsg::vec4{1, 0, 0, 1}));
    gizmo->addChild(createArrow(vsg::vec3{0, 0, 0}, vsg::vec3{0, 1, 0}, vsg::vec4{0, 1, 0, 1}));
    gizmo->addChild(createArrow(vsg::vec3{0, 0, 0}, vsg::vec3{0, 0, 1}, vsg::vec4{0, 0, 1, 1}));

    vsg::Builder builder;
    vsg::GeometryInfo geomInfo;
    vsg::StateInfo stateInfo;
    geomInfo.color = vsg::vec4{1, 1, 1, 1};
    geomInfo.transform = vsg::scale(0.1f, 0.1f, 0.1f);

    auto sphere = builder.createSphere(geomInfo, stateInfo);
    gizmo->addChild(sphere);

    return gizmo;
}

// Create a tracking overlay with a axes view that shows the orientation
// of the main camera view matrix
vsg::ref_ptr<vsg::View> createAxesView(vsg::ref_ptr<vsg::Camera> camera,
                                       double aspectRatio)
{
    auto viewMat = RotationTrackingMatrix::create(camera->viewMatrix);

    // Place the gizmo in the view port by modifying its ortho camera x
    // and y limits
    double camWidth = 10;
    double camXOffs = -8;
    double camYOffs = -8;
    auto ortho = vsg::Orthographic::create(camXOffs, camXOffs + camWidth,                               // left, right
                                           camYOffs / aspectRatio, (camYOffs + camWidth) / aspectRatio, // bottom, top
                                           -1000, 1000);                                                // near, far

    auto gizmoCamera = vsg::Camera::create(
        ortho,
        viewMat,
        camera->viewportState);

    auto scene = vsg::Group::create();
    scene->addChild(createGizmo());

    auto view = vsg::View::create(gizmoCamera);
    view->addChild(vsg::createHeadlight());
    view->addChild(scene);
    return view;
}

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

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgviewgizmo";
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

    vsg::ref_ptr<vsg::Node> scenegraph;

    if (argc > 1)
    {
        vsg::Path filename = arguments[1];
        scenegraph = vsg::read_cast<vsg::Node>(filename, options);
    }

    if (!scenegraph)
    {
        std::cout << "Please specify a valid model on command line" << std::endl;
        return 1;
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

    // provide a custom RenderPass
    // window->setRenderPass(createRenderPass(window->getOrCreateDevice()));

    uint32_t width = window->extent2D().width;
    uint32_t height = window->extent2D().height;
    double aspectRatio = (double)width / height;

    auto renderGraph = vsg::RenderGraph::create(window);

    // create view
    auto camera = createCameraForScene(scenegraph, 0, 0, width, height);
    auto view = vsg::View::create(camera);
    view->addChild(vsg::createHeadlight());
    view->addChild(scenegraph);
    renderGraph->addChild(view);
    renderGraph->addChild(createAxesView(camera, aspectRatio));

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(vsg::Trackball::create(camera));

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
