#include <vsg/all.h>
#include <iostream>

#include "Text.h"

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    auto debugLayer = arguments.read({"--debug","-d"});
    auto apiDumpLayer = arguments.read({"--api","-a"});
    auto [width, height] = arguments.value(std::pair<uint32_t, uint32_t>(800, 600), {"--window", "-w"});
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    auto scenegraph = vsg::Group::create();
    auto transform = vsg::MatrixTransform::create(); // there must be a transform, guessing to populate the push constant
    scenegraph->addChild(transform);

    // create text group
    auto textgroup = vsg::TextGroup::create(searchPaths);
    transform->addChild(textgroup);

    auto font = vsg::Font::create("roboto", searchPaths);

    auto text = vsg::Text::create(font, textgroup);
    textgroup->addChild(text);

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(width, height, debugLayer, apiDumpLayer));
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // camera related details
    auto viewport = vsg::ViewportState::create(VkExtent2D{width, height});
    vsg::ref_ptr<vsg::Perspective> perspective(new vsg::Perspective(60.0, static_cast<double>(width) / static_cast<double>(height), 0.1, 10.0));
    //vsg::ref_ptr<vsg::Orthographic> perspective(new vsg::Orthographic(0.0f, 1.0f, 0.0f, 1.0f, 0.1, 10.0));
    vsg::ref_ptr<vsg::LookAt> lookAt(new vsg::LookAt(vsg::dvec3(0.0, 0.0, 10.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 1.0, 0.0)));
    vsg::ref_ptr<vsg::Camera> camera(new vsg::Camera(perspective, lookAt, viewport));

    // add a GraphicsStage to the Window to do dispatch of the command graph to the commnad buffer(s)
    window->addStage(vsg::GraphicsStage::create(scenegraph, camera));

    // compile the Vulkan objects
    viewer->compile();

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    // main frame loop
    while (viewer->active())
    {
        // poll events and advance frame counters
        viewer->advance();

        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        if (viewer->aquireNextFrame())
        {
            // animate the transform
            float time = std::chrono::duration<float, std::chrono::seconds::period>(viewer->getFrameStamp()->time - viewer->start_point()).count();
            //transform->setMatrix(vsg::rotate(time * vsg::radians(90.0f), vsg::vec3(0.0f, 0.0, 1.0f)));

            viewer->populateNextFrame();

            viewer->submitNextFrame();
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
