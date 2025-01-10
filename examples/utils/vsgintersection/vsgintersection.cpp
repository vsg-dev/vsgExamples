#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>

class IntersectionHandler : public vsg::Inherit<vsg::Visitor, IntersectionHandler>
{
public:
    vsg::GeometryInfo geom;
    vsg::StateInfo state;

    vsg::ref_ptr<vsg::Builder> builder;
    vsg::ref_ptr<vsg::Options> options;
    vsg::ref_ptr<vsg::Camera> camera;
    vsg::ref_ptr<vsg::Group> scenegraph;
    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel;
    double scale = 1.0;
    bool verbose = true;

    IntersectionHandler(vsg::ref_ptr<vsg::Builder> in_builder, vsg::ref_ptr<vsg::Camera> in_camera, vsg::ref_ptr<vsg::Group> in_scenegraph, vsg::ref_ptr<vsg::EllipsoidModel> in_ellipsoidModel, double in_scale, vsg::ref_ptr<vsg::Options> in_options) :
        builder(in_builder),
        options(in_options),
        camera(in_camera),
        scenegraph(in_scenegraph),
        ellipsoidModel(in_ellipsoidModel),
        scale(in_scale)
    {
        geom.cullNode = true;
        builder->verbose = verbose;
        if (scale > 10.0) scale = 10.0;
    }

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (lastPointerEvent)
        {
            intersection_LineSegmentIntersector(*lastPointerEvent);
            if (!lastIntersection) return;

            vsg::info("keyPress.keyModifier = ", keyPress.keyModifier, " keyPress.keyBase = ", keyPress.keyBase);

            geom.dx.set(static_cast<float>(scale), 0.0f, 0.0f);
            geom.dy.set(0.0f, static_cast<float>(scale), 0.0f);
            geom.dz.set(0.0f, 0.0f, static_cast<float>(scale));

            if (keyPress.keyModifier & vsg::MODKEY_Control)
            {
                // when we press the ctrl key we want to enable billboarding of the created shapes
                state.billboard = true;

                // when billboarding the position is the pivot point in local coordinates
                geom.position.set(0.0f, 0.0f, 0.0f);

                // the position is set by positions data, in this case just one position so use a vec4Value, but if needed we can use an array of positions
                auto pos = vsg::vec3(lastIntersection->worldIntersection);
                geom.positions = vsg::vec4Value::create(vsg::vec4(pos.x, pos.y, pos.z, static_cast<float>(scale * 5.0))); // x,y,z and scaleDistance
            }
            else
            {
                geom.position = vsg::vec3(lastIntersection->worldIntersection);
            }

            if (keyPress.keyBase == 'b')
            {
                scenegraph->addChild(builder->createBox(geom, state));
            }
            else if (keyPress.keyBase == 'q')
            {
                scenegraph->addChild(builder->createQuad(geom, state));
            }
            else if (keyPress.keyBase == 'c')
            {
                scenegraph->addChild(builder->createCylinder(geom, state));
            }
            else if (keyPress.keyBase == 'p')
            {
                scenegraph->addChild(builder->createCapsule(geom, state));
            }
            else if (keyPress.keyBase == 's')
            {
                scenegraph->addChild(builder->createSphere(geom, state));
            }
            else if (keyPress.keyBase == 'n')
            {
                scenegraph->addChild(builder->createCone(geom, state));
            }
        }

        if (state.billboard)
        {
            // switch off billboarding so other shapes aren't affected.
            state.billboard = false;
            geom.positions = {};
        }

