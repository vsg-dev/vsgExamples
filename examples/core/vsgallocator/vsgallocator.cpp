#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

class StdAllocator : public vsg::Allocator
{
public:
    StdAllocator(std::unique_ptr<Allocator> in_nestedAllocator = {}) :
        vsg::Allocator(std::move(in_nestedAllocator))
    {
    }

    ~StdAllocator()
    {
    }

    void report(std::ostream& out) const override
    {
        out << "StdAllocator::report() " << std::endl;
    }

    void* allocate(std::size_t size, vsg::AllocatorAffinity) override
    {
        return operator new(size); //, std::align_val_t{default_alignment});
    }

    bool deallocate(void* ptr, std::size_t size) override
    {
        if (nestedAllocator && nestedAllocator->deallocate(ptr, size)) return true;

        operator delete(ptr); //, std::align_val_t{default_alignment});
        return true;
    }

    size_t deleteEmptyMemoryBlocks() override { return 0; }
    size_t totalAvailableSize() const override { return 0; }
    size_t totalReservedSize() const override { return 0; }
    size_t totalMemorySize() const override { return 0; }
    void setBlockSize(vsg::AllocatorAffinity, size_t) {}
};

struct SceneStatistics : public vsg::Inherit<vsg::ConstVisitor, SceneStatistics>
{
    std::map<const char*, size_t> objectCounts;

    void report(std::ostream& out)
    {
        for (auto& [str, count] : objectCounts) out << "  " << str << " " << count << std::endl;
    }

    void apply(const vsg::Node& node) override
    {
        ++objectCounts[node.className()];
        node.traverse(*this);
    }
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    // Allocaotor related command line settings
    if (size_t alignment; arguments.read("--alignment", alignment)) vsg::Allocator::instance().reset(new vsg::IntrusiveAllocator(std::move(vsg::Allocator::instance()), alignment));
    if (arguments.read("--std")) vsg::Allocator::instance().reset(new StdAllocator(std::move(vsg::Allocator::instance())));
    if (int type; arguments.read("--allocator", type)) vsg::Allocator::instance()->allocatorType = vsg::AllocatorType(type);
    if (size_t objectsBlockSize; arguments.read("--objects", objectsBlockSize)) vsg::Allocator::instance()->setBlockSize(vsg::ALLOCATOR_AFFINITY_OBJECTS, objectsBlockSize);
    if (size_t nodesBlockSize; arguments.read("--nodes", nodesBlockSize)) vsg::Allocator::instance()->setBlockSize(vsg::ALLOCATOR_AFFINITY_NODES, nodesBlockSize);
    if (size_t dataBlockSize; arguments.read("--data", dataBlockSize)) vsg::Allocator::instance()->setBlockSize(vsg::ALLOCATOR_AFFINITY_DATA, dataBlockSize);

    double loadDuration = 0.0;
    double frameRate = 0.0;
    vsg::time_point endOfViewerScope;

