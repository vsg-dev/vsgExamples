#include <vsg/all.h>

#include <iostream>
#include <chrono>
#include <thread>

#include "AnimationPath.h"

// TODO
// 1. Each window to get it's own slave Camera -> need to sync the view/projection matrix + offset
//    a. RelativeViewMatrix - multiple by an offset matrix DONE
//    a. RelativeProjectionMatrix - multiple by an offset matrix  DONE
// 2. Each logical Device to have it's own DeviceID : DONE
//
// 3. All Vulkan objects to be accessed via a DeviceID and to use compile time configured buffering
// 4. Compile traversal to run for each Logical device/DeviceID - one Context per Logical Device?
// 5. DatabasePager to handle multiple logical Device

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgviewer";

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--FIFO")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (arguments.read("--FIFO_RELAXED")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    if (arguments.read("--MAILBOX")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    if (arguments.read({"-t", "--test"})) { windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; windowTraits->fullscreen = true; }
    if (arguments.read({"--st", "--small-test"})) { windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; windowTraits->width = 192, windowTraits->height = 108; windowTraits->decoration = false; }
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    if (arguments.read({"--no-frame", "--nf"})) windowTraits->decoration = false;
    if (arguments.read("--or")) windowTraits->overrideRedirect = true;
    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    auto numScreens = arguments.value<int>(1, {"--screens", "--ns"});
    auto numFrames = arguments.value(-1, "-f");
    auto pathFilename = arguments.value(std::string(),"-p");
    auto loadLevels = arguments.value(0, "--load-levels");
    auto horizonMountainHeight = arguments.value(-1.0, "--hmh");
    auto useDatabasePager = arguments.read("--pager");
    auto maxPageLOD = arguments.value(-1, "--max-plod");
    auto powerWall = arguments.read({"--power-wall","--pw"});

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    if (argc<=1)
    {
        std::cout<<"Please specify a model to load on command line."<<std::endl;
        return 1;
    }

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    vsg::Path filename = arguments[1];
    vsg::Path path = vsg::filePath(filename);

    auto vsg_scene = vsg::read_cast<vsg::Node>(filename);
    if (!vsg_scene)
    {
        std::cout<<"Unable to load model "<<filename<<std::endl;
        return 1;
    }

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;
    double nearFarRatio = 0.0001;

    // create master camera
    auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    double aspectRatio = windowTraits->fullscreen ? (1920.0/1080.0) : static_cast<double>(windowTraits->width)/static_cast<double>(windowTraits->height);
    if (horizonMountainHeight >= 0.0)
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, vsg::EllipsoidModel::create(), 30.0, aspectRatio, nearFarRatio, horizonMountainHeight);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, aspectRatio, nearFarRatio*radius, radius * 4.5);
    }

    auto master_camera = vsg::Camera::create(perspective, lookAt);

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    if (pathFilename.empty())
    {
        viewer->addEventHandler(vsg::Trackball::create(master_camera));
    }
    else
    {
        std::ifstream in(pathFilename);
        if (!in)
        {
            std::cout << "AnimationPat: Could not open animation path file \"" << pathFilename << "\".\n";
            return 1;
        }

        vsg::ref_ptr<vsg::AnimationPath> animationPath(new vsg::AnimationPath);
        animationPath->read(in);

        viewer->addEventHandler(vsg::AnimationPathHandler::create(master_camera, animationPath, viewer->start_point()));
    }


    // set up database pager
    vsg::ref_ptr<vsg::DatabasePager> databasePager;
    if (useDatabasePager)
    {
        databasePager = vsg::DatabasePager::create();
        if (maxPageLOD>=0) databasePager->targetMaxNumPagedLODWithHighResSubgraphs = maxPageLOD;
    }

    if (windowTraits->screenNum<0) windowTraits->screenNum = 0;
    for(int i=0; i<numScreens; ++i)
    {
        // create a local copy of the main WindowTraits
        vsg::ref_ptr<vsg::WindowTraits> local_windowTraits = vsg::WindowTraits::create(*windowTraits);

        // set the screenNum relative to the base one
        local_windowTraits->screenNum = windowTraits->screenNum + i;

        vsg::ref_ptr<vsg::Window> window(vsg::Window::create(local_windowTraits));
        if (!window)
        {
            std::cout<<"Could not create window."<<std::endl;
            return 1;
        }

        auto local_scene = vsg::read_cast<vsg::Node>(filename);
        vsg::ref_ptr<vsg::Camera> camera;
        if (powerWall)
        {
            // assume a power wall layout
            auto relative_perspective = vsg::RelativeProjection::create(perspective, vsg::translate(double(numScreens-1) - 2.0*double(i), 0.0, 0.0));
            camera = vsg::Camera::create(relative_perspective, lookAt, vsg::ViewportState::create(window->extent2D()));
        }
        else
        {
            // assume monitor are rotate around the viewer
            double fovY = 30.0;
            double fovX = atan(tan(vsg::radians(fovY) * 0.5) * aspectRatio) * 2.0;
            double angle = fovX*(double(i)-double(numScreens-1)/2.0);
            auto relative_view = vsg::RelativeView::create(lookAt, vsg::rotate(angle, 0.0, 1.0, 0.0));
            camera = vsg::Camera::create(perspective, relative_view, vsg::ViewportState::create(window->extent2D()));
        }

        viewer->assignRecordAndSubmitTaskAndPresentation({vsg::createCommandGraphForView(window, camera, local_scene)}, databasePager);
        viewer->addWindow(window);
    }

    viewer->compile();

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames<0 || (numFrames--)>0))
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
