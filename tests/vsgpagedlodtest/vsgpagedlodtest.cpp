#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // create windowTraits using the any command line arugments to configure settings
        auto windowTraits = vsg::WindowTraits::create(arguments);

        // set up vsg::Options to pass in filepaths, ReaderWriters and other IO related options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());
#endif

        auto maxPagedLOD = arguments.value(1500, "--maxPagedLOD");
        auto useSharedObjects = arguments.value(false, "--shareObjects");

        options->readOptions(arguments);

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        if (argc <= 1)
        {
            std::cout << "Please specify 3d-models on the command line." << std::endl;
            return 1;
        }

        vsg::dvec3 center(0.0, 0.0, 0.0);
        vsg::dvec3 shift(0.0, 2.0, 0.0);
        auto vsg_scene = vsg::Group::create();

        // read any 3d-models files
        for (int i = 1; i < argc; ++i)
        {
            vsg::Path filename = arguments[i];

            auto object = vsg::read(filename, options);
            if (vsg::fileExists(filename))
            {
                int index = (i - 1);
                vsg::dvec3 position = shift * static_cast<double>(index);
                auto transform = vsg::MatrixTransform::create(vsg::translate(position));

                auto pagedLOD = vsg::PagedLOD::create();
                pagedLOD->filename = filename;
                pagedLOD->options = options;
                pagedLOD->bound = vsg::dsphere(center, 1.0);
                transform->addChild(pagedLOD);
                vsg_scene->addChild(transform);

                std::cout << "Sucessfully added pagedLOD for file " << filename << std::endl;
            }
            else
            {
                std::cout << "Unable to find file " << filename << std::endl;
            }
        }

        if (vsg_scene->children.empty())
        {
            std::cout << "No 3d models added" << std::endl;
            return 1;
        }

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create window." << std::endl;
            return 1;
        }

        viewer->addWindow(window);

        // compute position the camera
        vsg::dvec3 eyeShift(0.0, -5, 2.0);
        vsg::dvec3 up(0.0, 0.0, 1.0);
        auto lookAt = vsg::LookAt::create(center + eyeShift, center, up);

        auto perspective = vsg::Perspective::create(30.0,
                                                    static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
                                                    0.01,
                                                    1000.0);
        auto viewportState = vsg::ViewportState::create(window->extent2D());
        auto camera = vsg::Camera::create(perspective, lookAt, viewportState);

        // add close handler to respond to the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        viewer->addEventHandler(vsg::Trackball::create(camera));
        auto view = vsg::View::create(camera);
        view->addChild(vsg::createHeadlight());
        view->addChild(vsg_scene);

        auto renderGraph = vsg::RenderGraph::create(window, view);
        auto commandGraph = vsg::CommandGraph::create(window, renderGraph);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        viewer->compile();

        // set targetMaxNumPagedLODWithHighResSubgraphs after Viewer::compile() as it will assign any DatabasePager if required.
        if (maxPagedLOD >= 0)
        {
            for (auto& task : viewer->recordAndSubmitTasks)
            {
                if (task->databasePager) task->databasePager->targetMaxNumPagedLODWithHighResSubgraphs = maxPagedLOD;
                std::cout << "Applied databasePager->targetMaxNumPagedLODWithHighResSubgraphs = " << maxPagedLOD << std::endl;
            }
        }

        if (useSharedObjects)
        {
            options->sharedObjects = vsg::SharedObjects::create();
        }

        viewer->start_point() = vsg::clock::now();

        // rendering main loop
        while (viewer->advanceToNextFrame())
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
