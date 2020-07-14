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

    vsg::ref_ptr<Builder> builder;
    vsg::ref_ptr<vsg::Camera> camera;
    vsg::ref_ptr<vsg::Group> scenegraph;
    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel;

    IntersectionHandler(vsg::ref_ptr<Builder> in_builder, vsg::ref_ptr<vsg::Camera> in_camera, vsg::ref_ptr<vsg::Group> in_scenegraph, vsg::ref_ptr<vsg::EllipsoidModel> in_ellipsoidModel) :
        builder(in_builder),
        camera(in_camera),
        scenegraph(in_scenegraph),
        ellipsoidModel(in_ellipsoidModel)
    {
    }

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (keyPress.keyBase=='i' && lastPointerEvent)
        {
            interesection(*lastPointerEvent);

            if (lastIntersection)
            {
                std::cout<<"inserting at = "<<lastIntersection.worldIntersection<<" ";
                GeometryInfo info;
                info.dimensions.set(10.0f, 10.0f, 10.0f);
                info.position = vsg::vec3(lastIntersection.worldIntersection) - info.dimensions*0.5f;
                scenegraph->addChild( builder->createBox(info) );
            }
        }
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

        std::cout<<"interesection("<<pointerEvent.x << ", "<<pointerEvent.y<<") "<<intersector->intersections.size()<<")"<<std::endl;

        if (intersector->intersections.empty()) return;

        // sort the intersectors front to back
        std::sort(intersector->intersections.begin(), intersector->intersections.end(), [](auto lhs, auto rhs) { return lhs.ratio <rhs.ratio; });

        std::cout<<std::endl;
        for(auto& intersection : intersector->intersections)
        {
            std::cout<<"intersection = "<<intersection.worldIntersection<<" ";
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

            std::cout<<", Arrays[ ";
            for(auto& array : intersection.arrays)
            {
                std::cout<<array<<" ";
            }
            std::cout<<"] [";
            for(auto& ir : intersection.indexRatios)
            {
                std::cout<<"{"<<ir.index<<", "<<ir.ratio<<"} ";
            }
            std::cout<<"]";

            std::cout<<std::endl;
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

    if (arguments.read("--draw")) builder->geometryType = Builder::DRAW_COMMANDS;
    if (arguments.read("--draw-indexed")) builder->geometryType = Builder::DRAW_INDEXED_COMMANDS;
    if (arguments.read({"--geometry", "--geom"})) builder->geometryType = Builder::GEOMETRY;
    if (arguments.read("--vid")) builder->geometryType = Builder::VERTEX_INDEX_DRAW;

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

#ifdef USE_VSGXCHANGE
    // add use of vsgXchange's support for reading and writing 3rd party file formats
    options->readerWriter = vsgXchange::ReaderWriter_all::create();
#endif

    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    auto scene = vsg::Group::create();
    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel;

    if (argc>1)
    {
        vsg::Path filename = arguments[1];
        auto model = vsg::read_cast<vsg::Node>(filename);
        if (model)
        {
            scene->addChild(model);
            ellipsoidModel = model->getObject<vsg::EllipsoidModel>("EllipsoidModel");
        }
    }

    if (scene->getNumChildren()==0)
    {
        GeometryInfo info;
        info.dimensions.set(100.f, 100.0f, 100.0f);

        vsg::Path textureFile("textures/lz.vsgb");
        info.image = vsg::read_cast<vsg::Data>(vsg::findFile(textureFile, searchPaths));

        scene->addChild(builder->createBox(info));
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);

    vsg::ref_ptr<vsg::LookAt> lookAt;

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;

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

    double nearFarRatio = 0.001;
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

    // set up the compilation support in builder to allow us to interactively create and compile subgraphs from wtihin the IntersectionHandler
    builder->setup(window, camera->getViewportState());

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(vsg::Trackball::create(camera));

    viewer->addEventHandler(IntersectionHandler::create(builder, camera, scene, ellipsoidModel));

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
