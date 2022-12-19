#include <iostream>
#include <vsg/all.h>

class InputHandler : public vsg::Inherit<vsg::Visitor, InputHandler>
{
public:
    vsg::ref_ptr<vsg::Text> keyboard_text;
    vsg::ref_ptr<vsg::Text> pointer_text;
    vsg::ref_ptr<vsg::Text> scroll_text;
    vsg::ref_ptr<vsg::Text> window_text;
    vsg::ref_ptr<vsg::Text> frame_text;

    InputHandler(vsg::ref_ptr<vsg::Text> in_keyboard_text, vsg::ref_ptr<vsg::Text> in_pointer_text,
                 vsg::ref_ptr<vsg::Text> in_scroll_text, vsg::ref_ptr<vsg::Text> in_window_text, vsg::ref_ptr<vsg::Text> in_frame_text) :
        keyboard_text(in_keyboard_text),
        pointer_text(in_pointer_text),
        scroll_text(in_scroll_text),
        window_text(in_window_text),
        frame_text(in_frame_text)
    {
    }

    void assign(vsg::Text& text, const std::string& str)
    {
        auto text_string = text.text.cast<vsg::stringValue>();

        if (text_string)
        {
            text_string->value() = str;
        }
        else
        {
            text.text = text_string = vsg::stringValue::create(str);
        }

        text.setup();
    }

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        assign(*keyboard_text, vsg::make_string(keyPress.className(), ", keyBase=", keyPress.keyBase, " ", char(keyPress.keyBase), ", keyModified=", keyPress.keyModified, " ", char(keyPress.keyModified)));
    }

    void apply(vsg::KeyReleaseEvent& keyRelease) override
    {
        assign(*keyboard_text, vsg::make_string(keyRelease.className(), ", keyBase=", keyRelease.keyBase, " ", char(keyRelease.keyBase), ", keyModified=", keyRelease.keyModified, " ", char(keyRelease.keyModified)));
    }

    void apply(vsg::MoveEvent& moveEvent) override
    {
        // note under Unix the mask value includes the keyboard modifiers in mask as the native mask is used
        assign(*pointer_text, vsg::make_string(moveEvent.className(), ", x=", moveEvent.x, ", y= ", moveEvent.y, ", mask=", moveEvent.mask, " [",
                                               ((moveEvent.mask & vsg::BUTTON_MASK_1) != 0), ((moveEvent.mask & vsg::BUTTON_MASK_2) != 0), ((moveEvent.mask & vsg::BUTTON_MASK_3) != 0), "]"));
    }

    void apply(vsg::ButtonPressEvent& buttonPress) override
    {
        // note under Unix the mask value includes the keyboard modifiers in mask as the native mask is used
        assign(*pointer_text, vsg::make_string(buttonPress.className(), ", x=", buttonPress.x, ", y=", buttonPress.y, ", mask=", buttonPress.mask, " [",
                                               ((buttonPress.mask & vsg::BUTTON_MASK_1) != 0), ((buttonPress.mask & vsg::BUTTON_MASK_2) != 0), ((buttonPress.mask & vsg::BUTTON_MASK_3) != 0), "], button=", buttonPress.button));
    }

    void apply(vsg::ButtonReleaseEvent& buttonRelease) override
    {
        // note under Unix the mask value includes the keyboard modifiers in mask as the native mask is used
        assign(*pointer_text, vsg::make_string(buttonRelease.className(), ", x=", buttonRelease.x, ", y=", buttonRelease.y, ", mask=", buttonRelease.mask, " [",
                                               ((buttonRelease.mask & vsg::BUTTON_MASK_1) != 0), ((buttonRelease.mask & vsg::BUTTON_MASK_2) != 0), ((buttonRelease.mask & vsg::BUTTON_MASK_3) != 0), "], button=", buttonRelease.button));
    }

    void apply(vsg::ScrollWheelEvent& scrollWheel) override
    {
        assign(*scroll_text, vsg::make_string(scrollWheel.className(), ", delta = ", scrollWheel.delta));
    }

    void apply(vsg::ExposeWindowEvent& exposeWindow) override
    {
        assign(*window_text, vsg::make_string(exposeWindow.className(), ", x=", exposeWindow.x, ", y=", exposeWindow.y, ", width=", exposeWindow.width, ", height=", exposeWindow.height));
    }

    void apply(vsg::ConfigureWindowEvent& configureWindow) override
    {
        assign(*window_text, vsg::make_string(configureWindow.className(), ", x=", configureWindow.x, ", y=", configureWindow.y, ", width=", configureWindow.width, ", height=", configureWindow.height));
    }

    void apply(vsg::CloseWindowEvent& closeWindow) override
    {
        // closeWindow.window is a vsg::oberserver_ptr<> so to access the pointer we first have to take a ref_ptr<> to avoid threading related lifetime issues
        vsg::ref_ptr<vsg::Window> window = closeWindow.window;
        assign(*window_text, vsg::make_string(closeWindow.className(), ", window =", window));
    }

    void apply(vsg::FrameEvent& frame) override
    {
        // closeWindow.window is a vsg::oberserver_ptr<> so to access the pointer we first have to take a ref_ptr<> to avoid threading related lifetime issues
        assign(*frame_text, vsg::make_string(frame.className(), ", frameStamp->frameCount = ", frame.frameStamp->frameCount));
    }

