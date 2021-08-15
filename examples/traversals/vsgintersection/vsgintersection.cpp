#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>


class IntersectionHandler : public vsg::Inherit<vsg::Visitor, IntersectionHandler>
{
public:
    vsg::ref_ptr<vsg::Builder> builder;
    vsg::ref_ptr<vsg::Options> options;
    vsg::ref_ptr<vsg::Camera> camera;
    vsg::ref_ptr<vsg::Group> scenegraph;
    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel;
    double scale = 1.0;
    bool verbose = false;

    IntersectionHandler(vsg::ref_ptr<vsg::Builder> in_builder, vsg::ref_ptr<vsg::Camera> in_camera, vsg::ref_ptr<vsg::Group> in_scenegraph, vsg::ref_ptr<vsg::EllipsoidModel> in_ellipsoidModel, double in_scale, vsg::ref_ptr<vsg::Options> in_options) :
        builder(in_builder),
        camera(in_camera),
        scenegraph(in_scenegraph),
        ellipsoidModel(in_ellipsoidModel),
        scale(in_scale),
        options(in_options)
    {
        if (scale > 10.0) scale = 10.0;
    }

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (lastPointerEvent)
        {
            interesection(*lastPointerEvent);
            if (!lastIntersection) return;

            vsg::GeometryInfo info;
            info.position = vsg::vec3(lastIntersection.worldIntersection);
            info.dx.set(scale, 0.0f, 0.0f);
            info.dy.set(0.0f, scale, 0.0f);
            info.dz.set(0.0f, 0.0f, scale);

            // info.image = vsg::read_cast<vsg::Data>("textures/lz.vsgb", options);

            if (keyPress.keyBase == 'b')
            {
                scenegraph->addChild(builder->createBox(info));
            }
            else if (keyPress.keyBase == 'q')
            {
                scenegraph->addChild(builder->createQuad(info));
            }
            else if (keyPress.keyBase == 'c')
            {
                scenegraph->addChild(builder->createCylinder(info));
            }
            else if (keyPress.keyBase == 'p')
            {
                scenegraph->addChild(builder->createCapsule(info));
            }
            else if (keyPress.keyBase == 's')
            {
                scenegraph->addChild(builder->createSphere(info));
            }
            else if (keyPress.keyBase == 'n')
            {
                scenegraph->addChild(builder->createCone(info));
            }

        }
    }

    void apply(vsg::ButtonPressEvent& buttonPressEvent) override
    {
        lastPointerEvent = &buttonPressEvent;

        if (buttonPressEvent.button == 1)
        {
            interesection(buttonPressEvent);
        }
    }

    void apply(vsg::PointerEvent& pointerEvent) override
    {
        lastPointerEvent = &pointerEvent;
    }

    void interesection(vsg::PointerEvent& pointerEvent)
    {
        auto intersector = vsg::LineSegmentIntersector::create(*camera, pointerEvent.x, pointerEvent.y);
        scenegraph->accept(*intersector);

        if (verbose) std::cout << "interesection(" << pointerEvent.x << ", " << pointerEvent.y << ") " << intersector->intersections.size() << ")" << std::endl;

        if (intersector->intersections.empty()) return;

        // sort the intersectors front to back
        std::sort(intersector->intersections.begin(), intersector->intersections.end(), [](auto lhs, auto rhs) { return lhs.ratio < rhs.ratio; });

        for (auto& intersection : intersector->intersections)
        {
            if (verbose) std::cout << "intersection = " << intersection.worldIntersection << " ";

            if (ellipsoidModel)
            {
                std::cout.precision(10);
                auto location = ellipsoidModel->convertECEFToLatLongAltitude(intersection.worldIntersection);
                if (verbose) std::cout << " lat = " << location[0] << ", long = " << location[1] << ", height = " << location[2];
            }

            if (lastIntersection)
            {
                if (verbose) std::cout << ", distance from previous intersection = " << vsg::length(intersection.worldIntersection - lastIntersection.worldIntersection);
            }

            if (verbose)
            {
                for (auto& node : intersection.nodePath)
                {
                    std::cout << ", " << node->className();
                }

                std::cout << ", Arrays[ ";
                for (auto& array : intersection.arrays)
                {
                    std::cout << array << " ";
                }
                std::cout << "] [";
                for (auto& ir : intersection.indexRatios)
                {
                    std::cout << "{" << ir.index << ", " << ir.ratio << "} ";
                }
                std::cout << "]";

                std::cout << std::endl;
            }
        }

        lastIntersection = intersector->intersections.front();
    }

protected:
    vsg::ref_ptr<vsg::PointerEvent> lastPointerEvent;
    vsg::LineSegmentIntersector::Intersection lastIntersection;
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->objectCache = vsg::ObjectCache::create();

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsginteresction";

    auto builder = vsg::Builder::create();
    builder->options = options;

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    auto pointOfInterest = arguments.value(vsg::dvec3(0.0, 0.0, std::numeric_limits<double>::max()), "--poi");
    auto horizonMountainHeight = arguments.value(0.0, "--hmh");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto scene = vsg::Group::create();
    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel;

    if (argc > 1)
    {
        vsg::Path filename = arguments[1];
        auto model = vsg::read_cast<vsg::Node>(filename, options);
        if (model)
        {
            scene->addChild(model);
            ellipsoidModel = model->getObject<vsg::EllipsoidModel>("EllipsoidModel");
        }
    }

    if (scene->children.empty())
    {
        vsg::GeometryInfo info;
        info.dx.set(100.0f, 0.0f, 0.0f);
        info.dy.set(0.0f, 100.0f, 0.0f);
        info.dz.set(0.0f, 0.0f, 100.0f);

        vsg::StateInfo stateInfo;
        vsg::Path textureFile("textures/lz.vsgb");
        stateInfo.image = vsg::read_cast<vsg::Data>(textureFile, options);

        scene->addChild(builder->createBox(info, stateInfo));
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

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

    if (pointOfInterest[2] != std::numeric_limits<double>::max())
    {
        if (ellipsoidModel)
        {
            auto ecef = ellipsoidModel->convertLatLongAltitudeToECEF({pointOfInterest[0], pointOfInterest[1], 0.0});
            auto ecef_normal = vsg::normalize(ecef);

            vsg::dvec3 centre = ecef;
            vsg::dvec3 eye = centre + ecef_normal * pointOfInterest[2];
            vsg::dvec3 up = vsg::normalize(vsg::cross(ecef_normal, vsg::cross(vsg::dvec3(0.0, 0.0, 1.0), ecef_normal)));

            // set up the camera
            lookAt = vsg::LookAt::create(eye, centre, up);
        }
        else
        {
            vsg::dvec3 eye = pointOfInterest;
            vsg::dvec3 centre = eye - vsg::dvec3(0.0, -radius * 3.5, 0.0);
            vsg::dvec3 up = vsg::dvec3(0.0, 0.0, 1.0);

            // set up the camera
            lookAt = vsg::LookAt::create(eye, centre, up);
        }
    }
    else
    {
        // set up the camera
        lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    }

    double nearFarRatio = 0.001;
    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    if (ellipsoidModel)
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // set up the compilation support in builder to allow us to interactively create and compile subgraphs from wtihin the IntersectionHandler
    builder->setup(window, camera->viewportState);

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(vsg::Trackball::create(camera));

    viewer->addEventHandler(IntersectionHandler::create(builder, camera, scene, ellipsoidModel, radius * 0.1, options));

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scene);
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
