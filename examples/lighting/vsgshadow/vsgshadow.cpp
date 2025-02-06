#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#ifdef Tracy_FOUND
#    include <vsg/utils/TracyInstrumentation.h>
#endif

#include <iostream>

struct ModelSettings
{
    vsg::ref_ptr<vsg::Options> options;
    vsg::Path textureFile;
    double lodScreenRatio = 0.02;
    bool insertBaseGeometry = true;
    bool insertCullNode = false;
    bool insertLODNode = false;
    uint32_t targetNumObjects = 1000;
};

vsg::ref_ptr<vsg::Node> decorateIfRequired(vsg::ref_ptr<vsg::Node> node, const ModelSettings& settings)
{
    if (settings.insertLODNode)
    {
        if (auto cullNode = node.cast<vsg::CullNode>())
        {
            // replace cullNode with LOD
            auto lod = vsg::LOD::create();
            lod->bound = cullNode->bound;
            lod->addChild(vsg::LOD::Child{settings.lodScreenRatio, cullNode->child});
            return lod;
        }
        else
        {
            auto bb = vsg::visit<vsg::ComputeBounds>(node).bounds;
            auto lod = vsg::LOD::create();
            lod->bound = vsg::dsphere((bb.min + bb.max) * 0.5, vsg::length(bb.max - bb.min));
            lod->addChild(vsg::LOD::Child{settings.lodScreenRatio, node});
            return lod;
        }
    }

    return node;
};

vsg::ref_ptr<vsg::Node> createTestScene(const ModelSettings& settings)
{
    auto builder = vsg::Builder::create();
    builder->options = settings.options;

    auto scene = vsg::Group::create();

    vsg::GeometryInfo geomInfo;
    geomInfo.cullNode = settings.insertCullNode;

    vsg::StateInfo stateInfo;
    if (settings.textureFile) stateInfo.image = vsg::read_cast<vsg::Data>(settings.textureFile, settings.options);

    geomInfo.color.set(1.0f, 1.0f, 0.5f, 1.0f);
    scene->addChild(decorateIfRequired(builder->createBox(geomInfo, stateInfo), settings));

    geomInfo.color.set(1.0f, 0.5f, 1.0f, 1.0f);
    geomInfo.position += geomInfo.dx * 1.5f;
    scene->addChild(decorateIfRequired(builder->createSphere(geomInfo, stateInfo), settings));

    geomInfo.color.set(0.0f, 1.0f, 1.0f, 1.0f);
    geomInfo.position += geomInfo.dx * 1.5f;
    scene->addChild(decorateIfRequired(builder->createCylinder(geomInfo, stateInfo), settings));

    geomInfo.color.set(0.5f, 1.0f, 0.5f, 1.0f);
    geomInfo.position += geomInfo.dx * 1.5f;
    scene->addChild(decorateIfRequired(builder->createCapsule(geomInfo, stateInfo), settings));

    auto bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
    if (settings.insertBaseGeometry)
    {
        float diameter = static_cast<float>(vsg::length(bounds.max - bounds.min));
        geomInfo.position.set(static_cast<float>((bounds.min.x + bounds.max.x) * 0.5), static_cast<float>((bounds.min.y + bounds.max.y) * 0.5), static_cast<float>(bounds.min.z));
        geomInfo.dx.set(diameter, 0.0f, 0.0f);
        geomInfo.dy.set(0.0f, diameter, 0.0f);
        geomInfo.color.set(1.0f, 1.0f, 1.0f, 1.0f);

        stateInfo.two_sided = true;

        scene->addChild(builder->createQuad(geomInfo, stateInfo));
    }
    vsg::info("createTestScene() extents = ", bounds);

    return scene;
}

