#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>

vsg::ref_ptr<vsg::Node> createLargeTestScene(vsg::ref_ptr<vsg::Options> options, vsg::Path textureFile = {}, bool requiresBase = true, bool insertCullNode = true)
{
    auto builder = vsg::Builder::create();
    builder->options = options;

    auto scene = vsg::Group::create();

    vsg::GeometryInfo geomInfo;
    vsg::StateInfo stateInfo;

    geomInfo.cullNode = insertCullNode;

    if (textureFile) stateInfo.image = vsg::read_cast<vsg::Data>(textureFile, options);
    vsg::box bounds(vsg::vec3(-500.0f, -500.0f, 0.0f), vsg::vec3(500.0f, 500.0f, 20.0f));

    uint32_t numBoxes = 400;
    uint32_t numSpheres = 300;
    uint32_t numCapsules = 300;

    vsg::vec3 size = bounds.max - bounds.min;
    auto assignRandomGeometryInfo = [&]()
    {
        vsg::vec3 offset(size.x * float(std::rand()) / float(RAND_MAX),
                         size.y * float(std::rand()) / float(RAND_MAX),
                         size.z * float(std::rand()) / float(RAND_MAX));
        geomInfo.position = bounds.min + offset;
        geomInfo.dx.set(10.0f, 0.0f, 0.0f);
        geomInfo.dy.set(0.0f, 10.0f, 0.0f);
        geomInfo.dz.set(0.0f, 0.0f, 10.0f);

        geomInfo.color.set(float(std::rand()) / float(RAND_MAX),
                           float(std::rand()) / float(RAND_MAX),
                           float(std::rand()) / float(RAND_MAX),
                           1.0f);
    };

    for(uint32_t bi = 0; bi < numBoxes; ++bi)
    {
        assignRandomGeometryInfo();
        auto model = builder->createBox(geomInfo, stateInfo);
        scene->addChild(model);
    }

    for(uint32_t bi = 0; bi < numSpheres; ++bi)
    {
        assignRandomGeometryInfo();
        auto model = builder->createSphere(geomInfo, stateInfo);
        scene->addChild(model);
    }

    for(uint32_t bi = 0; bi < numCapsules; ++bi)
    {
        assignRandomGeometryInfo();
        auto model = builder->createCapsule(geomInfo, stateInfo);
        scene->addChild(model);
    }

    if (requiresBase)
    {
        double diameter = vsg::length(bounds.max - bounds.min);
        geomInfo.position.set((bounds.min.x + bounds.max.x)*0.5, (bounds.min.y + bounds.max.y)*0.5, bounds.min.z);
        geomInfo.dx.set(diameter, 0.0, 0.0);
        geomInfo.dy.set(0.0, diameter, 0.0);
        geomInfo.dz.set(0.0, 0.0, 1.0);
        geomInfo.color.set(1.0f, 1.0f, 1.0f, 1.0f);

        stateInfo.two_sided = true;

        scene->addChild(builder->createQuad(geomInfo, stateInfo));
    }

    return scene;
}

vsg::ref_ptr<vsg::Node> createDroneModel(vsg::ref_ptr<vsg::Options> options, float droneSize)
{
    vsg::GeometryInfo geomInfo;
    vsg::StateInfo stateInfo;
    vsg::Builder builder;
    builder.options = options;

    auto droneGroup = vsg::Group::create();

    geomInfo.dx.set(droneSize * 0.5f, 0.0f, 0.0f);
    geomInfo.dy.set(0.0f, droneSize, 0.0f);
    geomInfo.dz.set(0.0f, 0.0f, droneSize * 0.2f);

    // body
    droneGroup->addChild(builder.createBox(geomInfo, stateInfo));

    geomInfo.dx.set(droneSize*0.5f, 0.0f, 0.0f);
    geomInfo.dy.set(0.0f, droneSize*0.5f, 0.0f);
    geomInfo.dz.set(0.0f, 0.0f, droneSize*0.05f);
    geomInfo.color.set(1.0f, 0.8f, 0.4f, 1.0f);

    // rotors
    geomInfo.position.set(-droneSize*0.5f, -droneSize*0.5f, 0.0f);
    droneGroup->addChild(builder.createDisk(geomInfo, stateInfo));

    geomInfo.position.set(droneSize*0.5f, -droneSize*0.5f, 0.0f);
    droneGroup->addChild(builder.createDisk(geomInfo, stateInfo));

    geomInfo.position.set(droneSize*0.5f, droneSize*0.5f, 0.0f);
    droneGroup->addChild(builder.createDisk(geomInfo, stateInfo));

    geomInfo.position.set(-droneSize*0.5f, droneSize*0.5f, 0.0f);
    droneGroup->addChild(builder.createDisk(geomInfo, stateInfo));

    return droneGroup;
}

