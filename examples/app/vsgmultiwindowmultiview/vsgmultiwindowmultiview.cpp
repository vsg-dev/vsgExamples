#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
#include <atomic>
#include <future>

// Async terminal input checker
std::atomic<bool> shouldExit{false};

void checkTerminalInput() {
    char input;
    while (!shouldExit.load()) {
        if (std::cin.get(input)) {
            if (input == 'q' || input == 'Q') {
                shouldExit.store(true);
                break;
            }
        }
    }
}

// Custom close handler that handles window closing without terminating the viewer
struct WindowCloseHandler : public vsg::Inherit<vsg::Visitor, WindowCloseHandler>
{
    vsg::ref_ptr<vsg::Viewer> viewer;
    std::vector<vsg::ref_ptr<vsg::Window>>* windowsToRemove;

    WindowCloseHandler(vsg::ref_ptr<vsg::Viewer> v, std::vector<vsg::ref_ptr<vsg::Window>>* windows) : 
        viewer(v), windowsToRemove(windows) {}

    void apply(vsg::CloseWindowEvent& closeWindowEvent) override
    {   
        // Mark this window for removal
        if (closeWindowEvent.window && windowsToRemove) {
            windowsToRemove->push_back(closeWindowEvent.window);
        }
    }
};

// Structure to hold window recreation info
struct WindowInfo
{
    vsg::Path filename;
    int32_t x, y;
    uint32_t width, height;
    std::chrono::steady_clock::time_point recreateTime;
    bool needsRecreation = false;
    vsg::ref_ptr<vsg::Window> window;
    vsg::ref_ptr<vsg::CommandGraph> commandGraph;
    bool isMainWindow = false;
};

// Helper function to create a window with a loaded model
std::tuple<vsg::ref_ptr<vsg::CommandGraph>, vsg::ref_ptr<vsg::Window>> createWindowWithModel(
    vsg::ref_ptr<vsg::Viewer> viewer,
    const vsg::Path& filename,
    vsg::ref_ptr<vsg::Options> options,
    int32_t x, int32_t y, uint32_t width, uint32_t height,
    vsg::ref_ptr<vsg::WindowTraits> refTraits,
    bool isMainWindow = false)
{
    // Load the model
    auto node = vsg::read_cast<vsg::Node>(filename, options);
    if (!node)
    {
        std::cout << "Failed to load " << filename << std::endl;
        return {nullptr, nullptr};
    }

    std::cout << "Loaded " << filename << std::endl;

    // Create window traits
    auto traits = vsg::WindowTraits::create();
    traits->x = x;
    traits->y = y;
    traits->width = width;
    traits->height = height;
    traits->windowTitle = filename.string() + (isMainWindow ? " (Main)" : "");
    if (refTraits) {
        traits->debugLayer = refTraits->debugLayer;
        traits->apiDumpLayer = refTraits->apiDumpLayer;
        traits->device = refTraits->device;
    }

    // Create the new window
    auto newWindow = vsg::Window::create(traits);
    if (!newWindow)
    {
        std::cout << "Failed to create window for " << filename << std::endl;
        return {nullptr, nullptr};
    }

    // Create scene group
    vsg::ref_ptr<vsg::Group> vsg_scene = vsg::Group::create();
    vsg_scene->addChild(node);

    // Compute bounds for camera positioning
    vsg::ComputeBounds computeBounds;
    node->accept(computeBounds);

    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5;
    double nearFarRatio = 0.001;

    // Set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0),
                                      centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    if (isMainWindow) {
        // For main window, check for ellipsoid model like in original code
        auto ellipsoidModel = node->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
        if (ellipsoidModel)
        {
            double horizonMountainHeight = 0.0;
            perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, 
                static_cast<double>(width) / static_cast<double>(height), nearFarRatio, horizonMountainHeight);
        }
        else
        {
            perspective = vsg::Perspective::create(30.0, static_cast<double>(width) / static_cast<double>(height),
                                                   nearFarRatio * radius, radius * 4.5);
        }
    } else {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(width) / static_cast<double>(height),
                                               nearFarRatio * radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(VkExtent2D{width, height}));

    // Create view
    auto view = vsg::View::create(camera);
    view->addChild(vsg::createHeadlight());
    view->addChild(vsg_scene);

    // Create render graph
    auto renderGraph = vsg::RenderGraph::create(newWindow, view);
    renderGraph->clearValues[0].color = vsg::sRGB_to_linear(0.2f, 0.2f, 0.2f, 1.0f);

    // Create command graph
    auto commandGraph = vsg::CommandGraph::create(newWindow);
    commandGraph->addChild(renderGraph);

    // Create trackball for this window
    auto trackball = vsg::Trackball::create(camera);
    trackball->addWindow(newWindow);
    viewer->addEventHandler(trackball);

    // Add to viewer's compile manager
    viewer->compileManager->add(*newWindow, view);

    // Add the command graph to viewer
    viewer->addRecordAndSubmitTaskAndPresentation({commandGraph});

    return {commandGraph, newWindow};
}

