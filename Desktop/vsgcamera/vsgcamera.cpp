#include <vsg/all.h>

#include <vsg/viewer/Camera.h>
#include <vsg/ui/PrintEvents.h>

#include <vsg/viewer/CloseHandler.h>

#include <iostream>
#include <chrono>

#include "Trackball.h"
#include "CreateSceneData.h"
#include "GraphicsNodes.h"



int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    auto debugLayer = arguments.read({"--debug","-d"});
    auto apiDumpLayer = arguments.read({"--api","-a"});
    auto numFrames = arguments.value(-1, "-f");
    auto printFrameRate = arguments.read("--fr");
    auto numWindows = arguments.value(1, "--num-windows");
    auto [width, height] = arguments.value(std::pair<uint32_t, uint32_t>(800, 600), {"--window", "-w"});
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");


    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(width, height, debugLayer, apiDumpLayer));
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);

    for(int i=1; i<numWindows; ++i)
    {
        vsg::ref_ptr<vsg::Window> new_window(vsg::Window::create(width, height, debugLayer, apiDumpLayer, window));
        viewer->addWindow( new_window );
    }

    // create high level Vulkan objects associated the main window
    vsg::ref_ptr<vsg::PhysicalDevice> physicalDevice(window->physicalDevice());
    vsg::ref_ptr<vsg::Device> device(window->device());
    vsg::ref_ptr<vsg::Surface> surface(window->surface());
    vsg::ref_ptr<vsg::RenderPass> renderPass(window->renderPass());


    VkQueue graphicsQueue = device->getQueue(physicalDevice->getGraphicsFamily());
    VkQueue presentQueue = device->getQueue(physicalDevice->getPresentFamily());
    if (!graphicsQueue || !presentQueue)
    {
        std::cout<<"Required graphics/present queue not available!"<<std::endl;
        return 1;
    }

    vsg::ref_ptr<vsg::CommandPool> commandPool = vsg::CommandPool::create(device, physicalDevice->getGraphicsFamily());

    // camera related state
    vsg::ref_ptr<vsg::mat4Value> projMatrix(new vsg::mat4Value);
    vsg::ref_ptr<vsg::mat4Value> viewMatrix(new vsg::mat4Value);
    auto viewport = vsg::ViewportState::create(VkExtent2D{width, height});

    vsg::ref_ptr<vsg::Perspective> perspective(new vsg::Perspective(60.0, static_cast<double>(width) / static_cast<double>(height), 0.1, 10.0));
    vsg::ref_ptr<vsg::LookAt> lookAt(new vsg::LookAt(vsg::dvec3(1.0, 1.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0)));
    vsg::ref_ptr<vsg::Camera> camera(new vsg::Camera(perspective, lookAt, viewport));


    // create the scene/command graph
    vsg::ref_ptr<vsg::Node> commandGraph  = createSceneData(searchPaths);

    // compile the Vulkan objects
    vsg::CompileTraversal compile;
    compile.context.device = device;
    compile.context.commandPool = commandPool;
    compile.context.renderPass = renderPass;
    compile.context.viewport = viewport;
    compile.context.graphicsQueue = graphicsQueue;
    compile.context.projMatrix = projMatrix;
    compile.context.viewMatrix = viewMatrix;

    commandGraph->accept(compile);

    //
    // end of initialize vulkan
    //
    /////////////////////////////////////////////////////////////////////

    auto startTime =std::chrono::steady_clock::now();

    for (auto& win : viewer->windows())
    {
        // add a GraphicsStage tp the Window to do dispatch of the command graph to the commnad buffer(s)
        win->addStage(vsg::GraphicsStage::create(commandGraph));
        win->populateCommandBuffers();
    }


    // assign a Trackball and CloseHandler to the Viewer to respond to events
    auto trackball = vsg::Trackball::create(camera);
    viewer->addEventHandlers({trackball, vsg::CloseHandler::create(viewer)});

    bool windowResized = false;
    float time = 0.0f;

    while (viewer->active() && (numFrames<0 || (numFrames--)>0))
    {
        // poll events and advance frame counters
        viewer->advance();

        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        float previousTime = time;
        time = std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::steady_clock::now()-viewer->start_point()).count();
        if (printFrameRate) std::cout<<"time = "<<time<<" fps="<<1.0/(time-previousTime)<<std::endl;

        camera->getProjectionMatrix()->get((*projMatrix));
        camera->getViewMatrix()->get((*viewMatrix));

        for (auto& win : viewer->windows())
        {
            // we need to regenerate the CommandBuffer so that the PushConstants get called with the new values.
            win->populateCommandBuffers();
        }

        if (window->resized()) windowResized = true;

        viewer->submitFrame();

        if (windowResized)
        {
            windowResized = false;

            auto windowExtent = window->extent2D();

            vsg::UpdatePipeline updatePipeline(camera->getViewportState());

            viewport->getViewport().width = static_cast<float>(windowExtent.width);
            viewport->getViewport().height = static_cast<float>(windowExtent.height);
            viewport->getScissor().extent = windowExtent;

            commandGraph->accept(updatePipeline);

            perspective->aspectRatio = static_cast<double>(windowExtent.width) / static_cast<double>(windowExtent.height);

            std::cout<<"window aspect ratio = "<<perspective->aspectRatio<<std::endl;
        }
    }


    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