        if (keyPress.keyBase == 'o')
        {
            vsg::write(scenegraph, "builder.vsgt");
        }
    }

    void apply(vsg::ButtonPressEvent& buttonPressEvent) override
    {
        lastPointerEvent = &buttonPressEvent;

        if (buttonPressEvent.button == 1)
        {
            intersection_LineSegmentIntersector(buttonPressEvent);
        }
        else if (buttonPressEvent.button == 2)
        {
            intersection_PolytopeIntersector(buttonPressEvent);
        }
    }

    void apply(vsg::PointerEvent& pointerEvent) override
    {
        lastPointerEvent = &pointerEvent;
    }

    void intersection_LineSegmentIntersector(vsg::PointerEvent& pointerEvent)
    {
        auto intersector = vsg::LineSegmentIntersector::create(*camera, pointerEvent.x, pointerEvent.y);
        scenegraph->accept(*intersector);

        if (verbose) std::cout << "intersection_LineSegmentIntersector(" << pointerEvent.x << ", " << pointerEvent.y << ") " << intersector->intersections.size() << ")" << std::endl;

        if (intersector->intersections.empty()) return;

        // sort the intersections front to back
        std::sort(intersector->intersections.begin(), intersector->intersections.end(), [](auto& lhs, auto& rhs) { return lhs->ratio < rhs->ratio; });

        for (auto& intersection : intersector->intersections)
        {
            if (verbose) std::cout << "intersection = world(" << intersection->worldIntersection << "), instanceIndex " << intersection->instanceIndex;

            if (ellipsoidModel)
            {
                std::cout.precision(10);
                auto location = ellipsoidModel->convertECEFToLatLongAltitude(intersection->worldIntersection);
                if (verbose) std::cout << " lat = " << location[0] << ", long = " << location[1] << ", height = " << location[2];
            }

            if (lastIntersection)
            {
                if (verbose) std::cout << ", distance from previous intersection = " << vsg::length(intersection->worldIntersection - lastIntersection->worldIntersection);
            }

            if (verbose)
            {
                std::string name;
                for (auto& node : intersection->nodePath)
                {
                    std::cout << ", " << node->className();
                    if (node->getValue("name", name)) std::cout << ":name=" << name;
                }

                std::cout << ", Arrays[ ";
                for (auto& array : intersection->arrays)
                {
                    std::cout << array << " ";
                }
                std::cout << "] [";
                for (auto& ir : intersection->indexRatios)
                {
                    std::cout << "{" << ir.index << ", " << ir.ratio << "} ";
                }
                std::cout << "]";

                std::cout << std::endl;
            }
        }

        lastIntersection = intersector->intersections.front();
    }

    void intersection_PolytopeIntersector(vsg::PointerEvent& pointerEvent)
    {
        double size = 5.0;
        double xMin = static_cast<double>(pointerEvent.x) - size;
        double xMax = static_cast<double>(pointerEvent.x) + size;
        double yMin = static_cast<double>(pointerEvent.y) - size;
        double yMax = static_cast<double>(pointerEvent.y) + size;

        auto intersector = vsg::PolytopeIntersector::create(*camera, xMin, yMin, xMax, yMax);
        scenegraph->accept(*intersector);

        if (verbose) std::cout << "intersection_PolytopeIntersector(" << pointerEvent.x << ", " << pointerEvent.y << ") " << intersector->intersections.size() << ")" << std::endl;

        if (intersector->intersections.empty()) return;

        for (auto& intersection : intersector->intersections)
        {
            if (verbose) std::cout << "intersection = world(" << intersection->worldIntersection << "), instanceIndex " << intersection->instanceIndex;

            if (ellipsoidModel)
            {
                std::cout.precision(10);
                auto location = ellipsoidModel->convertECEFToLatLongAltitude(intersection->worldIntersection);
                if (verbose) std::cout << " lat = " << location[0] << ", long = " << location[1] << ", height = " << location[2];
            }

            if (lastIntersection)
            {
                if (verbose) std::cout << ", distance from previous intersection = " << vsg::length(intersection->worldIntersection - lastIntersection->worldIntersection);
            }

            if (verbose)
            {
                std::string name;
                for (auto& node : intersection->nodePath)
                {
                    std::cout << ", " << node->className();
                    if (node->getValue("name", name)) std::cout << ":name=" << name;
                }

                std::cout << ", Arrays[ ";
                for (auto& array : intersection->arrays)
                {
                    std::cout << array << " ";
                }
                std::cout << "] [";
                for (auto& index : intersection->indices)
                {
                    std::cout << index << " ";
                }
                std::cout << "]";

                std::cout << std::endl;
            }
        }
    }

protected:
    vsg::ref_ptr<vsg::PointerEvent> lastPointerEvent;
    vsg::ref_ptr<vsg::LineSegmentIntersector::Intersection> lastIntersection;
};

int main(int argc, char** argv)
{
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgintersection";

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
    vsg::Path textureFile = arguments.value<std::string>("", "-t");

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
            ellipsoidModel = model->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
        }
    }

    vsg::StateInfo stateInfo;

    if (textureFile)
    {
        stateInfo.image = vsg::read_cast<vsg::Data>(textureFile, options);
    }

    if (scene->children.empty())
    {
        vsg::GeometryInfo info;
        info.cullNode = true;
        info.dx.set(100.0f, 0.0f, 0.0f);
        info.dy.set(0.0f, 100.0f, 0.0f);
        info.dz.set(0.0f, 0.0f, 100.0f);
        scene->addChild(builder->createBox(info, stateInfo));
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

    vsg::ref_ptr<vsg::LookAt> lookAt;

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scene->accept(computeBounds);
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
            lookAt = vsg::LookAt::create(eye, ecef, up);
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
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
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

    auto commandGraph = createCommandGraphForView(window, camera, scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

    auto intersectionHandler = IntersectionHandler::create(builder, camera, scene, ellipsoidModel, radius * 0.1, options);
    intersectionHandler->state = stateInfo;
    viewer->addEventHandler(intersectionHandler);

    // assign a CompileTraversal to the Builder that will compile for all the views assigned to the viewer,
    // must be done after viewer.assignRecordAndSubmitTaskAndPresentation(...);
    builder->assignCompileTraversal(vsg::CompileTraversal::create(*viewer));

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
