#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <vsg/utils/Instrumentation.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

class InstrumentationHandler : public vsg::Inherit<vsg::Visitor, InstrumentationHandler>
{
public:
    vsg::ref_ptr<vsg::Profiler> instrumentation;

    InstrumentationHandler(vsg::ref_ptr<vsg::Profiler> in_instrumentation) :
        instrumentation(in_instrumentation) {}

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (keyPress.keyModified == 'c')
        {
            if (instrumentation->settings->cpu_instrumentation_level > 0) --instrumentation->settings->cpu_instrumentation_level;
        }
        else if (keyPress.keyModified == 'C')
        {
            if (instrumentation->settings->cpu_instrumentation_level < 3) ++instrumentation->settings->cpu_instrumentation_level;
        }
        if (keyPress.keyModified == 'g')
        {
            if (instrumentation->settings->gpu_instrumentation_level > 0) --instrumentation->settings->gpu_instrumentation_level;
        }
        else if (keyPress.keyModified == 'G')
        {
            if (instrumentation->settings->gpu_instrumentation_level < 3) ++instrumentation->settings->gpu_instrumentation_level;
        }
    }
};

vsg::ref_ptr<vsg::Node> decorateWithInstrumentationNode(vsg::ref_ptr<vsg::Node> node, const std::string& name, vsg::uint_color color)
{
    auto instrumentationNode = vsg::InstrumentationNode::create(node);
    instrumentationNode->setName(name);
    instrumentationNode->setColor(color);

    vsg::info("decorateWithInstrumentationNode(", node, ", ", name, ", {", int(color.r), ", ", int(color.g), ", ", int(color.b), ", ", int(color.a), "})");

    return instrumentationNode;
}

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // set up vsg::Options to pass in filepaths, ReaderWriters and other IO related options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->sharedObjects = vsg::SharedObjects::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());
#endif

        arguments.read(options);

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "vsginstrumentation";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        windowTraits->synchronizationLayer = arguments.read("--sync");
        bool reportAverageFrameRate = arguments.read("--fps");
        bool multiThreading = arguments.read("--mt");
        if (arguments.read("--double-buffer")) windowTraits->swapchainPreferences.imageCount = 2;
        if (arguments.read("--triple-buffer")) windowTraits->swapchainPreferences.imageCount = 3; // default
        if (arguments.read("--IMMEDIATE")) { windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; }
        if (arguments.read("--FIFO")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        if (arguments.read("--FIFO_RELAXED")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        if (arguments.read("--MAILBOX")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        if (arguments.read({"-t", "--test"}))
        {
            windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            windowTraits->fullscreen = true;
            reportAverageFrameRate = true;
        }
        if (arguments.read({"--st", "--small-test"}))
        {
            windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            windowTraits->width = 192, windowTraits->height = 108;
            windowTraits->decoration = false;
            reportAverageFrameRate = true;
        }

        if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
        if (arguments.read({"--no-frame", "--nf"})) windowTraits->decoration = false;
        if (arguments.read("--or")) windowTraits->overrideRedirect = true;
        if (arguments.read("--d32")) windowTraits->depthFormat = VK_FORMAT_D32_SFLOAT;
        arguments.read("--screen", windowTraits->screenNum);
        arguments.read("--display", windowTraits->display);
        arguments.read("--samples", windowTraits->samples);
        auto numFrames = arguments.value(-1, "-f");
        auto pathFilename = arguments.value<vsg::Path>("", "-p");
        auto outputFilename = arguments.value<vsg::Path>("", "-o");
        auto horizonMountainHeight = arguments.value(0.0, "--hmh");
        auto nearFarRatio = arguments.value<double>(0.001, "--nfr");
        if (arguments.read("--rgb")) options->mapRGBtoRGBAHint = false;

        // set whether calibrated timestamp extension should be enabled.
        bool calibrated = arguments.read("--calibrated");
        bool decorate = arguments.read("--decorate");

        // set Profiler options
        auto settings = vsg::Profiler::Settings::create();
        arguments.read("--cpu", settings->cpu_instrumentation_level);
        arguments.read("--gpu", settings->gpu_instrumentation_level);
        arguments.read("--log-size", settings->log_size);
        arguments.read("--gpu-size", settings->gpu_timestamp_size);

        // create the profiler
        auto instrumentation = vsg::Profiler::create(settings);

        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create window." << std::endl;
            return 1;
        }

        if (calibrated)
        {
            auto physicalDevice = window->getOrCreatePhysicalDevice();
            if (physicalDevice->supportsDeviceExtension(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME))
            {
                windowTraits->deviceExtensionNames.push_back(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
            }
        }

        if (int log_level = 0; arguments.read("--log-level", log_level)) vsg::Logger::instance()->level = vsg::Logger::Level(log_level);

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        if (argc <= 1)
        {
            std::cout << "Please specify a 3d model or image file on the command line." << std::endl;
            return 1;
        }

        // assign instrumentation to vsg::Options to enable read/write functions to provide instrumentation
        options->instrumentation = instrumentation;

        auto group = vsg::Group::create();

        vsg::Path path;

        // read any vsg files
        for (int i = 1; i < argc; ++i)
        {
            vsg::Path filename = arguments[i];
            path = vsg::filePath(filename);

            auto object = vsg::read(filename, options);
            if (auto node = object.cast<vsg::Node>())
            {
                if (decorate)
                {
                    group->addChild(decorateWithInstrumentationNode(node, filename.string(), vsg::uint_color(255, 255, 64, 255)));
                }
                else
                {
                    group->addChild(node);
                }
            }
            else if (object)
            {
                std::cout << "Unable to view object of type " << object->className() << std::endl;
            }
            else
            {
                std::cout << "Unable to load file " << filename << std::endl;
            }
        }

        if (group->children.empty())
        {
            return 1;
        }

        vsg::ref_ptr<vsg::Node> vsg_scene;
        if (group->children.size() == 1)
            vsg_scene = group->children[0];
        else
            vsg_scene = group;

        if (outputFilename)
        {
            vsg::write(vsg_scene, outputFilename, options);
            return 1;
        }

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
        viewer->addWindow(window);

        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

        // set up the camera
        auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
        if (ellipsoidModel)
        {
            perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
        }
        else
        {
            perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // add close handler to respond to the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        auto animationPathHandler = vsg::CameraAnimationHandler::create(camera, pathFilename, options);
        if (animationPathHandler->animation) animationPathHandler->play();
        viewer->addEventHandler(animationPathHandler);

        viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

        // add event handler to control the cpu and gpu_instrumentation_level using the 'c', 'g' keys to reduce the cpu and gpu instrumentation level, and 'C' and 'G' to increase them respectively.
        viewer->addEventHandler(InstrumentationHandler::create(instrumentation));

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
        viewer->assignInstrumentation(instrumentation);

        if (multiThreading)
        {
            viewer->setupThreading();
        }

        viewer->compile();

        viewer->start_point() = vsg::clock::now();

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->update();

            viewer->recordAndSubmit();

            viewer->present();
        }

        if (reportAverageFrameRate)
        {
            auto fs = viewer->getFrameStamp();
            double fps = static_cast<double>(fs->frameCount) / std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - viewer->start_point()).count();
            std::cout << "Average frame rate = " << fps << " fps" << std::endl;
        }

        if (instrumentation && instrumentation->log)
        {
            instrumentation->finish();
            instrumentation->log->report(std::cout);
        }
    }
    catch (const vsg::Exception& ve)
    {
        for (int i = 0; i < argc; ++i) std::cerr << argv[i] << " ";
        std::cerr << "\n[Exception] - " << ve.message << " result = " << ve.result << std::endl;
        return 1;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
