#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->sharedObjects = vsg::SharedObjects::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgtransform";

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});

    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    auto numFrames = arguments.value(-1, "-f");
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }

    // bool useStagingBuffer = arguments.read({"--staging-buffer", "-s"});

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create window." << std::endl;
        return 1;
    }

    auto outputFilename = arguments.value<vsg::Path>("", "-o");

    if (argc <= 1)
    {
        std::cout << "Please specify model to load on command line." << std::endl;
        return 0;
    }

    vsg::Path filename = argv[1];
    auto model = vsg::read_cast<vsg::Node>(filename, options);
    if (!model)
    {
        std::cout << "Failed to load " << filename << std::endl;
        return 1;
    }

    // compute the bounds of the scene graph to help position camera
    auto bounds = vsg::visit<vsg::ComputeBounds>(model).bounds;
    vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;
    double radius = vsg::length(bounds.max - bounds.min) * 0.6;

    auto scene = vsg::Group::create();

    auto font_filename = arguments.value(std::string("fonts/times.vsgb"), "-f");
    auto font = vsg::read_cast<vsg::Font>(font_filename, options);

    auto layout = vsg::StandardLayout::create();
    layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
    layout->position = vsg::vec3(0.0f, 0.0f, 0.0f);
    layout->horizontal = vsg::vec3(1.0f, 0.0f, 0.0f);
    layout->vertical = vsg::vec3(0.0f, 0.0f, 1.0f);
    layout->color = vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    layout->outlineWidth = 0.1f;

    auto text = vsg::Text::create();
    text->text = vsg::stringValue::create("Layer");
    text->font = font;
    text->layout = layout;
    text->setup(0, options);

    // clear the depth buffer before view2 gets rendered
    uint32_t width = window->extent2D().width;
    uint32_t height = window->extent2D().height;
    VkClearValue clearValue{};
    clearValue.depthStencil = {0.0f, 0};
    VkClearAttachment attachment{VK_IMAGE_ASPECT_DEPTH_BIT, 1, clearValue};
    VkClearRect rect{VkRect2D{VkOffset2D{0, 0}, VkExtent2D{width, height}}, 0, 1};
    auto clearAttachments = vsg::ClearAttachments::create(vsg::ClearAttachments::Attachments{attachment}, vsg::ClearAttachments::Rects{rect});

    auto hud = vsg::Group::create();
    hud->addChild(clearAttachments);
    hud->addChild(text);

    // draw lower binNumber first
    auto layer_1 = vsg::Layer::create();
    layer_1->binNumber = 0;
    layer_1->value = 0;
    layer_1->child = model;

    // draw higher binNumber second
    auto layer_2 = vsg::Layer::create();
    layer_2->binNumber = 1;
    layer_2->value = 1;
    layer_2->child = hud;

    scene->addChild(layer_1);
    scene->addChild(layer_2);

    // write out scene if required
    if (outputFilename)
    {
        vsg::write(scene, outputFilename, options);
        return 0;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);

    vsg::ref_ptr<vsg::LookAt> lookAt;

    // update bounds to new scene extent
    bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
    centre = (bounds.min + bounds.max) * 0.5;
    radius = vsg::length(bounds.max - bounds.min) * 0.6;

    // set up the camera
    lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    double nearFarRatio = 0.001;
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.0);

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add the camera and scene graph to View
    auto view = vsg::View::create();
    view->camera = camera;
    view->addChild(scene);

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));
    viewer->addEventHandler(vsg::Trackball::create(camera));

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile();

    auto startTime = vsg::clock::now();
    double numFramesCompleted = 0.0;

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();
        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();

        numFramesCompleted += 1.0;
    }

    auto duration = std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - startTime).count();
    if (numFramesCompleted > 0.0)
    {
        std::cout << "Average frame rate = " << (numFramesCompleted / duration) << std::endl;
    }

    return 0;
}
