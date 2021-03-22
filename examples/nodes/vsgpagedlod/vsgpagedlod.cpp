#include <vsg/all.h>

#include <vsgXchange/all.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#include "../../shared/AnimationPath.h"

#include "TileReader.h"
#include "GlobeTrackball.h"

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // set up vsg::Options to pass in filepaths and ReaderWriter's and other IO realted options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

        auto tileReader = TileReader::create();
        options->add(tileReader);

        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());

        arguments.read(options);

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "vsgpagedlod";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
        arguments.read("--screen", windowTraits->screenNum);
        arguments.read("--display", windowTraits->display);
        arguments.read("--samples", windowTraits->samples);
        auto outputFilename = arguments.value(std::string(), "-o");
        auto numFrames = arguments.value(-1, "-f");
        auto pathFilename = arguments.value(std::string(), "-p");
        auto loadLevels = arguments.value(0, "--load-levels");
        auto horizonMountainHeight = arguments.value(0.0, "--hmh");
        auto mipmapLevelsHint = arguments.value<uint32_t>(0, {"--mipmapLevels", "--mml"});
        auto oldTrackball = arguments.read("--trackball");
        if (arguments.read("--rgb")) options->mapRGBtoRGBAHint = false;

        if (arguments.read("--osm"))
        {
            tileReader->noX = 1;
            tileReader->noY = 1;
            tileReader->originTopLeft = true;
            tileReader->projection = "EPSG:3857"; // spherical-mercator
            tileReader->imageLayer = "https://a.tile.openstreetmap.org/{z}/{x}/{y}.png";
        }

        if (arguments.read("--rm") || tileReader->imageLayer.empty())
        {
            // setup ready mapp settings
            tileReader->noX = 2;
            tileReader->noY = 1;
            tileReader->originTopLeft = false;
            tileReader->imageLayer = "http://readymap.org/readymap/tiles/1.0.0/7/{z}/{x}/{y}.jpeg";
            // tileReader->terrainLayer = "http://readymap.org/readymap/tiles/1.0.0/116/{z}/{x}/{y}.tif";
        }

        const double invalid_value = std::numeric_limits<double>::max();
        double poi_latitude = invalid_value;
        double poi_longitude = invalid_value;
        double poi_distance = invalid_value;
        while(arguments.read("--poi", poi_latitude, poi_longitude)) {};
        while(arguments.read("--distance", poi_distance)) {};

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        // initial the state that will be shared between tiles.
        tileReader->init();

        // load the root tile.
        auto vsg_scene = vsg::read_cast<vsg::Node>("root.tile", options);
        if (!vsg_scene) return 1;

        if (!outputFilename.empty())
        {
            vsg::write(vsg_scene, outputFilename);
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

        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
        double nearFarRatio = 0.0005;

        // set up the camera
        vsg::ref_ptr<vsg::LookAt> lookAt;
        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel(vsg_scene->getObject<vsg::EllipsoidModel>("EllipsoidModel"));
        if (ellipsoidModel)
        {
            if (poi_latitude != invalid_value && poi_longitude != invalid_value)
            {
                double height = (poi_distance != invalid_value) ? poi_distance : radius*3.5 ;
                auto ecef = ellipsoidModel->convertLatLongAltitudeToECEF({poi_latitude, poi_longitude, 0.0});
                auto ecef_normal = vsg::normalize(ecef);

                vsg::dvec3 centre = ecef;
                vsg::dvec3 eye = centre + ecef_normal * height;
                vsg::dvec3 up = vsg::normalize( vsg::cross(ecef_normal, vsg::cross(vsg::dvec3(0.0, 0.0, 1.0), ecef_normal) ) );

                // set up the camera
                lookAt = vsg::LookAt::create(eye, centre, up);
            }
            else
            {
                lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
            }

            perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
        }
        else
        {
            perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
            lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // add close handler to respond the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        if (pathFilename.empty())
        {
            if (oldTrackball)
            {
                viewer->addEventHandler(vsg::Trackball::create(camera));
            }
            else
            {
                viewer->addEventHandler(vsg::GlobeTrackball::create(camera, ellipsoidModel));
            }
        }
        else
        {
            std::ifstream in(pathFilename);
            if (!in)
            {
                std::cout << "AnimationPat: Could not open animation path file \"" << pathFilename << "\".\n";
                return 1;
            }

            vsg::ref_ptr<vsg::AnimationPath> animationPath(new vsg::AnimationPath);
            animationPath->read(in);

            viewer->addEventHandler(vsg::AnimationPathHandler::create(camera, animationPath, viewer->start_point()));
        }

        // if required pre load specific number of PagedLOD levels.
        if (loadLevels > 0)
        {
            vsg::LoadPagedLOD loadPagedLOD(camera, loadLevels);

#if 0
            if (!path.empty())
            {
                options->paths.insert(options->paths.begin(), path);
            }
#endif

            loadPagedLOD.options = options;

            auto startTime = std::chrono::steady_clock::now();

            vsg_scene->accept(loadPagedLOD);

            auto time = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::steady_clock::now() - startTime).count();
            std::cout << "No. of tiles loaed " << loadPagedLOD.numTiles << " in " << time << "ms." << std::endl;
        }

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        viewer->compile();

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
