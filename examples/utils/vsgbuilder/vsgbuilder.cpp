#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->objectCache = vsg::ObjectCache::create();

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgbuilder";

    auto builder = vsg::Builder::create();
    builder->options = options;

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});

    vsg::GeometryInfo geomInfo;
    geomInfo.dx.set(1.0f, 0.0f, 0.0f);
    geomInfo.dy.set(0.0f, 1.0f, 0.0f);
    geomInfo.dz.set(0.0f, 0.0f, 1.0f);

    vsg::StateInfo stateInfo;

    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    auto numFrames = arguments.value(-1, "-f");
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--double-buffer")) windowTraits->swapchainPreferences.imageCount = 2;
    if (arguments.read("--triple-buffer")) windowTraits->swapchainPreferences.imageCount = 3; // default
    if (arguments.read("-t"))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->width = 192, windowTraits->height = 108;
        windowTraits->decoration = false;
    }

    auto outputFilename = arguments.value<std::string>("", "-o");

    bool floatColors = !arguments.read("--ubvec4-colors");
    stateInfo.wireframe = arguments.read("--wireframe");
    stateInfo.lighting = !arguments.read("--flat");
    stateInfo.doubleSided = arguments.read("--two-sided");

    arguments.read("--dx", geomInfo.dx);
    arguments.read("--dy", geomInfo.dy);
    arguments.read("--dz", geomInfo.dz);

    bool box = arguments.read("--box");
    bool capsule = arguments.read("--capsule");
    bool cone = arguments.read("--cone");
    bool cylinder = arguments.read("--cylinder");
    bool disk = arguments.read("--disk");
    bool quad = arguments.read("--quad");
    bool sphere = arguments.read("--sphere");
    bool heightfield = arguments.read("--hf");

    if (!(box || sphere || cone || capsule || quad || cylinder || disk || heightfield))
    {
        box = true;
        capsule = true;
        cone = true;
        cylinder = true;
        disk = true;
        quad = true;
        sphere = true;
        heightfield = true;
    }

    auto numVertices = arguments.value<uint32_t>(0, "-n");

    vsg::Path textureFile = arguments.value(vsg::Path{}, {"-i", "--image"});
    vsg::Path displacementFile = arguments.value(vsg::Path{}, "--dm");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto scene = vsg::Group::create();

    vsg::dvec3 centre = {0.0, 0.0, 0.0};
    double radius = 1.0;

    {
        radius = vsg::length(geomInfo.dx + geomInfo.dy + geomInfo.dz);

        //geomInfo.transform = vsg::perspective(vsg::radians(60.0f), 2.0f, 1.0f, 10.0f);
        //geomInfo.transform = vsg::inverse(vsg::perspective(vsg::radians(60.0f), 1920.0f/1080.0f, 1.0f, 100.0f)  * vsg::translate(0.0f, 0.0f, -1.0f) * vsg::scale(1.0f, 1.0f, 2.0f));
        //geomInfo.transform = vsg::rotate(vsg::radians(0.0), 0.0, 0.0, 1.0);

        if (!textureFile.empty()) stateInfo.image = vsg::read_cast<vsg::Data>(textureFile, options);
        if (!displacementFile.empty()) stateInfo.displacementMap = vsg::read_cast<vsg::Data>(displacementFile, options);

        vsg::dbox bound;

        if (numVertices > 0)
        {
            stateInfo.instancce_positions_vec3 = true;

            float w = std::pow(float(numVertices), 0.33f) * 2.0f;
            geomInfo.positions = vsg::vec3Array::create(numVertices);
            for (auto& v : *(geomInfo.positions))
            {
                v.set(w * (float(std::rand()) / float(RAND_MAX) - 0.5f),
                      w * (float(std::rand()) / float(RAND_MAX) - 0.5f),
                      w * (float(std::rand()) / float(RAND_MAX) - 0.5f));
            }

            radius += (0.5 * sqrt(3.0) * w);

            if (floatColors)
            {
                auto colors = vsg::vec4Array::create(geomInfo.positions->size());
                geomInfo.colors = colors;
                for (auto& c : *(colors))
                {
                    c.set(float(std::rand()) / float(RAND_MAX), float(std::rand()) / float(RAND_MAX), float(std::rand()) / float(RAND_MAX), 1.0f);
                }
            }
            else
            {
                auto colors = vsg::ubvec4Array::create(geomInfo.positions->size());
                geomInfo.colors = colors;
                for (auto& c : *(colors))
                {
                    c.set(uint8_t(255.0 * float(std::rand()) / float(RAND_MAX)), uint8_t(255.0 * float(std::rand()) / float(RAND_MAX)), uint8_t(255.0 * float(std::rand()) / float(RAND_MAX)), 255);
                }
            }
        }

        if (box)
        {
            scene->addChild(builder->createBox(geomInfo, stateInfo));
            bound.add(geomInfo.position);
            geomInfo.position += geomInfo.dx * 1.5f;
        }

        if (sphere)
        {
            scene->addChild(builder->createSphere(geomInfo, stateInfo));
            bound.add(geomInfo.position);
            geomInfo.position += geomInfo.dx * 1.5f;
        }

        if (quad)
        {
            scene->addChild(builder->createQuad(geomInfo, stateInfo));
            bound.add(geomInfo.position);
            geomInfo.position += geomInfo.dx * 1.5f;
        }

        if (disk)
        {
            scene->addChild(builder->createDisk(geomInfo, stateInfo));
            bound.add(geomInfo.position);
            geomInfo.position += geomInfo.dx * 1.5f;
        }

        if (cylinder)
        {
            scene->addChild(builder->createCylinder(geomInfo, stateInfo));
            bound.add(geomInfo.position);
            geomInfo.position += geomInfo.dx * 1.5f;
        }

        if (cone)
        {
            scene->addChild(builder->createCone(geomInfo, stateInfo));
            bound.add(geomInfo.position);
            geomInfo.position += geomInfo.dx * 1.5f;
        }

        if (capsule)
        {
            scene->addChild(builder->createCapsule(geomInfo, stateInfo));
            bound.add(geomInfo.position);
            geomInfo.position += geomInfo.dx * 1.5f;
        }

        if (heightfield)
        {
            scene->addChild(builder->createHeightField(geomInfo, stateInfo));
            bound.add(geomInfo.position);
        }

        // update the centre and radius to account for all the shapes added so we can position the camera to see them all.
        centre = (bound.min + bound.max) * 0.5;
        radius += vsg::length(bound.max - bound.min) * 0.5;
    }

    // write out scene if required
    if (!outputFilename.empty())
    {
        vsg::write(scene, outputFilename, options);
        return 0;
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

    vsg::ref_ptr<vsg::LookAt> lookAt;

    // compute the bounds of the scene graph to help position camera
    //vsg::ComputeBounds computeBounds;
    //scene->accept(computeBounds);
    //vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    //double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6 * 10.0;

    // set up the camera
    lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    double nearFarRatio = 0.001;
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.0);

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // set up the compilation support in builder to allow us to interactively create and compile subgraphs from within the IntersectionHandler
    // builder->setup(window, camera->viewportState);

    // add close handler to respond the close window button and pressing escape
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
