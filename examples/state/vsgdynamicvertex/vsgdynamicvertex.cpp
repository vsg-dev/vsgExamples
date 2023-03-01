#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

class FindVertexData : public vsg::Visitor
{
public:

    void apply(vsg::Object& object)
    {
        object.traverse(*this);
    }

    void apply(vsg::Geometry& geometry)
    {
        if (geometry.arrays.empty()) return;
        geometry.arrays[0]->data->accept(*this);
    }

    void apply(vsg::VertexIndexDraw& vid)
    {
        if (vid.arrays.empty()) return;
        vid.arrays[0]->data->accept(*this);
    }

    void apply(vsg::BindVertexBuffers& bvd)
    {
        if (bvd.arrays.empty()) return;
        bvd.arrays[0]->data->accept(*this);
    }

    void apply(vsg::vec3Array& vertices)
    {
        if (verticesSet.count(&vertices) == 0)
        {
            verticesSet.insert(&vertices);
        }
    }


    std::vector<vsg::ref_ptr<vsg::vec3Array>> getVerticesList()
    {
        std::vector<vsg::ref_ptr<vsg::vec3Array>> verticesList(verticesSet.size());
        auto vertices_itr = verticesList.begin();
        for(auto& vertices : verticesSet)
        {
            (*vertices_itr++) = const_cast<vsg::vec3Array*>(vertices);
        }

        return verticesList;
    }

    std::set<vsg::vec3Array*> verticesSet;
};

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // set up vsg::Options to pass in filepaths and ReaderWriter's and other IO related options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());
#endif

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "vsgdynamicvertex";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if (arguments.read("--double-buffer")) windowTraits->swapchainPreferences.imageCount = 2;
        if (arguments.read("--triple-buffer")) windowTraits->swapchainPreferences.imageCount = 3; // default
        if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
        arguments.read("--screen", windowTraits->screenNum);
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
        auto outputFilename = arguments.value<vsg::Path>("", "-o");

        auto numFrames = arguments.value(-1, "-f");

        bool multiThreading = arguments.read("--mt");
        auto modify = arguments.read("--modify");
        auto dirty = arguments.read("--dirty");
        auto dynamic = arguments.read("--dynamic") || dirty || modify;
        bool lateTransfer = arguments.read("--late");

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        if (argc <= 1)
        {
            std::cout << "Please specify a 3d model on the command line." << std::endl;
            return 1;
        }

        vsg::Path filename = arguments[1];
        auto vsg_scene = vsg::read_cast<vsg::Node>(filename, options);
        if (!vsg_scene)
        {
            std::cout << "Unable to load file " << filename << std::endl;
            return 1;
        }

        // visit the scene graph to collect all the vertex arrays;
        size_t numVertices = 0;
        auto verticesList = vsg::visit<FindVertexData>(vsg_scene).getVerticesList();
        if (dynamic)
        {
            for(auto& vertices : verticesList)
            {
                vertices->properties.dataVariance = lateTransfer ? vsg::DYNAMIC_DATA_TRANSFER_AFTER_RECORD : vsg::DYNAMIC_DATA;
                numVertices += vertices->size();
            }
        }

        vsg::info("number of dynamic vertex arrays : ", verticesList.size());
        vsg::info("number of dynamic vertices : ", numVertices);
        vsg::info("size of dynamic vertices : ", numVertices * sizeof(vsg::vec3));

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        auto window = vsg::Window::create(windowTraits);
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

        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
        if (ellipsoidModel)
        {
            perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, 0.0);
        }
        else
        {
            perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        // add trackball to control the Camera
        viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

        // set up commandGraph for rendering
        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);

        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});


        vsg::info("multiThreading = ", multiThreading);
        if (multiThreading) viewer->setupThreading();

        viewer->compile();

        auto startTime = std::chrono::steady_clock::now();
        double frameCount = 0.0;

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
        {
            ++frameCount;

            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->update();

            if (modify)
            {
                for(auto& vertices : verticesList)
                {
                    for(auto& v : *vertices)
                    {
                        v.z += (sin(vsg::PI * frameCount / 180.0) * radius * 0.001);
                    }
                    vertices->dirty();
                }
            }
            else if (dirty)
            {
                for(auto& vertices : verticesList)
                {
                    vertices->dirty();
                }
            }

            viewer->recordAndSubmit();
            viewer->present();
        }

        auto fps = frameCount / (std::chrono::duration<double, std::chrono::seconds::period>(std::chrono::steady_clock::now() - startTime).count());
        double transferSpeed = (double)(numVertices * sizeof(vsg::vec3) * fps);
        std::cout << "Average fps = " << fps << std::endl;
        std::cout << "Average transfer speed " <<(transferSpeed) / (1024.0 * 1024.0) << " Mb/sec"<<std::endl;
    }
    catch (const vsg::Exception& exception)
    {
        std::cout << exception.message << " VkResult = " << exception.result << std::endl;
        return 0;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
