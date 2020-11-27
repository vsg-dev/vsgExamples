#include <vsg/all.h>
#include <iostream>

class InputHandler : public vsg::Inherit<vsg::Visitor, InputHandler>
{
public:

    InputHandler()
    {
    }

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        std::cout<<"vsg::KeyPressEvent keyBase = "<<keyPress.keyBase<<", keyModified = "<<keyPress.keyModified<<std::endl;
    }

    void apply(vsg::KeyReleaseEvent& keyRelease) override
    {
        std::cout<<"vsg::KeyReleaseEvent keyBase = "<<keyRelease.keyBase<<", keyModified = "<<keyRelease.keyModified<<std::endl;
    }

    void apply(vsg::MoveEvent& moveEvent) override
    {
        std::cout<<"vsg::MoveEvent x= "<<moveEvent.x<<", y= "<<moveEvent.y<<", mask = "<<moveEvent.mask<<std::endl;
    }

    void apply(vsg::ButtonPressEvent& buttonPressEvent) override
    {
        std::cout<<"vsg::ButtonPressEvent x= "<<buttonPressEvent.x<<", y= "<<buttonPressEvent.y<<", mask = "<<buttonPressEvent.mask<<", button = "<<buttonPressEvent.button<<std::endl;
    }

    void apply(vsg::ButtonReleaseEvent& buttonReleaseEvent) override
    {
        std::cout<<"vsg::ButtonPressEvent x= "<<buttonReleaseEvent.x<<", y= "<<buttonReleaseEvent.y<<", mask = "<<buttonReleaseEvent.mask<<", button = "<<buttonReleaseEvent.button<<std::endl;
    }

protected:

};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create("vsg input");
    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);
    auto font_filename = arguments.value(std::string("fonts/times.vsgb"), "--font");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    auto options = vsg::Options::create();
    options->paths = searchPaths;


    auto scenegraph = vsg::Group::create();

    auto font = vsg::read_cast<vsg::Font>(font_filename, options);

    auto dynamic_text_label = vsg::stringValue::create("Please press keys and move/click mouse buttons.\n\nWatch the console output to see the event\ninformation displayed.");
    auto dynamic_text_layout = vsg::LeftAlignment::create();
    auto dynamic_text = vsg::Text::create();
    {
//        dynamic_text->technique = vsg::GpuLayoutTechnique::create();

        dynamic_text_layout->position = vsg::vec3(0.0, 0.0, 3.0);
        dynamic_text_layout->horizontal = vsg::vec3(1.0, 0.0, 0.0);
        dynamic_text_layout->vertical = vsg::vec3(0.0, 0.0, 1.0);
        dynamic_text_layout->color = vsg::vec4(1.0, 0.9, 1.0, 1.0);
        dynamic_text_layout->outlineWidth = 0.1;

        dynamic_text->text = dynamic_text_label;
        dynamic_text->font = font;
        dynamic_text->font->options = options;
        dynamic_text->layout = dynamic_text_layout;
        dynamic_text->setup();
        scenegraph->addChild(dynamic_text);
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

    // camera related details
    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto viewport = vsg::ViewportState::create(window->extent2D());
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio*radius, radius * 1000.0);
    auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scenegraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // compile the Vulkan objects
    viewer->compile();

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // assign Trackball
    viewer->addEventHandler(vsg::Trackball::create(camera));

    // assign Input handler
    viewer->addEventHandler(InputHandler::create());

    // main frame loop
    while (viewer->advanceToNextFrame())
    {
#if 0
        // update the dynamic_text label string and position
        dynamic_text_label->value() = vsg::make_string(viewer->getFrameStamp()->frameCount);
        dynamic_text_layout->position.y += 0.01;
        dynamic_text->setup();
#endif
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
