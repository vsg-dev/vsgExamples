#include <vsg/all.h>

#include <vsgXchange/all.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    // set up vsg::Options to pass in filepaths and ReaderWriter's and other IO related options to use when reading and writing files.
    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());

    arguments.read(options);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgtiledatabase";
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
    auto maxPagedLOD = arguments.value(0, "--maxPagedLOD");
    auto loadLevels = arguments.value(0, "--load-levels");
    auto horizonMountainHeight = arguments.value(0.0, "--hmh");
    bool useEllipsoidPerspective = !arguments.read({"--disble-EllipsoidPerspective", "--dep"});
    if (arguments.read("--rgb")) options->mapRGBtoRGBAHint = false;
    arguments.read("--file-cache", options->fileCache);
    bool osgEarthStyleMouseButtons = arguments.read({"--osgearth", "-e"});

    VkClearColorValue clearColor{{0.2f, 0.2f, 0.4f, 1.0f}};
    arguments.read({"--bc", "--background-color"}, clearColor.float32[0], clearColor.float32[1], clearColor.float32[2], clearColor.float32[3]);

    uint32_t numOperationThreads = 0;
    if (arguments.read("--ot", numOperationThreads)) options->operationThreads = vsg::OperationThreads::create(numOperationThreads);

    vsg::ref_ptr<vsg::TileDatabaseSettings> settings;

    if (arguments.read("--bing-maps"))
    {
        // Bing Maps official documentation:
        //    metadata (includes imagerySet details): https://learn.microsoft.com/en-us/bingmaps/rest-services/imagery/get-imagery-metadata
        //    culture codes: https://learn.microsoft.com/en-us/bingmaps/rest-services/common-parameters-and-types/supported-culture-codes
        //    api key: https://www.microsoft.com/en-us/maps/create-a-bing-maps-key
        auto imagerySet = arguments.value<std::string>("Aerial", "--imagery");
        auto culture = arguments.value<std::string>("en-GB", "--culture");
        auto key = arguments.value<std::string>("", "--key");
        if (key.empty()) key = vsg::getEnv("VSG_BING_KEY");
        if (key.empty()) key = vsg::getEnv("OSGEARTH_BING_KEY");

        vsg::info("imagerySet = ", imagerySet);
        vsg::info("culture = ", culture);
        vsg::info("key = ", key);

        settings = vsg::createBingMapsSettings(imagerySet, culture, key, options);
    }

    if (arguments.read("--rm"))
    {
        // setup ready map settings
        settings = vsg::TileDatabaseSettings::create();
        settings->extents = {{-180.0, -90.0, 0.0}, {180.0, 90.0, 1.0}};
        settings->noX = 2;
        settings->noY = 1;
        settings->maxLevel = 10;
        settings->originTopLeft = false;
        settings->imageLayer = "http://readymap.org/readymap/tiles/1.0.0/7/{z}/{x}/{y}.jpeg";
        // settings->terrainLayer = "http://readymap.org/readymap/tiles/1.0.0/116/{z}/{x}/{y}.tif";
    }

    if (arguments.read("--osm") || !settings)
    {
        // setup OpenStreetMap settings
        settings = vsg::TileDatabaseSettings::create();
        settings->extents = {{-180.0, -90.0, 0.0}, {180.0, 90.0, 1.0}};
        settings->noX = 1;
        settings->noY = 1;
        settings->maxLevel = 17;
        settings->originTopLeft = true;
        settings->lighting = false;
        settings->projection = "EPSG:3857";  // Spherical Mecator
        settings->imageLayer = "http://a.tile.openstreetmap.org/{z}/{x}/{y}.png";
    }


    if (!settings)
    {
        std::cout<<"No TileDatabaseSettings assigned."<<std::endl;
        return 1;
    }

    arguments.read("-t", settings->lodTransitionScreenHeightRatio);
    arguments.read("-m", settings->maxLevel);

    auto ellipsoidModel = settings->ellipsoidModel;

    auto earth = vsg::TileDatabase::create();
    earth->settings = settings;
    earth->readDatabase(options);

    auto vsg_scene = earth;

    const double invalid_value = std::numeric_limits<double>::max();
    double poi_latitude = invalid_value;
    double poi_longitude = invalid_value;
    double poi_distance = invalid_value;
    while (arguments.read("--poi", poi_latitude, poi_longitude)) {};
    while (arguments.read("--distance", poi_distance)) {};

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

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

    if (poi_latitude != invalid_value && poi_longitude != invalid_value)
    {
        double height = (poi_distance != invalid_value) ? poi_distance : radius * 3.5;
        auto ecef = ellipsoidModel->convertLatLongAltitudeToECEF({poi_latitude, poi_longitude, 0.0});
        auto ecef_normal = vsg::normalize(ecef);

        centre = ecef;
        vsg::dvec3 eye = centre + ecef_normal * height;
        vsg::dvec3 up = vsg::normalize(vsg::cross(ecef_normal, vsg::cross(vsg::dvec3(0.0, 0.0, 1.0), ecef_normal)));

        // set up the camera
        lookAt = vsg::LookAt::create(eye, centre, up);
    }
    else
    {
        lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    }

    if (useEllipsoidPerspective)
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
    }


    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    if (pathFilename.empty())
    {
        if (ellipsoidModel)
        {
            auto trackball = vsg::Trackball::create(camera, ellipsoidModel);
            trackball->addKeyViewpoint(vsg::KeySymbol('1'), 51.50151088842245, -0.14181489107549874, 2000.0, 2.0); // Grenwish Observatory
            trackball->addKeyViewpoint(vsg::KeySymbol('2'), 55.948642740309324, -3.199226855522667, 2000.0, 2.0);  // Edinburgh Castle
            trackball->addKeyViewpoint(vsg::KeySymbol('3'), 48.858264952330764, 2.2945039609604665, 2000.0, 2.0);  // Eiffel Town, Paris
            trackball->addKeyViewpoint(vsg::KeySymbol('4'), 52.5162603714634, 13.377684902745642, 2000.0, 2.0);    // Brandenburg Gate, Berlin
            trackball->addKeyViewpoint(vsg::KeySymbol('5'), 30.047448591298807, 31.236319571791213, 10000.0, 2.0); // Cairo
            trackball->addKeyViewpoint(vsg::KeySymbol('6'), 35.653099536061156, 139.74704060056993, 10000.0, 2.0); // Tokyo
            trackball->addKeyViewpoint(vsg::KeySymbol('7'), 37.38701052699002, -122.08555895549424, 10000.0, 2.0); // Mountain View, California
            trackball->addKeyViewpoint(vsg::KeySymbol('8'), 40.689618207006355, -74.04465595488215, 10000.0, 2.0); // Empire State Building
            trackball->addKeyViewpoint(vsg::KeySymbol('9'), 25.997055873649554, -97.15543476551771, 1000.0, 2.0);  // Boca Chica, Taxas

            if (osgEarthStyleMouseButtons)
            {
                trackball->panButtonMask = vsg::BUTTON_MASK_1;
                trackball->rotateButtonMask = vsg::BUTTON_MASK_2;
                trackball->zoomButtonMask = vsg::BUTTON_MASK_3;
            }

            viewer->addEventHandler(trackball);
        }
        else
        {
            viewer->addEventHandler(vsg::Trackball::create(camera));
        }
    }
    else
    {
        auto animationPath = vsg::read_cast<vsg::AnimationPath>(pathFilename, options);
        if (!animationPath)
        {
            std::cout<<"Warning: unable to read animation path : "<<pathFilename<<std::endl;
            return 1;
        }
        viewer->addEventHandler(vsg::AnimationPathHandler::create(camera, animationPath, viewer->start_point()));
    }

    // if required pre load specific number of PagedLOD levels.
    if (loadLevels > 0)
    {
        vsg::LoadPagedLOD loadPagedLOD(camera, loadLevels);

        auto startTime = std::chrono::steady_clock::now();

        vsg_scene->accept(loadPagedLOD);

        auto time = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::steady_clock::now() - startTime).count();
        std::cout << "No. of tiles loaed " << loadPagedLOD.numTiles << " in " << time << "ms." << std::endl;
    }

    auto rendergraph = vsg::createRenderGraphForView(window, camera, vsg_scene);
    rendergraph->setClearValues(clearColor);

    auto commandGraph = vsg::CommandGraph::create(window, rendergraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile();

    if (maxPagedLOD > 0)
    {
        // set targetMaxNumPagedLODWithHighResSubgraphs after Viewer::compile() as it will assign any DatabasePager if required.
        for(auto& task : viewer->recordAndSubmitTasks)
        {
            if (task->databasePager) task->databasePager->targetMaxNumPagedLODWithHighResSubgraphs = maxPagedLOD;
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

    return 0;
}