    try
    {
        auto windowTraits = vsg::WindowTraits::create(arguments);

        // set up vsg::Options to pass in filepaths, ReaderWriters and other IO related options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());
#endif

        options->readOptions(arguments);

        if (arguments.read("--so"))
        {
            options->sharedObjects = vsg::SharedObjects::create();
            std::cout << "Assigned vsg::SharedObjects " << options->sharedObjects << std::endl;
        }

        if (arguments.read({"-t", "--test"}))
        {
            windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            windowTraits->fullscreen = true;
        }
        if (arguments.read({"--st", "--small-test"}))
        {
            windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            windowTraits->width = 192, windowTraits->height = 108;
            windowTraits->decoration = false;
        }
        bool reportAtEndOfAllFrames = arguments.read("--report");
        auto numFrames = arguments.value(-1, "-f");
        auto pathFilename = arguments.value<vsg::Path>("", "-p");
        auto loadLevels = arguments.value(0, "--load-levels");
        auto horizonMountainHeight = arguments.value(0.0, "--hmh");
        auto maxPagedLOD = arguments.value(0, "--maxPagedLOD");
        if (arguments.read("--rgb")) options->mapRGBtoRGBAHint = false;

        size_t stats = 0;
        if (arguments.read("--stats")) stats = 1;
        if (arguments.read("--num-stats", stats)) {}

        bool useViewer = !arguments.read("--no-viewer");

        vsg::Affinity affinity;
        uint32_t cpu = 0;
        while (arguments.read({"--cpu", "-c"}, cpu))
        {
            affinity.cpus.insert(cpu);
        }

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        // if required set the affinity of the main thread.
        if (affinity) vsg::setAffinity(affinity);

        if (argc <= 1)
        {
            std::cout << "Please specify a 3d model on the command line." << std::endl;
            return 1;
        }

        // record time point just before loading the scene graph
        auto startOfLoad = vsg::clock::now();

        auto group = vsg::Group::create();

        // read any vsg files
        for (int i = 1; i < argc; ++i)
        {
            vsg::Path filename = arguments[i];
            if (auto node = vsg::read_cast<vsg::Node>(filename, options); node)
            {
                group->addChild(node);
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

        // record the total time taken loading the scene graph
        loadDuration = std::chrono::duration<double, std::chrono::milliseconds::period>(vsg::clock::now() - startOfLoad).count();

        if (stats > 0)
        {
            auto startOfStats = vsg::clock::now();

            auto sceneStatistics = SceneStatistics::create();

            for (size_t i = 0; i < stats; ++i)
            {
                sceneStatistics->objectCounts.clear();
                vsg_scene->accept(*sceneStatistics);
            }

            auto statsDuration = std::chrono::duration<double, std::chrono::milliseconds::period>(vsg::clock::now() - startOfStats).count();

            std::cout << "Stats collection took " << statsDuration << "ms"
                      << " for " << stats << " traversals." << std::endl;
            sceneStatistics->report(std::cout);
        }

        if (useViewer)
        {
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
            double nearFarRatio = 0.001;

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

                auto startTime = std::chrono::steady_clock::now();

                vsg_scene->accept(loadPagedLOD);

                auto time = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::steady_clock::now() - startTime).count();
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

            auto startOfFrameLopp = vsg::clock::now();

            // rendering main loop
            while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
            {
                // pass any events into EventHandlers assigned to the Viewer
                viewer->handleEvents();

                viewer->update();

                viewer->recordAndSubmit();

                viewer->present();

                if (reportAtEndOfAllFrames)
                {
                    vsg::Allocator::instance()->report(std::cout);
                }
            }

            if (viewer->getFrameStamp()->frameCount > 0)
            {
                auto duration = std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - startOfFrameLopp).count();
                frameRate = (double(viewer->getFrameStamp()->frameCount) / duration);
            }
        }

        std::cout << "\nBefore end of Viewer scope." << std::endl;
        vsg::Allocator::instance()->report(std::cout);

        // record the end of viewer scope
        endOfViewerScope = vsg::clock::now();
    }
    catch (const vsg::Exception& ve)
    {
        for (int i = 0; i < argc; ++i) std::cerr << argv[i] << " ";
        std::cerr << "\n[Exception] - " << ve.message << " result = " << ve.result << std::endl;
        return 1;
    }

    // to make sure everything cleans up for stats purposes we'll unref the ObjectFactory, it's perfectly safe to not have this step in your applications
    vsg::ObjectFactory::instance() = {};

    double releaseDuration = std::chrono::duration<double, std::chrono::milliseconds::period>(vsg::clock::now() - endOfViewerScope).count();

    std::cout << "\nAfter end of Viewer scope." << std::endl;
    vsg::Allocator::instance()->report(std::cout);

    // Optional call to delete any empty memory blocks, this won't normally be required in a VSG application, but if your memory usage goes up and down regularly and you want to free up memory
    // for use elsewhere then you can call vsg::Allocator::instance()->deleteEmptyMemoryBlocks() after you delete scene graph nodes, data and objects to make sure any memory blocks that may now be empty can be
    // released back to the OS. If you don't call deleteEmptyMemoryBlocks() the vsg::Allocator destructor will do all the clean up for you on exit from the application.
    auto beforeDelete = vsg::clock::now();
    auto memoryDeleted = vsg::Allocator::instance()->deleteEmptyMemoryBlocks();
    double deleteDuration = std::chrono::duration<double, std::chrono::milliseconds::period>(vsg::clock::now() - beforeDelete).count();

    std::cout << "\nAfter delete of empty memory blocks, where " << memoryDeleted << " was freed." << std::endl;
    vsg::Allocator::instance()->report(std::cout);

    std::cout << "\nload duration = " << loadDuration << "ms" << std::endl;
    std::cout << "release duration  = " << releaseDuration << "ms" << std::endl;
    std::cout << "delete duration  = " << deleteDuration << "ms" << std::endl;
    std::cout << "Average frame rate = " << frameRate << "fps" << std::endl;
    return 0;
}
