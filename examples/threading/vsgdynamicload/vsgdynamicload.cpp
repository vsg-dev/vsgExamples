#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

class DynamicLoadAndCompile : public vsg::Inherit<vsg::Object, DynamicLoadAndCompile>
{
public:
    vsg::ref_ptr<vsg::ActivityStatus> status;

    vsg::ref_ptr<vsg::OperationThreads> loadThreads;
    vsg::ref_ptr<vsg::OperationThreads> compileThreads;
    vsg::ref_ptr<vsg::OperationQueue> mergeQueue;

    std::mutex mutex_compileTraversals;
    std::list<vsg::ref_ptr<vsg::CompileTraversal>> compileTraversals;

    // window related settings used to set up the CompileTraversal
    vsg::ref_ptr<vsg::Window> window;
    vsg::ref_ptr<vsg::ViewportState> viewport;
    vsg::ResourceRequirements resourceRequirements;

    DynamicLoadAndCompile(vsg::ref_ptr<vsg::Window> in_window, vsg::ref_ptr<vsg::ViewportState> in_viewport, vsg::ref_ptr<vsg::ActivityStatus> in_status = vsg::ActivityStatus::create()) :
        status(in_status),
        window(in_window),
        viewport(in_viewport)
    {
        loadThreads = vsg::OperationThreads::create(12, status);
        compileThreads = vsg::OperationThreads::create(1, status);
        mergeQueue = vsg::OperationQueue::create(status);
    }

    struct Request : public vsg::Inherit<vsg::Object, Request>
    {
        Request(const vsg::Path& in_filename, vsg::ref_ptr<vsg::Group> in_attachmentPoint, vsg::ref_ptr<vsg::Options> in_options) :
            filename(in_filename),
            attachmentPoint(in_attachmentPoint),
            options(in_options) {}

        vsg::Path filename;
        vsg::ref_ptr<vsg::Group> attachmentPoint;
        vsg::ref_ptr<vsg::Options> options;
        vsg::ref_ptr<vsg::Node> loaded;
    };

    struct LoadOperation : public vsg::Inherit<vsg::Operation, LoadOperation>
    {
        LoadOperation(vsg::ref_ptr<Request> in_request, vsg::observer_ptr<DynamicLoadAndCompile> in_dlac) :
            request(in_request),
            dlac(in_dlac) {}

        vsg::ref_ptr<Request> request;
        vsg::observer_ptr<DynamicLoadAndCompile> dlac;

        void run() override;
    };

    struct CompileOperation : public vsg::Inherit<vsg::Operation, CompileOperation>
    {
        CompileOperation(vsg::ref_ptr<Request> in_request, vsg::observer_ptr<DynamicLoadAndCompile> in_dlac) :
            request(in_request),
            dlac(in_dlac) {}

        vsg::ref_ptr<Request> request;
        vsg::observer_ptr<DynamicLoadAndCompile> dlac;

        void run() override;
    };

    struct MergeOperation : public vsg::Inherit<vsg::Operation, MergeOperation>
    {
        MergeOperation(vsg::ref_ptr<Request> in_request, vsg::observer_ptr<DynamicLoadAndCompile> in_dlac) :
            request(in_request),
            dlac(in_dlac) {}

        vsg::ref_ptr<Request> request;
        vsg::observer_ptr<DynamicLoadAndCompile> dlac;

        void run() override;
    };

    void loadRequest(const vsg::Path& filename, vsg::ref_ptr<vsg::Group> attachmentPoint, vsg::ref_ptr<vsg::Options> options)
    {
        auto request = Request::create(filename, attachmentPoint, options);
        loadThreads->add(LoadOperation::create(request, vsg::observer_ptr<DynamicLoadAndCompile>(this)));
    }

    void compileRequest(vsg::ref_ptr<Request> request)
    {
        compileThreads->add(CompileOperation::create(request, vsg::observer_ptr<DynamicLoadAndCompile>(this)));
    }

    void mergeRequest(vsg::ref_ptr<Request> request)
    {
        mergeQueue->add(MergeOperation::create(request, vsg::observer_ptr<DynamicLoadAndCompile>(this)));
    }

    vsg::ref_ptr<vsg::CompileTraversal> takeCompileTraversal()
    {
        {
            std::scoped_lock lock(mutex_compileTraversals);
            if (!compileTraversals.empty())
            {
                auto ct = compileTraversals.front();
                compileTraversals.erase(compileTraversals.begin());
                std::cout << "takeCompileTraversal() resuming " << ct << std::endl;
                return ct;
            }
        }

        std::cout << "takeCompileTraversal() creating a new CompileTraversal" << std::endl;
        auto ct = vsg::CompileTraversal::create(window, viewport, resourceRequirements);

        return ct;
    }

    void addCompileTraversal(vsg::ref_ptr<vsg::CompileTraversal> ct)
    {
        std::cout << "addCompileTraversal(" << ct << ")" << std::endl;
        std::scoped_lock lock(mutex_compileTraversals);
        compileTraversals.push_back(ct);
    }

    void merge()
    {
        vsg::ref_ptr<vsg::Operation> operation;
        while (operation = mergeQueue->take())
        {
            operation->run();
        }
    }
};

