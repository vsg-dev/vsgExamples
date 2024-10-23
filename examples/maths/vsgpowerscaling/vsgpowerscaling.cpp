#include <vsg/all.h>

#include <vsgXchange/all.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

struct SolarSystemSettings
{
    vsg::ref_ptr<vsg::Builder> builder;
    vsg::ref_ptr<vsg::Options> options;
    vsg::ref_ptr<vsg::TileDatabaseSettings> tileDatabaseSettings;
    double earth_to_sun_distance = 1.49e11;
    double sun_radius = 6.9547e8;
};

vsg::ref_ptr<vsg::Node> creteSolarSystem(SolarSystemSettings& settings)
{
    auto solar_system_one = vsg::MatrixTransform::create();

    // create earth one
    auto earth_one = vsg::TileDatabase::create();
    earth_one->settings = settings.tileDatabaseSettings;
    earth_one->readDatabase(settings.options);

    auto earth_transform_one = vsg::MatrixTransform::create();
    earth_transform_one->addChild(earth_one);
    earth_transform_one->matrix = vsg::translate(settings.earth_to_sun_distance, 0.0, 0.0);

    solar_system_one->addChild(earth_transform_one);

    // create sun one

    vsg::GeometryInfo geom;
    vsg::StateInfo state;


    // vsg::Builder uses floats for sizing as it's intended for small local objects,
    // we'll ignore limitations for now as we won't be going close to sun's surface'
    geom.dx.set(2.0f*settings.sun_radius, 0.0f, 0.0f);
    geom.dy.set(0.0f, 2.0f*settings.sun_radius, 0.0f);
    geom.dz.set(0.0f, 0.0f, 2.0f*settings.sun_radius);
    geom.color.set(1.0f, 1.0f, 0.9f, 1.0f);

    state.lighting = false;

    auto sun_one = settings.builder->createSphere(geom, state);

    solar_system_one->addChild(sun_one);

    auto light = vsg::PointLight::create();
    light->intensity = settings.earth_to_sun_distance * settings.earth_to_sun_distance;
    light->position.set(0.0f, 0.0f, 0.0f);
    light->color.set(1.0f, 1.0f, 1.0f);
    solar_system_one->addChild(light);

    return solar_system_one;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    // set up vsg::Options to pass in filepaths, ReaderWriters and other IO related options to use when reading and writing files.
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
    windowTraits->synchronizationLayer = arguments.read("--sync");
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    arguments.read("--samples", windowTraits->samples);
    if (int log_level = 0; arguments.read("--log-level", log_level)) vsg::Logger::instance()->level = vsg::Logger::Level(log_level);
    auto outputFilename = arguments.value<vsg::Path>("", "-o");
    auto numFrames = arguments.value(-1, "-f");
    auto pathFilename = arguments.value<vsg::Path>("", "-p");
    auto maxPagedLOD = arguments.value(0, "--maxPagedLOD");
    if (arguments.read("--rgb")) options->mapRGBtoRGBAHint = false;
    arguments.read("--file-cache", options->fileCache);

    VkClearColorValue clearColor{{0.0f, 0.0f, 0.0f, 1.0f}};

    // set up the solar system paramaters
    SolarSystemSettings settings;

    settings.options = options;
    settings.builder = vsg::Builder::create();
    settings.builder->options = options;

    settings.sun_radius = arguments.value<double>(6.957e7, "--sun-radius");
    settings.earth_to_sun_distance = arguments.value<double>(1.49e8, {"--earth-to-sun-distance", "--sd"}); //1.49e11;

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

        settings.tileDatabaseSettings = vsg::createBingMapsSettings(imagerySet, culture, key, options);
    }

    if (arguments.read("--rm"))
    {
        // setup ready map settings
        settings.tileDatabaseSettings = vsg::TileDatabaseSettings::create();
        settings.tileDatabaseSettings->extents = {{-180.0, -90.0, 0.0}, {180.0, 90.0, 1.0}};
        settings.tileDatabaseSettings->noX = 2;
        settings.tileDatabaseSettings->noY = 1;
        settings.tileDatabaseSettings->maxLevel = 10;
        settings.tileDatabaseSettings->originTopLeft = true;
        settings.tileDatabaseSettings->imageLayer = "http://readymap.org/readymap/tiles/1.0.0/7/{z}/{x}/{y}.jpeg";
        // settings.tileDatabaseSettings->terrainLayer = "http://readymap.org/readymap/tiles/1.0.0/116/{z}/{x}/{y}.tif";
    }

    if (arguments.read("--osm") || !settings.tileDatabaseSettings)
    {
        // setup OpenStreetMap settings
        settings.tileDatabaseSettings = vsg::TileDatabaseSettings::create();
        settings.tileDatabaseSettings->extents = {{-180.0, -90.0, 0.0}, {180.0, 90.0, 1.0}};
        settings.tileDatabaseSettings->noX = 1;
        settings.tileDatabaseSettings->noY = 1;
        settings.tileDatabaseSettings->maxLevel = 17;
        settings.tileDatabaseSettings->originTopLeft = true;
        settings.tileDatabaseSettings->lighting = true;
        settings.tileDatabaseSettings->projection = "EPSG:3857"; // Spherical Mecator
        settings.tileDatabaseSettings->imageLayer = "http://a.tile.openstreetmap.org/{z}/{x}/{y}.png";
    }

    if (!settings.tileDatabaseSettings)
    {
        std::cout << "No TileDatabaseSettings assigned." << std::endl;
        return 1;
    }

    arguments.read("-t", settings.tileDatabaseSettings->lodTransitionScreenHeightRatio);
    arguments.read("-m", settings.tileDatabaseSettings->maxLevel);


    auto universe = vsg::Group::create();

    //
    // create solar system one
    //

    auto solar_system_one = creteSolarSystem(settings);


    universe->addChild(solar_system_one);

    //
    // end of creating solar system one
    //

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    if (outputFilename)
    {
        vsg::write(universe, outputFilename);
        return 0;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create window." << std::endl;
        return 1;
    }

    window->clearColor().set(0.0f, 0.0f, 0.0f, 1.0f);

    viewer->addWindow(window);

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    universe->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.0005;

    vsg::info("universe bounds computeBounds.bounds = ", computeBounds.bounds);

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    auto cameraAnimation = vsg::CameraAnimation::create(camera, pathFilename, options);
    viewer->addEventHandler(cameraAnimation);
    if (cameraAnimation->animation) cameraAnimation->play();

    viewer->addEventHandler(vsg::Trackball::create(camera));

    auto renderGraph = vsg::createRenderGraphForView(window, camera, universe, VK_SUBPASS_CONTENTS_INLINE, false);
    renderGraph->setClearValues(clearColor);

    auto commandGraph = vsg::CommandGraph::create(window, renderGraph);

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