vsg::ref_ptr<vsg::Node> createLargeTestScene(const ModelSettings& settings)
{
    auto builder = vsg::Builder::create();
    builder->options = settings.options;

    auto scene = vsg::Group::create();

    vsg::GeometryInfo geomInfo;
    geomInfo.cullNode = settings.insertCullNode;

    vsg::StateInfo stateInfo;
    if (settings.textureFile) stateInfo.image = vsg::read_cast<vsg::Data>(settings.textureFile, settings.options);

    vsg::box bounds(vsg::vec3(0.0f, 0.0f, 0.0f), vsg::vec3(1000.0f, 1000.0f, 20.0f));

    uint32_t numBoxes = (400 * settings.targetNumObjects) / 1000;
    uint32_t numSpheres = (300 * settings.targetNumObjects) / 1000;
    uint32_t numCapsules = (300 * settings.targetNumObjects) / 1000;

    vsg::vec3 size = bounds.max - bounds.min;
    float length = 0.5f * std::sqrt((size.x * size.y) / static_cast<float>(settings.targetNumObjects));

    auto assignRandomGeometryInfo = [&]() {
        vsg::vec3 offset(size.x * float(std::rand()) / float(RAND_MAX),
                         size.y * float(std::rand()) / float(RAND_MAX),
                         size.z * float(std::rand()) / float(RAND_MAX));
        geomInfo.position = bounds.min + offset;
        geomInfo.dx.set(length, 0.0f, 0.0f);
        geomInfo.dy.set(0.0f, length, 0.0f);
        geomInfo.dz.set(0.0f, 0.0f, length);

        geomInfo.color.set(float(std::rand()) / float(RAND_MAX),
                           float(std::rand()) / float(RAND_MAX),
                           float(std::rand()) / float(RAND_MAX),
                           1.0f);
    };

    for (uint32_t bi = 0; bi < numBoxes; ++bi)
    {
        assignRandomGeometryInfo();
        auto model = decorateIfRequired(builder->createBox(geomInfo, stateInfo), settings);
        // vsg::info("BOX geomInfo.position = ", geomInfo.position, ", ", model);
        scene->addChild(model);
    }

    for (uint32_t bi = 0; bi < numSpheres; ++bi)
    {
        assignRandomGeometryInfo();
        auto model = decorateIfRequired(builder->createSphere(geomInfo, stateInfo), settings);
        // vsg::info("Sphere geomInfo.position = ", geomInfo.position, ", ", model);
        scene->addChild(model);
    }

    for (uint32_t bi = 0; bi < numCapsules; ++bi)
    {
        assignRandomGeometryInfo();
        auto model = decorateIfRequired(builder->createCapsule(geomInfo, stateInfo), settings);
        // vsg::info("Capsule geomInfo.position = ", geomInfo.position, ", ", model);
        scene->addChild(model);
    }

    if (settings.insertBaseGeometry)
    {
        float diameter = static_cast<float>(vsg::length(bounds.max - bounds.min));
        geomInfo.position.set(static_cast<float>((bounds.min.x + bounds.max.x) * 0.5), static_cast<float>((bounds.min.y + bounds.max.y) * 0.5), static_cast<float>(bounds.min.z));
        geomInfo.dx.set(diameter, 0.0f, 0.0f);
        geomInfo.dy.set(0.0f, diameter, 0.0f);
        geomInfo.dz.set(0.0f, 0.0f, 1.0f);
        geomInfo.color.set(1.0f, 1.0f, 1.0f, 1.0f);

        stateInfo.two_sided = true;

        scene->addChild(builder->createQuad(geomInfo, stateInfo));
    }

    vsg::info("createTestScene() extents = ", bounds);

    return scene;
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
    windowTraits->windowTitle = "vsgshadows";

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    windowTraits->synchronizationLayer = arguments.read("--sync");

    bool reportAverageFrameRate = arguments.read("--fps");
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
        reportAverageFrameRate = true;
    }
    if (arguments.read("--st"))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->width = 192, windowTraits->height = 108;
        windowTraits->decoration = false;
        reportAverageFrameRate = true;
    }

    const double invalid_time = std::numeric_limits<double>::max();
    auto duration = arguments.value(invalid_time, "--duration");

    vsg::ref_ptr<vsg::Instrumentation> instrumentation;
    if (arguments.read({"--gpu-annotation", "--ga"}) && vsg::isExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        windowTraits->debugUtils = true;

        auto gpu_instrumentation = vsg::GpuAnnotation::create();
        if (arguments.read("--func")) gpu_instrumentation->labelType = vsg::GpuAnnotation::SourceLocation_function;

        instrumentation = gpu_instrumentation;
    }
    else if (arguments.read({"--profiler", "--pr"}))
    {
        // set Profiler options
        auto settings = vsg::Profiler::Settings::create();
        arguments.read("--cpu", settings->cpu_instrumentation_level);
        arguments.read("--gpu", settings->gpu_instrumentation_level);
        arguments.read("--log-size", settings->log_size);
        arguments.read("--gpu-size", settings->gpu_timestamp_size);

        // create the profiler
        instrumentation = vsg::Profiler::create(settings);
    }
