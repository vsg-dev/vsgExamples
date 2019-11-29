#include <vsg/all.h>

#include <iostream>
#include <chrono>
#include <thread>

#include "../vsgviewer/AnimationPath.h"

#include "ExperimentalViewer.h"

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto windowTraits = vsg::Window::Traits::create();
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
    auto numFrames = arguments.value(-1, "-f");
    auto pathFilename = arguments.value(std::string(),"-p");
    auto loadLevels = arguments.value(0, "--load-levels");
    auto horizonMountainHeight = arguments.value(-1.0, "--hmh");
    auto useDatabasePager = arguments.read("--pager");
    auto maxPageLOD = arguments.value(-1, "--max-plod");
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);
    auto useNewViewer = arguments.read("--new");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    using VsgNodes = std::vector<vsg::ref_ptr<vsg::Node>>;
    VsgNodes vsgNodes;

    vsg::Path path;

    // read any vsg files
    for (int i=1; i<argc; ++i)
    {
        vsg::Path filename = arguments[i];

        path = vsg::filePath(filename);

        auto loaded_scene = vsg::read_cast<vsg::Node>(filename);
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


    // if required pre load specific number of PagedLOD levels.
    if (loadLevels > 0)
    {
        struct LoadTiles : public vsg::Visitor
        {
            LoadTiles(int in_loadLevels, const vsg::Path& in_path) :
                loadLevels(in_loadLevels),
                path(in_path) {}

            int loadLevels = 0;
            int level = 0;
            unsigned int numTiles = 0;
            vsg::Path path;

            void apply(vsg::Node& node) override
            {
                node.traverse(*this);
            }

            void apply(vsg::PagedLOD& plod) override
            {
                if (level < loadLevels && !plod.filename.empty())
                {
                    plod.getChild(0).node = vsg::read_cast<vsg::Node>(plod.filename) ;

                    ++numTiles;

                    ++level;
                        plod.traverse(*this);
                    --level;
                }

            }
        } loadTiles(loadLevels, path);

        vsg_scene->accept(loadTiles);

        std::cout<<"No. of tiles loaed "<<loadTiles.numTiles<<std::endl;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::ExperimentalViewer::create();

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(windowTraits));
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);


    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;
    double nearFarRatio = 0.0001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    if (horizonMountainHeight >= 0.0)
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, vsg::EllipsoidModel::create(), 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
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

    if (pathFilename.empty())
    {
        viewer->addEventHandler(vsg::Trackball::create(camera));
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

        viewer->addEventHandler(vsg::AnimationPathHandler::create(camera, animationPath, viewer->start_point()));
    }

    if (useNewViewer)
    {
        // set up the render graph for viewport & scene
        auto renderGraph = vsg::RenderGraph::create();
        renderGraph->addChild(vsg_scene);

        renderGraph->camera = camera;
        renderGraph->window = window;

        renderGraph->renderArea.offset = {0, 0};
        renderGraph->renderArea.extent = window->extent2D();

        renderGraph->clearValues.resize(2);
        renderGraph->clearValues[0].color = VkClearColorValue{0.2f, 0.4f, 0.5f, 1.0f}; // window->clearColor()
        renderGraph->clearValues[1].depthStencil = VkClearDepthStencilValue{1.0f, 0};

        auto device = window->device();
        auto physicalDevice = window->physicalDevice();

        // set up commandGraph to rendering viewport
        auto commandGraph = vsg::CommandGraph::create(device, physicalDevice->getGraphicsFamily());
        commandGraph->addChild(renderGraph);
        for(size_t i = 0; i < window->numFrames(); ++i)
        {
            commandGraph->commandBuffers.emplace_back(window->commandBuffer(i));
        }

        auto renderFinishedSemaphore = vsg::Semaphore::create(window->device());

        // set up Submission with CommandBuffer and signals
        auto recordAndSubmitTask = vsg::RecordAndSubmitTask::create();
        recordAndSubmitTask->commandGraphs.emplace_back(commandGraph);
        recordAndSubmitTask->signalSemaphores.emplace_back(renderFinishedSemaphore);
        recordAndSubmitTask->databasePager = databasePager;
        recordAndSubmitTask->windows = viewer->windows();
        recordAndSubmitTask->queue = window->device()->getQueue(window->physicalDevice()->getGraphicsFamily());
        viewer->recordAndSubmitTasks.emplace_back(recordAndSubmitTask);

        auto presentation = vsg::Presentation::create();
        presentation->waitSemaphores.emplace_back(renderFinishedSemaphore);
        presentation->windows = viewer->windows();
        presentation->queue = window->device()->getQueue(window->physicalDevice()->getPresentFamily());
        viewer->presentation = presentation;

        viewer->compile();

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames<0 || (numFrames--)>0))
        {
            vsg::ref_ptr<vsg::FrameStamp> frameStamp(viewer->getFrameStamp());

            if (databasePager) databasePager->updateSceneGraph(viewer->getFrameStamp());

            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();


            for(auto& recordAndSubmitTask : viewer->recordAndSubmitTasks)
            {
                recordAndSubmitTask->submit(frameStamp);
            }

            viewer->presentation->present();

            viewer->presentation->queue->waitIdle();
        }

    }
    else
    {
        // add a GraphicsStage to the Window to do dispatch of the command graph to the command buffer(s)
        auto graphicsStage = vsg::GraphicsStage::create(vsg_scene, camera);
        graphicsStage->databasePager = databasePager;
        window->addStage(graphicsStage);

        // compile the Vulkan objects

        viewer->compile();


        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames<0 || (numFrames--)>0))
        {
            if (databasePager) databasePager->updateSceneGraph(viewer->getFrameStamp());

            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->populateNextFrame();

            viewer->submitNextFrame();
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
