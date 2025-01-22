#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include "custom_phong.h"

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
    auto assignRandomGeometryInfo = [&]() {
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

    for (uint32_t bi = 0; bi < numBoxes; ++bi)
    {
        assignRandomGeometryInfo();
        auto model = builder->createBox(geomInfo, stateInfo);
        scene->addChild(model);
    }

    for (uint32_t bi = 0; bi < numSpheres; ++bi)
    {
        assignRandomGeometryInfo();
        auto model = builder->createSphere(geomInfo, stateInfo);
        scene->addChild(model);
    }

    for (uint32_t bi = 0; bi < numCapsules; ++bi)
    {
        assignRandomGeometryInfo();
        auto model = builder->createCapsule(geomInfo, stateInfo);
        scene->addChild(model);
    }

    if (requiresBase)
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

    geomInfo.dx.set(droneSize * 0.5f, 0.0f, 0.0f);
    geomInfo.dy.set(0.0f, droneSize * 0.5f, 0.0f);
    geomInfo.dz.set(0.0f, 0.0f, droneSize * 0.05f);
    geomInfo.color.set(1.0f, 0.8f, 0.4f, 1.0f);

    // rotors
    geomInfo.position.set(-droneSize * 0.5f, -droneSize * 0.5f, 0.0f);
    droneGroup->addChild(builder.createDisk(geomInfo, stateInfo));

    geomInfo.position.set(droneSize * 0.5f, -droneSize * 0.5f, 0.0f);
    droneGroup->addChild(builder.createDisk(geomInfo, stateInfo));

    geomInfo.position.set(droneSize * 0.5f, droneSize * 0.5f, 0.0f);
    droneGroup->addChild(builder.createDisk(geomInfo, stateInfo));

    geomInfo.position.set(-droneSize * 0.5f, droneSize * 0.5f, 0.0f);
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
        settings->lighting = true;
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
        settings->lighting = true;
        settings->projection = "EPSG:3857"; // Spherical Mecator
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

vsg::ref_ptr<vsg::Animation> createAnimationPath(vsg::ref_ptr<vsg::MatrixTransform> transform, double flight_scale, double period)
{
    auto path = vsg::Animation::create();
    path->mode = vsg::Animation::REPEAT;

    auto transformSampler = vsg::TransformSampler::create();
    auto keyframes = transformSampler->keyframes = vsg::TransformKeyframes::create();
    transformSampler->object = transform;
    path->samplers.push_back(transformSampler);

    vsg::dvec3 scale(1.0, 1.0, 1.0);
    double delta = period / 100.0;
    for (double time = 0.0; time < period; time += delta)
    {
        double theta = 2 * vsg::PI * (time / period);
        vsg::dvec3 position(flight_scale * sin(theta), flight_scale * cos(theta), 0.0);
        vsg::dquat rotation(-0.5 * vsg::PI - theta, vsg::dvec3(0.0, 0.0, 1.0));
        keyframes->add(time, position, rotation, scale);
    }
    return path;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
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

    double maxShadowDistance = arguments.value<double>(2000.0, "--sd");
    double shadowMapBias = arguments.value<double>(0.005, "--sb");
    double lambda = arguments.value<double>(0.5, "--lambda");
    double nearFarRatio = arguments.value<double>(0.001, "--nf");

    auto deviceFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();
    deviceFeatures->get().samplerAnisotropy = VK_TRUE;

    /// Use if you want to enable GLSL texture arrays declared as [],
    /// also requires the addition to the GLSL shader:
    ///    #extension GL_EXT_nonuniform_qualifier : enable
    // auto& decriptorIndexingFeatures = deviceFeatures->get<VkPhysicalDeviceDescriptorIndexingFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES>();
    // decriptorIndexingFeatures.runtimeDescriptorArray = true;

    bool shaderDebug = arguments.read("--shader-debug");
    bool depthClamp = arguments.read({"--dc", "--depthClamp"});
    if (depthClamp)
    {
        std::cout << "Enabled depth clamp." << std::endl;
        deviceFeatures->get().depthClamp = VK_TRUE;
    }

    auto insertCullNode = arguments.read("--cull");
    auto direction = arguments.value(vsg::dvec3(0.0, 0.0, -1.0), "--direction");
    auto location = arguments.value<vsg::dvec3>({0.0, 0.0, 100.0}, "--location");
    auto scale = arguments.value<double>(1.0, "--scale");
    double viewingDistance = scale;

    auto pathFilename = arguments.value<vsg::Path>("", "-p");
    auto outputFilename = arguments.value<vsg::Path>("", "-o");
    auto textureFile = arguments.value(vsg::Path{}, {"-i", "--image"});

    auto numShadowMapsPerLight = arguments.value<uint32_t>(1, "--sm");

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

    // replace phong shader only for simplicity of example - vsg::TileDataset and vsg::Builder use phong ShaderSet
    vsg::ref_ptr<vsg::ShaderSet> shaderSet = custom::phong_ShaderSet(options);

    options->shaderSets["phong"] = shaderSet;
    if (!shaderSet)
    {
        std::cout << "No vsg::ShaderSet to process." << std::endl;
        return 1;
    }

    auto shaderHints = shaderSet->defaultShaderHints = vsg::ShaderCompileSettings::create();

    //    if (numShadowMapsPerLight>0)
    //    {
    //        shaderHints->defines.insert("VSG_SHADOWS_HARD");
    //    }

    if (depthClamp || shaderDebug)
    {
        if (shaderDebug)
        {
            shaderHints->defines.insert("SHADOWMAP_DEBUG");
        }

        if (depthClamp)
        {
            auto rasterizationState = vsg::RasterizationState::create();
            rasterizationState->depthClampEnable = VK_TRUE;
            shaderSet->defaultGraphicsPipelineStates.push_back(rasterizationState);
        }

        // clear prebuilt variants
        shaderSet->variants.clear();
    }

    std::vector<vsg::ref_ptr<vsg::Data>> images;
    vsg::ref_ptr<vsg::Data> firstImage;
    uint32_t depth = 0;
    for (int i = 1; i < argc; ++i)
    {
        vsg::Path filename = argv[i];
        if (auto data = vsg::read_cast<vsg::Data>(filename, options))
        {
            data->properties.format = vsg::uNorm_to_sRGB(data->properties.format);
            if (data->properties.format == VK_FORMAT_R8G8B8A8_SRGB)
            {
                if (!firstImage)
                {
                    firstImage = data;
                    depth += data->depth();
                    images.push_back(data);
                }
                else if (firstImage->width() == data->width() && firstImage->height() == data->height())
                {
                    depth += data->depth();
                    images.push_back(data);
                }
                else
                {
                    std::cout << "Image file : " << filename << " loaded, but does not match required dimensions for first image." << std::endl;
                }
            }
            else
            {
                std::cout << "Image file : " << filename << " loaded, but does not match required VK_FORMAT_R8G8B8A8_SRGB format." << std::endl;
            }
        }
        else
        {
            std::cout << "Image file : " << filename << " not loaded." << std::endl;
            return 1;
        }
    }

    if (images.empty())
    {
        std::cout << "No images to project, please specify an image file on the command line." << std::endl;
        return 1;
    }

    // set up the state at the top of the scene graph that will be inherited
    auto stateGroup = vsg::StateGroup::create();

    auto textureCount = vsg::uintValue::create(depth);
    auto countDescriptor = vsg::DescriptorBuffer::create(textureCount);
    auto texgenDescritor = vsg::DescriptorBuffer::create();
    auto textureDescriptor = vsg::DescriptorImage::create();

    {
        auto& countDescriptorBinding = shaderSet->getDescriptorBinding("textureCount");
        auto& texGenMatricesDescriptorBinding = shaderSet->getDescriptorBinding("texGenMatrices");
        auto& projectedTexturesDescriptorBinding = shaderSet->getDescriptorBinding("projectedTextures");
        //
        // place the FogValue DescriptorSet at the root of the scene graph
        // and tell the loader via vsg::Options::inheritedState that it doesn't need to assign
        // it to subgraphs that loader will create
        //
        auto layout = shaderSet->createPipelineLayout({}, {0, 2});

        uint32_t vds_set = 1;
        stateGroup->add(vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, vds_set));

        uint32_t cm_set = 0;

        countDescriptor->dstBinding = countDescriptorBinding.binding;
        texgenDescritor->dstBinding = texGenMatricesDescriptorBinding.binding;
        textureDescriptor->dstBinding = projectedTexturesDescriptorBinding.binding;

        auto cm_dsl = shaderSet->createDescriptorSetLayout({}, cm_set);
        auto cm_ds = vsg::DescriptorSet::create(cm_dsl, vsg::Descriptors{countDescriptor, texgenDescritor, textureDescriptor});

        auto cm_bds = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, cm_ds);
        stateGroup->add(cm_bds);

        vsg::info("Added state to inherit ");
        options->inheritedState = stateGroup->stateCommands;
    }

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

    auto droneLocation = vsg::MatrixTransform::create();  // transform into world coordinate frame
    auto droneTransform = vsg::MatrixTransform::create(); // local position of the drone within this droneLocation
    droneLocation->addChild(droneTransform);
    droneTransform->addChild(droneModel);

    //
    // set up the main scene graph
    //

    vsg::ref_ptr<vsg::Node> scene = createLargeTestScene(options, textureFile, requiresBase, insertCullNode);

    auto sampler = vsg::Sampler::create();
    auto texgenMatrices = vsg::mat4Array::create(depth);
    texgenMatrices->properties.dataVariance = vsg::DYNAMIC_DATA;

    auto texture2DArray = vsg::ubvec4Array3D::create(firstImage->width(), firstImage->height(), depth, vsg::ubvec4(255, 255, 255, 255), vsg::Data::Properties{VK_FORMAT_R8G8B8A8_SRGB});
    texture2DArray->properties.imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

    texgenDescritor->bufferInfoList.push_back(vsg::BufferInfo::create(texgenMatrices));
    textureDescriptor->imageInfoList.push_back(vsg::ImageInfo::create(sampler, texture2DArray));

    double flight_scale = 400.0;
    double animationPeriod = 60.0;

    // create the path for the drone to follow, and to use for setting the texgen matrices to position the textures
    auto dronePath = createAnimationPath(droneTransform, flight_scale, animationPeriod);

    // copy loaded images to single texture2DArray
    uint32_t layer = 0;
    for (auto& data : images)
    {
        std::memcpy(texture2DArray->dataPointer(texture2DArray->index(0, 0, layer)), data->dataPointer(), data->dataSize());
        layer += data->depth();
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

        vsg::dvec3 model_location = {location.x, location.y, 0.0};

        auto transform = vsg::MatrixTransform::create(ellipsoidModel->computeLocalToWorldTransform(model_location) * vsg::translate(-center));
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

    // set up the texgen matrices
    auto imageMatrices = vsg::dmat4Array::create(depth);
    double aspectRatio = (static_cast<double>(texture2DArray->width()) / static_cast<double>(texture2DArray->height()));
    dronePath->start(0.0);
    for (size_t i = 0; i < depth; ++i)
    {
        // drone has Y forward, X to the right, Z up, so without any further rotation the projected texture
        // will poject down the negative Z axis, which in this setup is straight down towards the earth.
        double time = dronePath->maxTime() * (double(i) / double(depth));
        dronePath->update(time);
        auto mv = vsg::inverse(droneLocation->matrix * droneTransform->matrix);
        auto p = vsg::perspective(vsg::radians(45.0), aspectRatio, 1.0, 100.0);
        imageMatrices->set(i, p * mv);
    }
    dronePath->stop(0.0);

    //auto span = vsg::length(bounds.max - bounds.min);
    auto group = vsg::Group::create();
    group->addChild(scene);

    // set up lighting
    {
        auto directionalLight = vsg::DirectionalLight::create();
        directionalLight->name = "directional";
        directionalLight->color.set(1.0, 1.0, 1.0);
        directionalLight->intensity = 0.98f;
        directionalLight->direction = direction;
        if (numShadowMapsPerLight > 0)
        {
            directionalLight->shadowSettings = vsg::HardShadows::create(numShadowMapsPerLight);
        }
        group->addChild(directionalLight);

        auto ambientLight = vsg::AmbientLight::create();
        ambientLight->name = "ambient";
        ambientLight->color.set(1.0, 1.0, 1.0);
        ambientLight->intensity = 0.02f;
        group->addChild(ambientLight);
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

    if (pathFilename)
    {
        auto cameraAnimation = vsg::CameraAnimationHandler::create(camera, pathFilename, options);
        viewer->addEventHandler(cameraAnimation);
        if (cameraAnimation->animation) cameraAnimation->play();
    }

    auto trackball = vsg::Trackball::create(camera, ellipsoidModel);
    viewer->addEventHandler(trackball);

    auto renderGraph = vsg::RenderGraph::create(window, view);
    auto commandGraph = vsg::CommandGraph::create(window, renderGraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile(resourceHints);

    viewer->animationManager->play(dronePath);

    auto startTime = vsg::clock::now();
    double numFramesCompleted = 0.0;

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        // update the eye line text
        vsg::dmat4 inverse_viewMatrix = vsg::inverse(camera->viewMatrix->transform());
        for (size_t i = 0; i < depth; ++i)
        {
            vsg::dmat4 texgenProjView = imageMatrices->at(i);
            vsg::dmat4 texgenTM = texgenProjView * inverse_viewMatrix;
            texgenMatrices->set(i, vsg::mat4(texgenTM));
        }
        texgenMatrices->dirty();

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