#ifdef Tracy_FOUND
    else if (arguments.read("--tracy"))
    {
        windowTraits->deviceExtensionNames.push_back(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);

        auto tracy_instrumentation = vsg::TracyInstrumentation::create();
        arguments.read("--cpu", tracy_instrumentation->settings->cpu_instrumentation_level);
        arguments.read("--gpu", tracy_instrumentation->settings->gpu_instrumentation_level);
        instrumentation = tracy_instrumentation;
    }
#endif

    double maxShadowDistance = arguments.value<double>(1e8, "--sd");
    double shadowMapBias = arguments.value<double>(0.005, "--sb");
    double lambda = arguments.value<double>(0.5, "--lambda");
    double nearFarRatio = arguments.value<double>(0.001, "--nf");

    bool depthClamp = arguments.read({"--dc", "--depthClamp"});
    if (depthClamp)
    {
        std::cout << "Enabled depth clamp." << std::endl;
        auto deviceFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();
        deviceFeatures->get().samplerAnisotropy = VK_TRUE;
        deviceFeatures->get().depthClamp = VK_TRUE;
    }

    ModelSettings settings;
    settings.options = options;
    settings.textureFile = arguments.value(vsg::Path{}, {"-i", "--image"});
    settings.insertCullNode = arguments.read("--cull");
    settings.insertLODNode = arguments.read("--lod");
    arguments.read("--target", settings.targetNumObjects);

    auto numShadowMapsPerLight = arguments.value<uint32_t>(1, "--sm");
    auto numLights = arguments.value<uint32_t>(1, "-n");

    vsg::ref_ptr<vsg::ResourceHints> resourceHints;
    if (auto resourceHintsFilename = arguments.value<vsg::Path>("", "--rh"))
    {
        resourceHints = vsg::read_cast<vsg::ResourceHints>(resourceHintsFilename, options);
    }

    bool assignRegionOfInterest = arguments.read({"--region-of-interest", "--roi"});

    if (!resourceHints) resourceHints = vsg::ResourceHints::create();

    arguments.read("--shadowMapSize", resourceHints->shadowMapSize);

    if (auto outputResourceHintsFilename = arguments.value<vsg::Path>("", "--orh"))
    {
        if (!resourceHints) resourceHints = vsg::ResourceHints::create();
        vsg::write(resourceHints, outputResourceHintsFilename, options);
        return 0;
    }

    auto shaderHints = vsg::ShaderCompileSettings::create();
    vsg::ref_ptr<vsg::ShadowSettings> shadowSettings;

    int shadowSampleCount = arguments.value(16, "--shadow-samples");

    if (arguments.read("--pcss"))
    {
        shaderHints->defines.insert("VSG_SHADOWS_PCSS");
        shadowSettings = vsg::PercentageCloserSoftShadows::create(numShadowMapsPerLight);
    }

    float penumbraRadius = 0.05f;
    if (arguments.read("--pcf", penumbraRadius) || arguments.read("--soft", penumbraRadius))
    {
        shaderHints->defines.insert("VSG_SHADOWS_SOFT");
        shadowSettings = vsg::SoftShadows::create(numShadowMapsPerLight, penumbraRadius);
    }

    if (arguments.read("--hard") || !shadowSettings)
    {
        shaderHints->defines.insert("VSG_SHADOWS_HARD");
        shadowSettings = vsg::HardShadows::create(numShadowMapsPerLight);
    }

    if (arguments.read({"--alpha-test", "--at"}))
    {
        shaderHints->defines.insert("VSG_ALPHA_TEST");
    }

    bool overrideShadowSettings = arguments.read("--override");
    if (overrideShadowSettings)
    {
        // overrideShadowSettings test will use the viewDependentState::shadowSettingsOverride with HardShadows later
        shaderHints->defines.insert("VSG_SHADOWS_HARD");
    }

    if (arguments.read("--shader-debug"))
    {
        shaderHints->defines.insert("SHADOWMAP_DEBUG");
    }

    if (arguments.read({"-c", "--custom"}) || depthClamp || !shaderHints->defines.empty() || shadowSampleCount != 16)
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

            phong->defaultShaderHints = shaderHints;

            phong_fragShader->specializationConstants = vsg::ShaderStage::SpecializationConstants{
                {0, vsg::intValue::create(shadowSampleCount)},
            };

            if (depthClamp)
            {
                auto rasterizationState = vsg::RasterizationState::create();
                rasterizationState->depthClampEnable = VK_TRUE;
                phong->defaultGraphicsPipelineStates.push_back(rasterizationState);
            }

            // clear prebuilt variants
            phong->variants.clear();

            options->shaderSets["phong"] = phong;

            std::cout << "Replaced phong shader" << std::endl;
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

            pbr->defaultShaderHints = shaderHints;

            pbr_fragShader->specializationConstants = vsg::ShaderStage::SpecializationConstants{
                {0, vsg::intValue::create(shadowSampleCount)},
            };

            if (depthClamp)
            {
                auto rasterizationState = vsg::RasterizationState::create();
                rasterizationState->depthClampEnable = VK_TRUE;
                pbr->defaultGraphicsPipelineStates.push_back(rasterizationState);
            }

            // clear prebuilt variants
            pbr->variants.clear();

            options->shaderSets["pbr"] = pbr;

            std::cout << "Replaced pbr shader" << std::endl;
        }
    }

    auto pathFilename = arguments.value<vsg::Path>("", "-p");
    auto outputFilename = arguments.value<vsg::Path>("", "-o");

    vsg::ref_ptr<vsg::Node> earthModel;
    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel;
    if (auto earthFilename = arguments.value<vsg::Path>("", "--earth"))
    {
        earthModel = vsg::read_cast<vsg::Node>(earthFilename, options);
        if (earthModel)
        {
            ellipsoidModel = earthModel->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
            settings.insertBaseGeometry = false;
        }
    }

    auto inherit = arguments.read("--inherit");
    auto direction = arguments.value(vsg::dvec3(0.0, 0.0, -1.0), "--direction");
    auto location = arguments.value<vsg::dvec3>({0.0, 0.0, 0.0}, "--location");
    auto angleSubtended = arguments.value<float>(0.0090f, "--angleSubtended");
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
    bool largeScene = arguments.read("--large");
    if (largeScene)
    {
        scene = createLargeTestScene(settings);
    }
    else if (argc > 1)
    {
        vsg::Path filename = argv[1];
        auto model = vsg::read_cast<vsg::Node>(filename, options);
        if (!model)
        {
            std::cout << "Failed to load " << filename << std::endl;
            return 1;
        }

        scene = model;
    }
    else
    {
        scene = createTestScene(settings);
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

    if (assignRegionOfInterest)
    {
        auto group = vsg::Group::create();

        auto regionOfInterest = vsg::RegionOfInterest::create();
        regionOfInterest->name = "region to cast shadow";
        regionOfInterest->points.emplace_back(bounds.min.x, bounds.min.y, bounds.min.z);
        regionOfInterest->points.emplace_back(bounds.max.x, bounds.min.y, bounds.min.z);
        regionOfInterest->points.emplace_back(bounds.max.x, bounds.max.y, bounds.min.z);
        regionOfInterest->points.emplace_back(bounds.min.x, bounds.max.y, bounds.min.z);
        regionOfInterest->points.emplace_back(bounds.min.x, bounds.min.y, bounds.max.z);
        regionOfInterest->points.emplace_back(bounds.max.x, bounds.min.y, bounds.max.z);
        regionOfInterest->points.emplace_back(bounds.max.x, bounds.max.y, bounds.max.z);
        regionOfInterest->points.emplace_back(bounds.min.x, bounds.max.y, bounds.max.z);
        group->addChild(regionOfInterest);
        group->addChild(scene);

        scene = group;
    }

    vsg::ref_ptr<vsg::LookAt> lookAt;

    if (earthModel && ellipsoidModel)
    {
        auto center = (bounds.min + bounds.max) * 0.5;
        center.z = bounds.min.z;

        auto transform = vsg::MatrixTransform::create(ellipsoidModel->computeLocalToWorldTransform(location) * vsg::scale(scale, scale, scale) * vsg::translate(-center));
        transform->addChild(scene);

        auto group = vsg::Group::create();
        group->addChild(transform);
        group->addChild(earthModel);

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

        // set up the camera
        lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -viewingDistance, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    }

    //auto span = vsg::length(bounds.max - bounds.min);
    auto group = vsg::Group::create();
    group->addChild(scene);

    vsg::ref_ptr<vsg::DirectionalLight> directionalLight;
    if (numLights >= 1 && numLights < 4)
    {
        directionalLight = vsg::DirectionalLight::create();
        directionalLight->name = "directional";
        directionalLight->color.set(1.0f, 1.0f, 1.0f);
        directionalLight->intensity = 0.95f;
        directionalLight->direction = direction;
        directionalLight->angleSubtended = angleSubtended;
        directionalLight->shadowSettings = shadowSettings;

        group->addChild(directionalLight);
    }

    vsg::ref_ptr<vsg::AmbientLight> ambientLight;
    if (numLights >= 2)
    {
        ambientLight = vsg::AmbientLight::create();
        ambientLight->name = "ambient";
        ambientLight->color.set(1.0f, 1.0f, 1.0f);
        ambientLight->intensity = 0.05f;
        group->addChild(ambientLight);
    }

    if (numLights >= 3 && numLights < 4)
    {
        directionalLight->intensity = 0.7f;
        ambientLight->intensity = 0.1f;

        auto directionalLight2 = vsg::DirectionalLight::create();
        directionalLight2->name = "2nd directional";
        directionalLight2->color.set(1.0f, 1.0f, 0.0f);
        directionalLight2->intensity = 0.95f;
        directionalLight2->direction = vsg::normalize(vsg::dvec3(0.9, 1.0, -1.0));
        directionalLight2->angleSubtended = angleSubtended;
        directionalLight2->shadowSettings = shadowSettings;

        group->addChild(directionalLight2);
    }

    if (numLights >= 4)
    {
        auto spotLight = vsg::SpotLight::create();
        spotLight->name = "spot";
        spotLight->color.set(0.0f, 1.0f, 1.0f);
        spotLight->intensity = 200.0f;
        spotLight->position = vsg::vec3(3.0, 0.5, 15.0);
        spotLight->direction = vsg::normalize(vsg::vec3(-0.5, -1, -10));
        if (largeScene)
        {
            spotLight->position = vsg::vec3(3000.0, 500.0, 15000.0);
            spotLight->direction = vsg::normalize(vsg::dvec3(500, 500, 0) - spotLight->position);
            spotLight->intensity = static_cast<float>(15000.0 * 15000.0 / 4.0);
        }
        spotLight->outerAngle = vsg::radians(10.0);
        spotLight->innerAngle = vsg::radians(5.0);
        spotLight->radius = 0.2;
        spotLight->shadowSettings = shadowSettings;

        group->addChild(spotLight);
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
    if (overrideShadowSettings) view->viewDependentState->shadowSettingsOverride[{}] = vsg::HardShadows::create(1);

    view->addChild(scene);

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    auto cameraAnimation = vsg::CameraAnimationHandler::create(camera, pathFilename, options);
    viewer->addEventHandler(cameraAnimation);
    if (cameraAnimation->animation)
    {
        cameraAnimation->play();
        if (reportAverageFrameRate && duration == invalid_time) duration = cameraAnimation->animation->maxTime();
    }

    viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

    auto renderGraph = vsg::RenderGraph::create(window, view);
    auto commandGraph = vsg::CommandGraph::create(window, renderGraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    if (instrumentation) viewer->assignInstrumentation(instrumentation);

    viewer->compile(resourceHints);

    auto startTime = vsg::clock::now();
    double numFramesCompleted = 0.0;

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0) && (viewer->getFrameStamp()->simulationTime < duration))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();
        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();

        numFramesCompleted += 1.0;
    }

    if (reportAverageFrameRate)
    {
        auto elapesedTime = std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - startTime).count();
        if (numFramesCompleted > 0.0)
        {
            std::cout << "Average frame rate = " << (numFramesCompleted / elapesedTime) << std::endl;
        }
    }

    if (auto profiler = instrumentation.cast<vsg::Profiler>())
    {
        instrumentation->finish();
        profiler->log->report(std::cout);
    }

    return 0;
}
