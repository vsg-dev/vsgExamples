#include <vsg/all.h>

#ifdef USE_VSGXCHANGE
#    include <vsgXchange/ReaderWriter_all.h>
#endif

#ifdef USE_VSGGIS
#    include <vsgGIS/ReaderWriter_GDAL.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#include "../../shared/AnimationPath.h"

// 1. Load data
// 2. compile data.
// 3. submit compile data.
// 4. wait for submission to complete
// 5. add to main scene graph

class DynamicLoadAndCompile : public vsg::Inherit<vsg::Object, DynamicLoadAndCompile>
{
public:

    vsg::ref_ptr<vsg::ActivityStatus> status;
    vsg::ref_ptr<vsg::OperationQueue> loadQueue;
    vsg::ref_ptr<vsg::OperationQueue> compileQueue;
    vsg::ref_ptr<vsg::OperationQueue> mergeQueue;

    DynamicLoadAndCompile(vsg::ref_ptr<vsg::ActivityStatus> in_status = vsg::ActivityStatus::create()) :
        status(in_status)
    {
        loadQueue = vsg::OperationQueue::create(status);
        compileQueue = vsg::OperationQueue::create(status);
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

        void run() override
        {
            vsg::ref_ptr<DynamicLoadAndCompile> dynamicLoadAndCompile(dlac);
            if (!dynamicLoadAndCompile) return;

            std::cout<<"Loading "<<request->filename<<std::endl;

            if (auto node = vsg::read_cast<vsg::Node>(request->filename, request->options); node)
            {
                vsg::ComputeBounds computeBounds;
                node->accept(computeBounds);

                vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
                double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5;
                auto scale = vsg::MatrixTransform::create(vsg::scale(1.0 / radius, 1.0 / radius, 1.0 / radius) * vsg::translate(-centre));

                scale->addChild(node);

                request->loaded = scale;

                dynamicLoadAndCompile->compileRequest(request);
            }
        }
    };

    struct CompileOperation : public vsg::Inherit<vsg::Operation, CompileOperation>
    {
        CompileOperation(vsg::ref_ptr<Request> in_request, vsg::observer_ptr<DynamicLoadAndCompile> in_dlac) :
            request(in_request),
            dlac(in_dlac) {}

        vsg::ref_ptr<Request> request;
        vsg::observer_ptr<DynamicLoadAndCompile> dlac;

        void run() override
        {
            vsg::ref_ptr<DynamicLoadAndCompile> dynamicLoadAndCompile(dlac);
            if (!dynamicLoadAndCompile) return;

            std::cout<<"Compiling "<<request->filename<<std::endl;

            dynamicLoadAndCompile->mergeRequest(request);
        }
    };

    struct MergeOperation : public vsg::Inherit<vsg::Operation, MergeOperation>
    {
        MergeOperation(vsg::ref_ptr<Request> in_request, vsg::observer_ptr<DynamicLoadAndCompile> in_dlac) :
            request(in_request),
            dlac(in_dlac) {}

        vsg::ref_ptr<Request> request;
        vsg::observer_ptr<DynamicLoadAndCompile> dlac;

        void run() override
        {
            std::cout<<"Merging "<<request->filename<<std::endl;

            request->attachmentPoint->addChild(request->loaded);
        }
    };

    void loadRequest(const vsg::Path& filename, vsg::ref_ptr<vsg::Group> attachmentPoint, vsg::ref_ptr<vsg::Options> options)
    {
        auto request = Request::create(filename, attachmentPoint, options);
        loadQueue->add(LoadOperation::create(request, vsg::observer_ptr<DynamicLoadAndCompile>(this)));
    }

    void compileRequest(vsg::ref_ptr<Request> request)
    {
        compileQueue->add(CompileOperation::create(request, vsg::observer_ptr<DynamicLoadAndCompile>(this)));
    }

    void mergeRequest(vsg::ref_ptr<Request> request)
    {
        mergeQueue->add(MergeOperation::create(request, vsg::observer_ptr<DynamicLoadAndCompile>(this)));
    }

    void load()
    {
        std::cout<<"\nstarting load"<<std::endl;
        vsg::ref_ptr<vsg::Operation> operation;
        while(operation = loadQueue->take())
        {
            operation->run();
        }
    }

    void compile()
    {
        std::cout<<"\nstarting compile"<<std::endl;
        vsg::ref_ptr<vsg::Operation> operation;
        while(operation = compileQueue->take())
        {
            operation->run();
        }
    }

    void merge()
    {
        std::cout<<"\nstarting merge"<<std::endl;
        vsg::ref_ptr<vsg::Operation> operation;
        while(operation = mergeQueue->take())
        {
            operation->run();
        }
    }
};

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // set up vsg::Options to pass in filepaths and ReaderWriter's and other IO realted options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef USE_VSGXCHANGE
        options->add(vsgXchange::ReaderWriter_all::create()); // add the optional ReaderWriter_all fron vsgXchange to read 3d models and imagery
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

        std::vector<std::pair<vsg::Path, vsg::ref_ptr<vsg::Group>>> filenameAttachments;

        vsg::dvec3 origin(0.0, 0.0, 0.0);
        vsg::dvec3 primary(2.0, 0.0, 0.0);
        vsg::dvec3 secondary(0.0, 2.0, 0.0);

        int numColumns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(argc - 1))));

        // create a Group to contain all the nodes
        auto vsg_scene = vsg::Group::create();

        // Assign any ResourceHints so that the Compile traversal can allocate suffucient DescriptorPool resources for the needs of loading all possible models.
        if (resourceHints)
        {
            vsg_scene->setObject("ResourceHints", resourceHints);
        }

        auto dynamicLoadAndCompile = DynamicLoadAndCompile::create();

        // build the sene graph attachmments points to place all of the loaded models at.
        for (int i = 1; i < argc; ++i)
        {
            int index = i - 1;
            vsg::dvec3 position = origin + primary * static_cast<double>(index % numColumns) + secondary * static_cast<double>(index / numColumns);
            auto transform = vsg::MatrixTransform::create(vsg::translate(position));

            vsg_scene->addChild(transform);

            dynamicLoadAndCompile->loadRequest(argv[i], transform, options);
        }

        // merge any loaded scene graph elements with the main scene graph
        dynamicLoadAndCompile->load();
        dynamicLoadAndCompile->compile();
        dynamicLoadAndCompile->merge();

        // vsg::write(vsg_scene, "test.vsgt");

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        vsg::ref_ptr<vsg::Window> window(vsg::Window::create(windowTraits));
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
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
        auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // add close handler to respond the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        viewer->addEventHandler(vsg::Trackball::create(camera));

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
        viewer->compile();

        for (auto& task : viewer->recordAndSubmitTasks)
        {
            std::cout << task << std::endl;
            std::cout << "  windows.size() = " << task->windows.size() << std::endl;
            for (auto& window : task->windows)
            {
                std::cout << "    " << window << std::endl;
            }

            std::cout << "  waitSemaphores.size() " << task->waitSemaphores.size() << std::endl;
            for (auto& semaphore : task->waitSemaphores)
            {
                std::cout << "    " << semaphore << std::endl;
            }

            std::cout << "  commandGraphs.size() = " << task->commandGraphs.size() << std::endl;
            for (auto& cg : task->commandGraphs)
            {
                std::cout << "    " << cg << std::endl;
            }

            std::cout << "  signalSemaphores.size() = " << task->signalSemaphores.size() << std::endl;
            for (auto& semaphore : task->signalSemaphores)
            {
                std::cout << "    " << semaphore << std::endl;
            }
        }

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

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