int main(int argc, char** argv)
{
    try
    {
        // Start terminal input monitoring thread
        std::thread inputThread(checkTerminalInput);
        inputThread.detach();
        
        std::cout << "Press 'q' and Enter in the terminal to quit the application" << std::endl;

        // Set up defaults and read command line arguments
        vsg::CommandLine arguments(&argc, argv);
        auto windowTraits = vsg::WindowTraits::create(arguments);

        // Set up options
        auto options = vsg::Options::create();
        options->sharedObjects = vsg::SharedObjects::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
        options->add(vsgXchange::all::create());
#endif

        options->readOptions(arguments);

        auto numFrames = arguments.value(-1, "-f");

        // Resource hints
        vsg::ref_ptr<vsg::ResourceHints> resourceHints;
        if (vsg::Path resourceFile; arguments.read("--resource", resourceFile)) 
            resourceHints = vsg::read_cast<vsg::ResourceHints>(resourceFile);

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        // Create the viewer
        auto viewer = vsg::Viewer::create();
        viewer->compileManager = vsg::CompileManager::create(*viewer, vsg::ResourceHints::create());

        // List to track windows that need to be removed
        std::vector<vsg::ref_ptr<vsg::Window>> windowsToRemove;

        // Add our custom close handler
        viewer->addEventHandler(WindowCloseHandler::create(viewer, &windowsToRemove));

        // Set up window recreation info - including main window
        std::vector<WindowInfo> windowInfos = {
            {"models/teapot.vsgt", 100, 100, 800, 600, {}, false, nullptr, nullptr, true}, // Main window
            {"models/lz.vsgt", 50, 50, 512, 480, {}, false, nullptr, nullptr, false},
            {"models/openstreetmap.vsgt", 600, 50, 512, 480, {}, false, nullptr, nullptr, false},
            {"models/teapot.vsgt", 1150, 50, 512, 480, {}, false, nullptr, nullptr, false}
        };

        // Set up resource hints
        if (!resourceHints)
        {
            resourceHints = vsg::ResourceHints::create();
            resourceHints->numDescriptorSets = 256;
            resourceHints->descriptorPoolSizes.push_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256});
        }

        // Configure viewer with resource hints
        viewer->compile(resourceHints);

        // Create all windows initially
        vsg::ref_ptr<vsg::WindowTraits> refTraits = nullptr;
        for (auto& info : windowInfos)
        {
            auto [cmdGraph, newWindow] = createWindowWithModel(viewer, info.filename, options, 
                info.x, info.y, info.width, info.height, refTraits, info.isMainWindow);
            if (cmdGraph && newWindow)
            {
                info.window = newWindow;
                info.commandGraph = cmdGraph;
            }
            if (!refTraits)
            {
                refTraits = newWindow->traits();
                refTraits->device = newWindow->getOrCreateDevice();
            }
        }

        // Compile any new additions
        viewer->compile();

        // Main rendering loop
        while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0) && !shouldExit.load())
        {
            auto currentTime = std::chrono::steady_clock::now();
            
            // Handle window removal
            for (auto& windowToRemove : windowsToRemove)
            {
                // Find which window info this corresponds to
                for (auto& info : windowInfos)
                {
                    if (info.window == windowToRemove)
                    {
                        std::cout << "Removing window for " << info.filename << ", will recreate in 1 second" << std::endl;
                        
                        // Remove from viewer
                        viewer->deviceWaitIdle();
                        viewer->removeWindow(info.window);
                        
                        // Clear our references
                        info.window = nullptr;
                        info.commandGraph = nullptr;
                        info.needsRecreation = true;
                        info.recreateTime = currentTime + std::chrono::seconds(1);
                        break;
                    }
                }
            }
            windowsToRemove.clear();
            
            // Check for windows that need recreation
            for (auto& info : windowInfos)
            {
                if (info.needsRecreation && currentTime >= info.recreateTime)
                {
                    std::cout << "Recreating window for " << info.filename << std::endl;
                    auto [cmdGraph, newWindow] = createWindowWithModel(viewer, info.filename, options, 
                        info.x, info.y, info.width, info.height, 
                        refTraits, info.isMainWindow); // Use first window as reference if available
                    
                    if (cmdGraph && newWindow)
                    {
                        info.window = newWindow;
                        info.commandGraph = cmdGraph;
                        info.needsRecreation = false;
                        viewer->compile();
                    }
                }
            }

            viewer->handleEvents();
            viewer->update();
            viewer->recordAndSubmit();
            viewer->present();
        }
        
        shouldExit.store(true); // Signal input thread to stop
    }
    catch (const vsg::Exception& ve)
    {
        for (int i = 0; i < argc; ++i) std::cerr << argv[i] << " ";
        std::cerr << "\n[Exception] - " << ve.message << " result = " << ve.result << std::endl;
        shouldExit.store(true);
        return 1;
    }

    return 0;
}