vsg::ref_ptr<vsg::Node> createEarthModel(vsg::CommandLine& arguments, vsg::ref_ptr<vsg::Options> options)
{
    if (auto earthFilename = arguments.value<vsg::Path>("", "--earth"))
    {
        return vsg::read_cast<vsg::Node>(earthFilename, options);
    }
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

    if (settings)
    {
        arguments.read("-t", settings->lodTransitionScreenHeightRatio);
        arguments.read("-m", settings->maxLevel);
        auto earth = vsg::TileDatabase::create();
        earth->settings = settings;
        earth->readDatabase(options);

        return earth;
    }

    return {};
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgtextureprojection";


    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});

    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    auto numFrames = arguments.value(-1, "-f");
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--d32")) windowTraits->depthFormat = VK_FORMAT_D32_SFLOAT;
    arguments.read("--samples", windowTraits->samples);
    if (arguments.read({"-t", "--test"}))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->fullscreen = true;
    }
    if (arguments.read("--st"))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->width = 192, windowTraits->height = 108;
        windowTraits->decoration = false;
    }

    double maxShadowDistance = arguments.value<double>(1e8, "--sd");
    double shadowMapBias = arguments.value<double>(0.005, "--sb");
    double lambda = arguments.value<double>(0.5, "--lambda");
    double nearFarRatio = arguments.value<double>(0.001, "--nf");

    bool shaderDebug = arguments.read("--shader-debug");
    bool depthClamp = arguments.read({"--dc", "--depthClamp"});
    if (depthClamp)
    {
        std::cout<<"Enabled depth clamp."<<std::endl;
        auto deviceFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();
        deviceFeatures->get().samplerAnisotropy = VK_TRUE;
        deviceFeatures->get().depthClamp = VK_TRUE;
    }

    vsg::Path textureFile = arguments.value(vsg::Path{}, {"-i", "--image"});

    auto numShadowMapsPerLight = arguments.value<uint32_t>(1, "--sm");
    auto numLights = arguments.value<uint32_t>(1, "-n");

    vsg::ref_ptr<vsg::ResourceHints> resourceHints;
    if (auto resourceHintsFilename = arguments.value<vsg::Path>("", "--rh"))
    {
        resourceHints = vsg::read_cast<vsg::ResourceHints>(resourceHintsFilename, options);
    }

    if (!resourceHints) resourceHints = vsg::ResourceHints::create();

    arguments.read("--shadowMapSize", resourceHints->shadowMapSize);

    if (auto outputResourceHintsFilename = arguments.value<vsg::Path>("", "--orh"))
    {
        if (!resourceHints) resourceHints = vsg::ResourceHints::create();
        vsg::write(resourceHints, outputResourceHintsFilename, options);
        return 0;
    }

    if (arguments.read({"-c", "--custom"}) || depthClamp || shaderDebug)
    {
        // customize the phong ShaderSet
        auto phong_vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/standard.vert", options);
        auto phong_fragShader = vsg::read_cast<vsg::ShaderStage>("shaders/standard_phong.frag", options);
        auto phong = vsg::createPhongShaderSet(options);
        if (phong && phong_vertexShader && phong_fragShader)
        {
            // replace shaders
            phong->stages.clear();
            phong->stages.push_back(phong_vertexShader);
            phong->stages.push_back(phong_fragShader);

            if (shaderDebug)
            {
                phong->optionalDefines.insert("SHADOWMAP_DEBUG");
                phong->defaultShaderHints = vsg::ShaderCompileSettings::create();
                phong->defaultShaderHints->defines.insert("SHADOWMAP_DEBUG");
            }

            if (depthClamp)
            {
                auto rasterizationState = vsg::RasterizationState::create();
                rasterizationState->depthClampEnable = VK_TRUE;
                phong->defaultGraphicsPipelineStates.push_back(rasterizationState);
            }

            // clear prebuilt variants
            phong->variants.clear();

            options->shaderSets["phong"] = phong;

            std::cout<<"Replaced phong shader"<<std::endl;
        }

        // customize the pbr ShaderSet
        auto pbr_vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/standard.vert", options);
        auto pbr_fragShader = vsg::read_cast<vsg::ShaderStage>("shaders/standard_pbr.frag", options);
        auto pbr = vsg::createPhysicsBasedRenderingShaderSet(options);
        if (pbr && pbr_vertexShader && pbr_fragShader)
        {
            // replace shaders
            pbr->stages.clear();
            pbr->stages.push_back(pbr_vertexShader);
            pbr->stages.push_back(pbr_fragShader);

            if (shaderDebug)
            {
                pbr->optionalDefines.insert("SHADOWMAP_DEBUG");
                pbr->defaultShaderHints = vsg::ShaderCompileSettings::create();
                pbr->defaultShaderHints->defines.insert("SHADOWMAP_DEBUG");
            }

            if (depthClamp)
            {
                auto rasterizationState = vsg::RasterizationState::create();
                rasterizationState->depthClampEnable = VK_TRUE;
                pbr->defaultGraphicsPipelineStates.push_back(rasterizationState);
            }

            // clear prebuilt variants
            pbr->variants.clear();

            options->shaderSets["pbr"] = pbr;

            std::cout<<"Replaced pbr shader"<<std::endl;
        }
    }

    auto pathFilename = arguments.value<vsg::Path>("", "-p");
    auto outputFilename = arguments.value<vsg::Path>("", "-o");

    bool requiresBase = true;
    auto earthModel = createEarthModel(arguments, options);

    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel;
    if (earthModel)
    {
        ellipsoidModel = earthModel->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
        requiresBase = false;
    }

    //
    // set up the drone subgraph
    //
    vsg::ref_ptr<vsg::Node> droneModel;
    if (auto droneFilename = arguments.value<vsg::Path>("", "--drone"))
    {
        droneModel = vsg::read_cast<vsg::Node>(droneFilename, options);
    }
    if (!droneModel)
    {
        droneModel = createDroneModel(options, arguments.value(10.0f, "--drone-size"));
    }

    auto droneLocation = vsg::MatrixTransform::create(); // transform into world coordinate frame
    auto droneTransform = vsg::MatrixTransform::create(); // local position of the drone within this droneLocation
    droneLocation->addChild(droneTransform);
    droneTransform->addChild(droneModel);

    //
    // set up the main scene graph
    //
    auto insertCullNode = arguments.read("--cull");
    auto inherit = arguments.read("--inherit");
    auto direction = arguments.value(vsg::dvec3(0.0, 0.0, -1.0), "--direction");
    auto location = arguments.value<vsg::dvec3>({0.0, 0.0, 0.0}, "--location");
    auto scale = arguments.value<double>(1.0, "--scale");
    double viewingDistance = scale;

    vsg::ref_ptr<vsg::StateGroup> stateGroup;
    if (inherit)
    {
        auto shaderSet = vsg::createPhongShaderSet(options);
        auto layout = shaderSet->createPipelineLayout({}, {0, 1});

        stateGroup = vsg::StateGroup::create();

        uint32_t vds_set = 0;
        stateGroup->add(vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, vds_set));

        vsg::info("Added state to inherit ");
        options->inheritedState = stateGroup->stateCommands;
    }

    vsg::ref_ptr<vsg::Node> scene;
    if (argc>1)
    {
        vsg::Path filename = argv[1];
        auto model = vsg::read_cast<vsg::Node>(filename, options);
        if (!model)
        {
            std::cout<<"Faled to load "<<filename<<std::endl;
            return 1;
        }

        scene = model;
    }
    else
    {
        scene = createLargeTestScene(options, textureFile, requiresBase, insertCullNode);
    }

    if (stateGroup)
    {
        // if setup place the StateGroup at the root of the scene graph
        stateGroup->addChild(scene);
        scene = stateGroup;
    }

    // compute the bounds of the scene graph to help position camera
    auto bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
    viewingDistance = vsg::length(bounds.max - bounds.min) * 2.0;

    vsg::ref_ptr<vsg::LookAt> lookAt;

    if (earthModel && ellipsoidModel)
    {
        auto center = (bounds.min + bounds.max) * 0.5;
        center.z = bounds.min.z;

        vsg::dvec3 model_location = { location.x, location.y, 0.0};

        auto transform = vsg::MatrixTransform::create( ellipsoidModel->computeLocalToWorldTransform(model_location) * vsg::translate(-center) );
        transform->addChild(scene);

        droneLocation->matrix = ellipsoidModel->computeLocalToWorldTransform(location);

        // rotate the light direction to have the same location light direction as it would be on a flat model.
        direction = direction * vsg::inverse_3x3(droneLocation->matrix);

        auto group = vsg::Group::create();
        group->addChild(transform);
        group->addChild(earthModel);
        group->addChild(droneLocation);

        scene = group;

        viewingDistance *= scale;

        // set up the camera
        lookAt = vsg::LookAt::create(center + vsg::dvec3(0.0, 0.0, viewingDistance), center, vsg::dvec3(0.0, 1.0, 0.0));

        lookAt->transform(transform->transform({}));

        // update bounds
        bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
    }
    else
    {
        vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;

        auto group = vsg::Group::create();
        group->addChild(scene);
        group->addChild(droneLocation);

        droneLocation->matrix = vsg::translate(location);

        scene = group;

        // set up the camera
        lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -viewingDistance, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    }

    //auto span = vsg::length(bounds.max - bounds.min);
    auto group = vsg::Group::create();
    group->addChild(scene);


    vsg::ref_ptr<vsg::DirectionalLight> directionalLight;
    if (numLights >= 1)
    {
        directionalLight = vsg::DirectionalLight::create();
        directionalLight->name = "directional";
        directionalLight->color.set(1.0, 1.0, 1.0);
        directionalLight->intensity = 0.9;
        directionalLight->direction = direction;
        directionalLight->shadowMaps = numShadowMapsPerLight;
        group->addChild(directionalLight);
    }

    vsg::ref_ptr<vsg::AmbientLight> ambientLight;
    if (numLights >= 2)
    {
        ambientLight = vsg::AmbientLight::create();
        ambientLight->name = "ambient";
        ambientLight->color.set(1.0, 1.0, 1.0);
        ambientLight->intensity = 0.2;
        group->addChild(ambientLight);
    }

    if (numLights >= 3)
    {
        directionalLight->intensity = 0.7;
        ambientLight->intensity = 0.1;

        auto directionalLight2 = vsg::DirectionalLight::create();
        directionalLight2->name = "2nd directional";
        directionalLight2->color.set(1.0, 1.0, 0.0);
        directionalLight2->intensity = 0.7;
        directionalLight2->direction = vsg::normalize(vsg::vec3(0.9, 1.0, -1.0));
        directionalLight2->shadowMaps = numShadowMapsPerLight;
        group->addChild(directionalLight2);
    }

    scene = group;

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


    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    if (ellipsoidModel)
    {
        double horizonMountainHeight = 0.0;
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * viewingDistance, viewingDistance * 1.3);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add the camera and scene graph to View
    auto view = vsg::View::create();
    view->camera = camera;
    view->viewDependentState->maxShadowDistance = maxShadowDistance;
    view->viewDependentState->shadowMapBias = shadowMapBias;
    view->viewDependentState->lambda = lambda;
    view->addChild(scene);

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    auto animationPathHandler = vsg::RecordAnimationPathHandler::create(camera, pathFilename, options);
    animationPathHandler->printFrameStatsToConsole = true;
    viewer->addEventHandler(animationPathHandler);

    auto trackball = vsg::Trackball::create(camera, ellipsoidModel);
    viewer->addEventHandler(trackball);

    auto renderGraph = vsg::RenderGraph::create(window, view);
    auto commandGraph = vsg::CommandGraph::create(window, renderGraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile(resourceHints);

    auto startTime = vsg::clock::now();
    double numFramesCompleted = 0.0;

    double dtheta = 0.01 * 2.0 * vsg::PI;
    double flight_scale = 400.0;

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        double theta = dtheta * std::chrono::duration<double, std::chrono::seconds::period>(viewer->getFrameStamp()->time - startTime).count();

        droneTransform->matrix = vsg::translate(flight_scale * sin(theta), flight_scale * cos(theta), 0.0) * vsg::rotate(-0.5*vsg::PI-theta, vsg::dvec3(0.0, 0.0, 1.0));
        theta += dtheta;

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