protected:
};

class MyPrintEvents : public vsg::PrintEvents
{
public:
    MyPrintEvents(vsg::clock::time_point in_start_point) :
        vsg::PrintEvents(in_start_point) {}

    MyPrintEvents(std::ostream& out, vsg::clock::time_point in_start_point) :
        vsg::PrintEvents(out, in_start_point) {}

    std::ostream& print(vsg::UIEvent& event) override
    {
        output << "    " << event.className() << ", " << std::chrono::duration<double, std::chrono::milliseconds::period>(event.time - start_point).count() << "ms";

        return output;
    }

    void apply(vsg::FrameEvent& event) override
    {
        output << "vsg::FameEvent previousFrameDuration = " << std::chrono::duration<double, std::chrono::milliseconds::period>(event.time - start_point).count() << "ms"
               << ", frameCount = " << event.frameStamp->frameCount << std::endl;

        start_point = event.time;
    }
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create("vsg input");
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read("--double-buffer")) windowTraits->swapchainPreferences.imageCount = 2;
    if (arguments.read("--triple-buffer")) windowTraits->swapchainPreferences.imageCount = 3; // default
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--FIFO")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (arguments.read("--FIFO_RELAXED")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    if (arguments.read("--MAILBOX")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--no-frame", "--nf"})) windowTraits->decoration = false;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    bool printEvents = arguments.read("-p");
    auto event_read_filename = arguments.value(std::string(""), "-i");
    auto event_output_filename = arguments.value(std::string(""), "-o");
    auto font_filename = arguments.value(std::string("fonts/times.vsgb"), "--font");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    auto options = vsg::Options::create();
    options->paths = searchPaths;

    auto scenegraph = vsg::Group::create();

    auto font = vsg::read_cast<vsg::Font>(font_filename, options);

    double aspectRatio = double(windowTraits->width) / double(windowTraits->height);
    double projectionHeight = 25.0;

    vsg::vec3 position(2.0f, projectionHeight - 2.0f, 0.0f);
    vsg::vec3 delta(0.0f, -2.0f, 0.0f);

    // main label
    {
        auto main_label = vsg::stringValue::create("Please press keys and move/click mouse buttons\nand resize the window.");
        auto main_layout = vsg::StandardLayout::create();
        auto main = vsg::Text::create();

        main_layout->position = position;
        main_layout->horizontal = vsg::vec3(1.0f, 0.0f, 0.0f);
        main_layout->vertical = vsg::vec3(0.0f, 1.0f, 0.0f);
        main_layout->color = vsg::vec4(1.0f, 1.0f, 0.3, 1.0f);
        main_layout->outlineWidth = 0.1;

        main->text = main_label;
        main->font = font;
        main->layout = main_layout;
        main->setup();
        scenegraph->addChild(main);

        position += delta * 2.0f;
    }

    // pointer event label
    auto pointer_text = vsg::Text::create();
    {
        pointer_text->technique = vsg::GpuLayoutTechnique::create();

        auto pointer_layout = vsg::StandardLayout::create();
        pointer_layout->position = position;
        pointer_layout->horizontal = vsg::vec3(1.0f, 0.0f, 0.0f);
        pointer_layout->vertical = vsg::vec3(0.0f, 1.0f, 0.0f);
        pointer_layout->color = vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        pointer_layout->outlineWidth = 0.1;

        auto pointer_string = vsg::stringValue::create("Pointer event:");
        pointer_text->text = pointer_string;
        pointer_text->font = font;
        pointer_text->layout = pointer_layout;
        pointer_text->setup(256);
        scenegraph->addChild(pointer_text);

        position += delta;
    }

    // keyboard events label
    auto keyboard_text = vsg::Text::create();
    {
        keyboard_text->technique = vsg::GpuLayoutTechnique::create();

        auto keyboard_layout = vsg::StandardLayout::create();
        keyboard_layout->position = position;
        keyboard_layout->horizontal = vsg::vec3(1.0f, 0.0f, 0.0f);
        keyboard_layout->vertical = vsg::vec3(0.0f, 1.0f, 0.0f);
        keyboard_layout->color = vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        keyboard_layout->outlineWidth = 0.1;

        auto keyboard_string = vsg::stringValue::create("Keyboard event:");
        keyboard_text->text = keyboard_string;
        keyboard_text->font = font;
        keyboard_text->layout = keyboard_layout;
        keyboard_text->setup(256);
        scenegraph->addChild(keyboard_text);

        position += delta;
    }

    // scroll events label
    auto scroll_text = vsg::Text::create();
    {
        scroll_text->technique = vsg::GpuLayoutTechnique::create();

        auto scroll_layout = vsg::StandardLayout::create();
        scroll_layout->position = position;
        scroll_layout->horizontal = vsg::vec3(1.0f, 0.0f, 0.0f);
        scroll_layout->vertical = vsg::vec3(0.0f, 1.0f, 0.0f);
        scroll_layout->color = vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        scroll_layout->outlineWidth = 0.1;

        auto scroll_string = vsg::stringValue::create("Scroll event:");
        scroll_text->text = scroll_string;
        scroll_text->font = font;
        scroll_text->layout = scroll_layout;
        scroll_text->setup(256);
        scenegraph->addChild(scroll_text);

        position += delta;
    }

    // window events label
    auto window_text = vsg::Text::create();
    {
        window_text->technique = vsg::GpuLayoutTechnique::create();

        auto application_layout = vsg::StandardLayout::create();
        application_layout->position = position;
        application_layout->horizontal = vsg::vec3(1.0, 0.0, 0.0f);
        application_layout->vertical = vsg::vec3(0.0, 1.0, 0.0f);
        application_layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0f);
        application_layout->outlineWidth = 0.1;

        auto application_string = vsg::stringValue::create("Window event:");
        window_text->text = application_string;
        window_text->font = font;
        window_text->layout = application_layout;
        window_text->setup(256);
        scenegraph->addChild(window_text);

        position += delta;
    }

    // application events label
    auto frame_text = vsg::Text::create();
    {
        frame_text->technique = vsg::GpuLayoutTechnique::create();

        auto application_layout = vsg::StandardLayout::create();
        application_layout->position = position;
        application_layout->horizontal = vsg::vec3(1.0, 0.0, 0.0f);
        application_layout->vertical = vsg::vec3(0.0, 1.0, 0.0f);
        application_layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0f);
        application_layout->outlineWidth = 0.1;

        auto application_string = vsg::stringValue::create("Frame event:");
        frame_text->text = application_string;
        frame_text->font = font;
        frame_text->layout = application_layout;
        frame_text->setup(256);
        scenegraph->addChild(frame_text);

        position += delta;
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

    // set up the camera
    auto viewport = vsg::ViewportState::create(window->extent2D());
    auto projection = vsg::Orthographic::create(0.0, projectionHeight * aspectRatio, 0.0, projectionHeight, 100.0, 0.0);
    auto lookAt = vsg::LookAt::create(vsg::dvec3(0.0, 0.0, 2.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 1.0, 0.0));
    auto camera = vsg::Camera::create(projection, lookAt, viewport);

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scenegraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // compile the Vulkan objects
    viewer->compile();

    vsg::ref_ptr<vsg::RecordEvents> recordEvents;
    if (!event_output_filename.empty() || printEvents)
    {
        recordEvents = vsg::RecordEvents::create();
        viewer->addEventHandler(recordEvents);
    }

    vsg::ref_ptr<vsg::PlayEvents> playEvents;
    if (!event_read_filename.empty())
    {
        auto read_events = vsg::read(event_read_filename);
        if (read_events)
        {
            playEvents = vsg::PlayEvents::create(read_events, viewer->start_point().time_since_epoch());
        }
    }

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // assign Input handler
    viewer->addEventHandler(InputHandler::create(keyboard_text, pointer_text, scroll_text, window_text, frame_text));

    // main frame loop
    while (viewer->advanceToNextFrame())
    {
        if (playEvents)
        {
            playEvents->dispatchFrameEvents(viewer->getEvents());
        }

        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    if (recordEvents)
    {
        if (!event_output_filename.empty())
        {
            // shift the time of recorded events to relative to 0, so we can later add in any new viewer->start_point() during playback.
            vsg::ShiftEventTime shiftTime(-viewer->start_point().time_since_epoch());
            recordEvents->events->accept(shiftTime);

            vsg::write(recordEvents->events, event_output_filename);
        }

        if (printEvents)
        {
            // shift the time of recorded events to relative to 0, so we can later add in any new viewer->start_point() during playback.
            MyPrintEvents print(viewer->start_point());
            recordEvents->events->accept(print);
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