void DynamicLoadAndCompile::LoadOperation::run()
{
    vsg::ref_ptr<DynamicLoadAndCompile> dynamicLoadAndCompile(dlac);
    if (!dynamicLoadAndCompile) return;

    std::cout << "Loading " << request->filename << std::endl;

    if (auto node = vsg::read_cast<vsg::Node>(request->filename, request->options); node)
    {
        vsg::ComputeBounds computeBounds;
        node->accept(computeBounds);

        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5;
        auto scale = vsg::MatrixTransform::create(vsg::scale(1.0 / radius, 1.0 / radius, 1.0 / radius) * vsg::translate(-centre));

        scale->addChild(node);

        request->loaded = scale;

        std::cout << "Loaded " << request->filename << std::endl;

        dynamicLoadAndCompile->compileRequest(request);
    }
}

void DynamicLoadAndCompile::CompileOperation::run()
{
    vsg::ref_ptr<DynamicLoadAndCompile> dynamicLoadAndCompile(dlac);
    if (!dynamicLoadAndCompile) return;

    if (request->loaded)
    {
        std::cout << "Compiling " << request->filename << std::endl;

        auto compileTraversal = dynamicLoadAndCompile->takeCompileTraversal();

        vsg::CollectResourceRequirements collectRequirements;
        request->loaded->accept(collectRequirements);

        auto maxSets = collectRequirements.requirements.computeNumDescriptorSets();
        auto descriptorPoolSizes = collectRequirements.requirements.computeDescriptorPoolSizes();

        // brute force allocation of new DescrptorPool for this subgraph, TODO : need to preallocate large DescritorPoil for multiple loaded subgraphs
        if (descriptorPoolSizes.size() > 0) compileTraversal->context.descriptorPool = vsg::DescriptorPool::create(compileTraversal->context.device, maxSets, descriptorPoolSizes);

        request->loaded->accept(*compileTraversal);

        std::cout << "Finished compile traversal " << request->filename << std::endl;

        compileTraversal->context.record(); // records and submits to queue

        compileTraversal->context.waitForCompletion();

        std::cout << "Finished waiting for compile " << request->filename << std::endl;

        dynamicLoadAndCompile->mergeRequest(request);

        dynamicLoadAndCompile->addCompileTraversal(compileTraversal);
    }
}

void DynamicLoadAndCompile::MergeOperation::run()
{
    std::cout << "Merging " << request->filename << std::endl;

    request->attachmentPoint->addChild(request->loaded);
}

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // set up vsg::Options to pass in filepaths and ReaderWriter's and other IO related options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());
#endif

        arguments.read(options);

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "vsgdynamicload";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
        arguments.read("--screen", windowTraits->screenNum);
        arguments.read("--display", windowTraits->display);
        auto numFrames = arguments.value(-1, "-f");

        // provide setting of the resource hints on the command line
        vsg::ref_ptr<vsg::ResourceHints> resourceHints;
        if (vsg::Path resourceFile; arguments.read("--resource", resourceFile)) resourceHints = vsg::read_cast<vsg::ResourceHints>(resourceFile);

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        if (argc <= 1)
        {
            std::cout << "Please specify a 3d models on the command line." << std::endl;
            return 1;
        }

        // create a Group to contain all the nodes
        auto vsg_scene = vsg::Group::create();

        // Assign any ResourceHints so that the Compile traversal can allocate sufficient DescriptorPool resources for the needs of loading all possible models.
        if (resourceHints)
        {
            vsg_scene->setObject("ResourceHints", resourceHints);
        }

        vsg::ref_ptr<vsg::Window> window(vsg::Window::create(windowTraits));
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        viewer->addWindow(window);

        // set up the grid dimensions to place the loaded models on.
        vsg::dvec3 origin(0.0, 0.0, 0.0);
        vsg::dvec3 primary(2.0, 0.0, 0.0);
        vsg::dvec3 secondary(0.0, 2.0, 0.0);

        int numModels = static_cast<float>(argc - 1);
        int numColumns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(numModels))));
        int numRows = static_cast<int>(std::ceil(static_cast<float>(numModels) / static_cast<float>(numColumns)));

        // compute the bounds of the scene graph to help position camera
        vsg::dvec3 centre = origin + primary * (static_cast<double>(numColumns - 1) * 0.5) + secondary * (static_cast<double>(numRows - 1) * 0.5);
        double viewingDistance = std::sqrt(static_cast<float>(numModels)) * 3.0;
        double nearFarRatio = 0.001;

        // set up the camera
        auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -viewingDistance, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
        auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * viewingDistance, viewingDistance * 2.0);
        auto viewportState = vsg::ViewportState::create(window->extent2D());
        auto camera = vsg::Camera::create(perspective, lookAt, viewportState);

        // add close handler to respond the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        viewer->addEventHandler(vsg::Trackball::create(camera));

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        // create the DynamicLoadAndCompile object that manages loading, compile and merging of new objects.
        // Pass in window and viewportState to help initialize CompilTraversals
        auto dynamicLoadAndCompile = DynamicLoadAndCompile::create(window, viewportState, viewer->status);

        // build the scene graph attachments points to place all of the loaded models at.
        for (int i = 1; i < argc; ++i)
        {
            int index = i - 1;
            vsg::dvec3 position = origin + primary * static_cast<double>(index % numColumns) + secondary * static_cast<double>(index / numColumns);
            auto transform = vsg::MatrixTransform::create(vsg::translate(position));

            vsg_scene->addChild(transform);

            dynamicLoadAndCompile->loadRequest(argv[i], transform, options);
        }

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            dynamicLoadAndCompile->merge();

            viewer->update();

            viewer->recordAndSubmit();

            viewer->present();
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
