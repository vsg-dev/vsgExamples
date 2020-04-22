#include <vsg/all.h>

#ifdef USE_VSGXCHANGE
#include <vsgXchange/ReaderWriter_all.h>
#include <vsgXchange/ShaderCompiler.h>
#endif

#include <iostream>
#include <chrono>
#include <thread>

class IntersectonHandler : public vsg::Inherit<vsg::Visitor, IntersectonHandler>
{
public:

    vsg::ref_ptr<vsg::Camera> camera;
    vsg::ref_ptr<vsg::Node> scenegraph;

    IntersectonHandler(vsg::ref_ptr<vsg::Camera> in_camera, vsg::ref_ptr<vsg::Node> in_scenegraph) :
        camera(in_camera),
        scenegraph(in_scenegraph) {}


    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (keyPress.keyBase=='i' && lastPointerEvent) interesection(*lastPointerEvent);
    }

    void apply(vsg::ButtonPressEvent& buttonPressEvent) override
    {
        lastPointerEvent = &buttonPressEvent;
        interesection(buttonPressEvent);
    }

    void apply(vsg::PointerEvent& pointerEvent) override
    {
        lastPointerEvent = &pointerEvent;
    }

    void interesection(vsg::PointerEvent& pointerEvent)
    {
        vsg::ref_ptr<vsg::Window> window = pointerEvent.window;

        VkExtent2D extents = window->extent2D();

        if (camera)
        {
            auto viewportState = camera->getViewportState();
            VkViewport viewport = viewportState->getViewport();

            vsg::vec2 ndc((static_cast<float>(pointerEvent.x)-viewport.x)/viewport.width, (static_cast<float>(pointerEvent.y)-viewport.y)/viewport.height);

            vsg::dvec3 ndc_near(ndc.x, ndc.y, viewport.minDepth);
            vsg::dvec3 ndc_far(ndc.x, ndc.y, viewport.maxDepth);

            std::cout<<"\n ndc_near = "<<ndc_near<<", ndc_far ="<<ndc_far<<std::endl;

            vsg::dmat4 projectionMatrix;
            camera->getProjectionMatrix()->get(projectionMatrix);

            vsg::dmat4 viewMatrix;
            camera->getViewMatrix()->get(viewMatrix);

            std::cout<<"projectionMatrix = "<<projectionMatrix<<std::endl;
            std::cout<<"viewMatrix = "<<viewMatrix<<std::endl;

            auto inv_projectionMatrix = vsg::inverse(projectionMatrix);
            auto inv_viewMatrix = vsg::inverse(viewMatrix);
            auto inv_projectionViewMatrix = vsg::inverse(projectionMatrix * viewMatrix);

            vsg::dvec3 eye_near = inv_projectionMatrix * ndc_near;
            vsg::dvec3 eye_far = inv_projectionMatrix * ndc_far;

            std::cout<<"eye_near = "<<eye_near<<std::endl;
            std::cout<<"eye_far = "<<eye_far<<std::endl;

            vsg::dvec3 world_near = inv_projectionViewMatrix * ndc_near;
            vsg::dvec3 world_far = inv_projectionViewMatrix * ndc_far;

            std::cout<<"world_near = "<<world_near<<std::endl;
            std::cout<<"world_far = "<<world_far<<std::endl;

            // just for testing purposes, assume we are working with a whole earth model.
            auto elipsoidModel = vsg::EllipsoidModel::create();
            auto latlongheight = elipsoidModel->convertECEFToLatLongHeight(world_near);
            std::cout<<"latlongheight lat = "<<vsg::degrees(latlongheight[0])<<", long = "<<vsg::degrees(latlongheight[1])<<", height "<<latlongheight[2]<<std::endl;

        }
    }

protected:
    vsg::ref_ptr<vsg::PointerEvent> lastPointerEvent;

};



int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsginteresction";

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    auto pointOfInterest = arguments.value(vsg::dvec3(0.0, 0.0, std::numeric_limits<double>::max()), "--poi");
    auto horizonMountainHeight = arguments.value(-1.0, "--hmh");
    auto useDatabasePager = arguments.read("--pager");
    auto maxPageLOD = arguments.value(-1, "--max-plod");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

#ifdef USE_VSGXCHANGE
    // add use of vsgXchange's support for reading and writing 3rd party file formats
    options->readerWriter = vsgXchange::ReaderWriter_all::create();
#endif

    using VsgNodes = std::vector<vsg::ref_ptr<vsg::Node>>;
    VsgNodes vsgNodes;

    vsg::Path path;

    // read any vsg files
    for (int i=1; i<argc; ++i)
    {
        vsg::Path filename = arguments[i];

        path = vsg::filePath(filename);

        auto loaded_scene = vsg::read_cast<vsg::Node>(filename, options);
        if (loaded_scene)
        {
            vsgNodes.push_back(loaded_scene);
            arguments.remove(i, 1);
            --i;
        }
    }

    // assign the vsg_scene from the loaded nodes
    vsg::ref_ptr<vsg::Node> vsg_scene;
    if (vsgNodes.size()>1)
    {
        auto vsg_group = vsg::Group::create();
        for(auto& subgraphs : vsgNodes)
        {
            vsg_group->addChild(subgraphs);
        }

        vsg_scene = vsg_group;
    }
    else if (vsgNodes.size()==1)
    {
        vsg_scene = vsgNodes.front();
    }


    if (!vsg_scene)
    {
        std::cout<<"No command graph created."<<std::endl;
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


    auto ellipsoidModel = vsg::EllipsoidModel::create();

    vsg::ref_ptr<vsg::LookAt> lookAt;

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;

    if (pointOfInterest[2] != std::numeric_limits<double>::max())
    {
        auto ellipsoidModel = vsg::EllipsoidModel::create();
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
        // set up the camera
        lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    }

    double nearFarRatio = 0.0001;
    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    if (horizonMountainHeight >= 0.0)
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

    viewer->addEventHandler(IntersectonHandler::create(camera, vsg_scene));

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
