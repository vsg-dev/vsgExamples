#include <vsg/all.h>

#ifdef USE_VSGXCHANGE
#include <vsgXchange/ReaderWriter_all.h>
#include <vsgXchange/ShaderCompiler.h>
#endif

#include <iostream>

#include "Builder.h"

class IntersectionHandler : public vsg::Inherit<vsg::Visitor, IntersectionHandler>
{
public:

    vsg::ref_ptr<vsg::Camera> camera;
    vsg::ref_ptr<vsg::Node> scenegraph;

    IntersectionHandler(vsg::ref_ptr<vsg::Camera> in_camera, vsg::ref_ptr<vsg::Node> in_scenegraph) :
        camera(in_camera),
        scenegraph(in_scenegraph) {}

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (keyPress.keyBase=='i' && lastPointerEvent) interesection(*lastPointerEvent);
    }

    void apply(vsg::ButtonPressEvent& buttonPressEvent) override
    {
        lastPointerEvent = &buttonPressEvent;

        if (buttonPressEvent.button==1)
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

        std::cout<<std::endl;
        for(auto& intersection : intersector->intersections)
        {
            std::cout<<"intersection = "<<intersection.worldIntersection<<" ";
            auto ellipsoidModel = scenegraph->getObject<vsg::EllipsoidModel>("EllipsoidModel");
            if (ellipsoidModel)
            {
                std::cout.precision(10);
                auto location = ellipsoidModel->convertECEFToLatLongHeight(intersection.worldIntersection);
                std::cout<<" lat = "<<vsg::degrees(location[0])<<", long = "<<vsg::degrees(location[1])<<", height = "<<location[2];
            }

            if (lastIntersection)
            {
                std::cout<<", distance from previous intersection = "<<vsg::length(intersection.worldIntersection - lastIntersection.worldIntersection);
            }

            for(auto& node : intersection.nodePath)
            {
                std::cout<<", "<<node->className();
            }

            std::cout<<std::endl;

            lastIntersection = intersection;
        }
    }

protected:
    vsg::ref_ptr<vsg::PointerEvent> lastPointerEvent;
    vsg::LineSegmentIntersector::Intersection lastIntersection;

};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsginteresction";

    auto builder = Builder::create();

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    auto pointOfInterest = arguments.value(vsg::dvec3(0.0, 0.0, std::numeric_limits<double>::max()), "--poi");
    auto horizonMountainHeight = arguments.value(0.0, "--hmh");
    auto useDatabasePager = arguments.read("--pager");
    auto maxPageLOD = arguments.value(-1, "--max-plod");

    if (arguments.read("--draw")) builder->geometryType = Builder::DRAW_COMMANDS;
    if (arguments.read("--draw-indexed")) builder->geometryType = Builder::DRAW_INDEXED_COMMANDS;
    if (arguments.read({"--geometry", "--geom"})) builder->geometryType = Builder::GEOMETRY;
    if (arguments.read("--vid")) builder->geometryType = Builder::VERTEX_INDEX_DRAW;

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

#ifdef USE_VSGXCHANGE
    // add use of vsgXchange's support for reading and writing 3rd party file formats
    options->readerWriter = vsgXchange::ReaderWriter_all::create();
#endif

    using VsgNodes = std::vector<vsg::ref_ptr<vsg::Node>>;
    VsgNodes vsgNodes;

    vsg::Path path;


    vsg::ref_ptr<vsg::Node> vsg_scene;
    if (argc>1)
    {
        vsg::Path filename = arguments[1];
        vsg_scene = vsg::read_cast<vsg::Node>(filename);
    }
    else
    {
        vsg_scene = builder->createCube();
    }

    if (!vsg_scene)
    {
        return 1;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(windowTraits));
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);

    vsg::ref_ptr<vsg::LookAt> lookAt;

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;

    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel(vsg_scene->getObject<vsg::EllipsoidModel>("EllipsoidModel"));

    if (pointOfInterest[2] != std::numeric_limits<double>::max())
    {
        if ( ellipsoidModel)
        {
            auto ecef = ellipsoidModel->convertLatLongHeightToECEF({vsg::radians(pointOfInterest[0]), vsg::radians(pointOfInterest[1]), 0.0});
            auto ecef_normal = vsg::normalize(ecef);

            vsg::dvec3 centre = ecef;
            vsg::dvec3 eye = centre + ecef_normal * pointOfInterest[2];
            vsg::dvec3 up = vsg::normalize( vsg::cross(ecef_normal, vsg::cross(vsg::dvec3(0.0, 0.0, 1.0), ecef_normal) ) );

            // set up the camera
            lookAt = vsg::LookAt::create(eye, centre, up);
        }
        else
        {
            vsg::dvec3 eye = pointOfInterest;
            vsg::dvec3 centre = eye - vsg::dvec3(0.0, -radius*3.5, 0.0);
            vsg::dvec3 up = vsg::dvec3(0.0, 0.0, 1.0);

            // set up the camera
            lookAt = vsg::LookAt::create(eye, centre, up);
        }
    }
    else
    {
        // set up the camera
        lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    }

    double nearFarRatio = 0.0001;
    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    if (ellipsoidModel)
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio*radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // set up database pager
    vsg::ref_ptr<vsg::DatabasePager> databasePager;
    if (useDatabasePager)
    {
        databasePager = vsg::DatabasePager::create();
        if (maxPageLOD>=0) databasePager->targetMaxNumPagedLODWithHighResSubgraphs = maxPageLOD;
    }

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(vsg::Trackball::create(camera));

    viewer->addEventHandler(IntersectionHandler::create(camera, vsg_scene));

    auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph}, databasePager);

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
