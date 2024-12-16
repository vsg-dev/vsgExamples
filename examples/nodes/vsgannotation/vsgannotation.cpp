// Derived from vsgviewer, of course

#include "Annotation.h"

#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

// using NamedGroup = experimental::Annotation<vsg::Group>;
using NamedTransform = experimental::Annotation<vsg::MatrixTransform>;

// Create a custom subclass with a different display color. This illustrates a gotcha with
// vsg::Inherit and a custom accept(RecordTraversal&): Inherit::accept() calls
// RecordTraveral::apply(), but that is not virtual and will end up doing the recording action for
// vsg::Group, which would leave us without an annotation.

class NamedGroup : public vsg::Inherit<experimental::Annotation<vsg::Group>, NamedGroup>
{
public:
    NamedGroup()
    {
        debugColor = vsg::vec4(.8f, .8f, .8f, 1.0f);
    }

    using experimental::Annotation<vsg::Group>::accept;

    void accept(vsg::RecordTraversal& visitor) const override
    {
        experimental::Annotation<vsg::Group>::accept(visitor);
    }
};

// Lay out the loaded files on the X axis

vsg::ref_ptr<NamedGroup> createAnnotatedScene(const std::vector<std::string>& names,
                                              const std::vector<vsg::ref_ptr<vsg::Node>>& nodes)
{
    auto scene = NamedGroup::create();
    scene->annotation = "Scene";
    double xExtent = 0.0;
    auto nameItr = names.begin();
    auto nodeItr = nodes.begin();
    for (; nodeItr != nodes.end(); ++nameItr, ++nodeItr)
    {
        vsg::ComputeBounds computeBounds;
        (*nodeItr)->accept(computeBounds);
        double width = (computeBounds.bounds.max.x - computeBounds.bounds.min.x) * 1.1;
        vsg::dmat4 mat = vsg::translate(xExtent + width / 2, 0.0, 0.0);
        auto matNode = NamedTransform::create(mat);
        matNode->annotation = *nameItr;
        matNode->addChild(*nodeItr);
        scene->addChild(matNode);
        xExtent += width;
    }
    return scene;
}

void enableGenerateDebugInfo(vsg::ref_ptr<vsg::Options> options)
{
    auto shaderHints = vsg::ShaderCompileSettings::create();
    shaderHints->generateDebugInfo = true;

    auto& text = options->shaderSets["text"] = vsg::createTextShaderSet(options);
    text->defaultShaderHints = shaderHints;

    auto& flat = options->shaderSets["flat"] = vsg::createFlatShadedShaderSet(options);
    flat->defaultShaderHints = shaderHints;

    auto& phong = options->shaderSets["phong"] = vsg::createPhongShaderSet(options);
    phong->defaultShaderHints = shaderHints;

    auto& pbr = options->shaderSets["pbr"] = vsg::createPhysicsBasedRenderingShaderSet(options);
    pbr->defaultShaderHints = shaderHints;
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
        // Use the VK_EXT_debug_utils extension, the point of this demo
        windowTraits->debugUtils = true;
        windowTraits->windowTitle = "vsgviewer";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        windowTraits->synchronizationLayer = arguments.read("--sync");
        bool reportAverageFrameRate = arguments.read("--fps");
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
        auto loadLevels = arguments.value(0, "--load-levels");
        auto maxPagedLOD = arguments.value(0, "--maxPagedLOD");
        auto horizonMountainHeight = arguments.value(0.0, "--hmh");
        auto nearFarRatio = arguments.value<double>(0.001, "--nfr");
        if (arguments.read("--rgb")) options->mapRGBtoRGBAHint = false;

        if (arguments.read({"--shader-debug-info", "--sdi"}))
        {
            enableGenerateDebugInfo(options);
            windowTraits->deviceExtensionNames.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
        }

        if (int log_level = 0; arguments.read("--log-level", log_level)) vsg::Logger::instance()->level = vsg::Logger::Level(log_level);

        // For a sanity check that everything still works if we don't enable the extension.
        if (arguments.read("--no-annotate"))
        {
            windowTraits->debugUtils = false;
        }
        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        if (argc <= 1)
        {
            std::cout << "Please specify a 3d model or image file on the command line." << std::endl;
            return 1;
        }

        std::vector<std::string> fileNames;
        std::vector<vsg::ref_ptr<vsg::Node>> nodes;
        // read any vsg files
        for (int i = 1; i < argc; ++i)
        {
            vsg::Path filename = arguments[i];
            vsg::Path path = vsg::findFile(filename, options);

            auto object = vsg::read(filename, options);
            if (auto node = object.cast<vsg::Node>())
            {
                fileNames.push_back(path.string());
                nodes.push_back(node);
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

        if (nodes.empty())
        {
            return 1;
        }

        vsg::ref_ptr<vsg::Node> vsg_scene = createAnnotatedScene(fileNames, nodes);

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create window." << std::endl;
            return 1;
        }

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

        if (pathFilename)
        {
            auto cameraAnimation = vsg::CameraAnimationHandler::create(camera, pathFilename, options);
            viewer->addEventHandler(cameraAnimation);
            if (cameraAnimation->animation) cameraAnimation->play();
        }

        viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

        // if required preload specific number of PagedLOD levels.
        if (loadLevels > 0)
        {
            vsg::LoadPagedLOD loadPagedLOD(camera, loadLevels);

            auto startTime = vsg::clock::now();

            vsg_scene->accept(loadPagedLOD);

            auto time = std::chrono::duration<float, std::chrono::milliseconds::period>(vsg::clock::now() - startTime).count();
            std::cout << "No. of tiles loaded " << loadPagedLOD.numTiles << " in " << time << "ms." << std::endl;
        }

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        viewer->compile();

        if (maxPagedLOD > 0)
        {
            // set targetMaxNumPagedLODWithHighResSubgraphs after Viewer::compile() as it will assign any DatabasePager if required.
            for (auto& task : viewer->recordAndSubmitTasks)
            {
                if (task->databasePager) task->databasePager->targetMaxNumPagedLODWithHighResSubgraphs = maxPagedLOD;
            }
        }

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
