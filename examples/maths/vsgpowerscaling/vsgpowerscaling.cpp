#include <vsg/all.h>

#include <vsgXchange/all.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>


struct SolarSystemSettings
{
    std::string name;
    vsg::ref_ptr<vsg::Builder> builder;
    vsg::ref_ptr<vsg::Options> options;
    vsg::ref_ptr<vsg::TileDatabaseSettings> tileDatabaseSettings;
    double earth_to_sun_distance = 1.49e11;
    double sun_radius = 6.9547e8;
    vsg::vec4 sun_color = {1.0f, 1.0f, 0.5f, 1.0f};

    vsg::ref_ptr<vsg::ShaderSet> flatShaderSet;
    vsg::ref_ptr<vsg::ShaderSet> phongShaderSet;
    vsg::ref_ptr<vsg::ShaderSet> pbrShaderSet;
};

vsg::ref_ptr<vsg::Node> createStarfield(double maxRadius, size_t numStars, vsg::ref_ptr<vsg::ShaderSet> shaderSet)
{
    // set up the star positions
    auto vertices = vsg::vec3Array::create(numStars);
    auto colors = vsg::vec4Array::create(numStars);
    for(auto& v : *vertices)
    {
        // lazy when to ensure vertices lie within sphere - just keep recomputing the position if the position is outside the maxRadius
        do
        {
            v.x = 2.0 * maxRadius * (static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX) - 0.5);
            v.y = 2.0 * maxRadius * (static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX) - 0.5);
            v.z = 2.0 * maxRadius * (static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX) - 0.5);
        } while (vsg::length(v) > maxRadius);
    }

    for(auto& c : *colors)
    {
        float brightness = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        c.set(brightness, brightness, brightness, 1.0);
    }

    auto normals = vsg::vec3Value::create(vsg::vec3(0.0f, 0.0f, 1.0f));
    auto texcoords = vsg::vec2Value::create(vsg::vec2(1.0f, 1.0f));


    // configure the state and array layouts to assign to the scene graph
    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(shaderSet);


    vsg::DataList vertexArrays;
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normals", VK_VERTEX_INPUT_RATE_INSTANCE, normals);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_INSTANCE, texcoords);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_VERTEX, colors);

    // set up passing of material
    auto mat = vsg::PhongMaterialValue::create();
    graphicsPipelineConfig->assignDescriptor("material", mat);

    struct SetPipelineStates : public vsg::Visitor
    {

        void apply(vsg::Object& object) override
        {
            object.traverse(*this);
        }

        void apply(vsg::InputAssemblyState& ias) override
        {
            ias.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        }
    } sps;

    graphicsPipelineConfig->accept(sps);

    auto vertexDraw = vsg::VertexDraw::create();
    vertexDraw->assignArrays(vertexArrays);
    vertexDraw->vertexCount = numStars;
    vertexDraw->instanceCount = 1;
    vertexDraw->firstBinding = graphicsPipelineConfig->baseAttributeBinding;

    graphicsPipelineConfig->init();

    // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
    auto stateGroup = vsg::StateGroup::create();

    graphicsPipelineConfig->copyTo(stateGroup);

    stateGroup->addChild(vertexDraw);

    return stateGroup;
}

vsg::ref_ptr<vsg::MatrixTransform> creteSolarSystem(SolarSystemSettings& settings)
{
    auto solar_system = vsg::MatrixTransform::create();

    auto sun_view = vsg::MatrixTransform::create();
    sun_view->setValue("viewpoint", settings.name+"sun_view");
    sun_view->matrix =  vsg::rotate(vsg::radians(70.0), 1.0, 0.0, 0.0) * vsg::translate(0.0, 0.0, settings.earth_to_sun_distance*3.0);

    solar_system->addChild(sun_view);

    double day = 24.0 * 60.0 * 60.0;
    double year = 365.25 * day;
    double earth_radius = settings.tileDatabaseSettings->ellipsoidModel->radiusEquator();

    // create earth one
    auto earth = vsg::TileDatabase::create();
    earth->settings = settings.tileDatabaseSettings;
    earth->readDatabase(settings.options);

    auto earth_view = vsg::MatrixTransform::create();
    earth_view->setValue("viewpoint", settings.name+"earth_view");
    earth_view->matrix = vsg::rotate(vsg::radians(90.0), 1.0, 0.0, 0.0) * vsg::translate(0.0, 0.0, earth_radius * 5.0);

    auto earth_rotation_about_axis = vsg::MatrixTransform::create();
    earth_rotation_about_axis->addChild(earth);
    earth_rotation_about_axis->addChild(earth_view);


    auto orbit_view = vsg::MatrixTransform::create();
    orbit_view->setValue("viewpoint", settings.name+"orbit_view");
    orbit_view->matrix = vsg::rotate(vsg::radians(45.0), 0.0, 0.0, 1.0) * vsg::rotate(vsg::radians(90.0), 1.0, 0.0, 0.0) * vsg::translate(0.0, 0.0, earth_radius * 5.0);

    auto earth_position_from_sun = vsg::MatrixTransform::create();
    earth_position_from_sun->addChild(earth_rotation_about_axis);
    earth_position_from_sun->addChild(orbit_view);
    earth_position_from_sun->matrix = vsg::translate(settings.earth_to_sun_distance, 0.0, 0.0);


    auto earth_orbit_transform = vsg::MatrixTransform::create();
    earth_orbit_transform->addChild(earth_position_from_sun);
    solar_system->addChild(earth_orbit_transform);

    // animate the earths rotation around it's axis
    auto earth_keyframes = vsg::TransformKeyframes::create();
    earth_keyframes->add(0.0, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(0.0), vsg::dvec3(0.0, 0.0, 1.0)));
    earth_keyframes->add(day*0.5, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(180.0), vsg::dvec3(0.0, 0.0, 1.0)));
    earth_keyframes->add(day, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(360.0), vsg::dvec3(0.0, 0.0, 1.0)));

    auto earth_rotation_about_axisSampler = vsg::TransformSampler::create();
    earth_rotation_about_axisSampler->keyframes = earth_keyframes;
    earth_rotation_about_axisSampler->object = earth_rotation_about_axis;

    auto earth_animation = vsg::Animation::create();
    earth_animation->samplers.push_back(earth_rotation_about_axisSampler);



    // animate the earths rotation around the sun
    auto orbit_keyframes = vsg::TransformKeyframes::create();
    orbit_keyframes->add(0.0, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(0.0), vsg::dvec3(0.0, 0.0, 1.0)));
    orbit_keyframes->add(year*0.5, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(180.0), vsg::dvec3(0.0, 0.0, 1.0)));
    orbit_keyframes->add(year, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(360.0), vsg::dvec3(0.0, 0.0, 1.0)));

    auto orbit_transformSampler = vsg::TransformSampler::create();
    orbit_transformSampler->keyframes = orbit_keyframes;
    orbit_transformSampler->object = earth_orbit_transform;

    auto oribit_animation = vsg::Animation::create();
    oribit_animation->samplers.push_back(orbit_transformSampler);

    auto animationGroup = vsg::AnimationGroup::create();
    animationGroup->animations.push_back(earth_animation);
    animationGroup->animations.push_back(oribit_animation);

    solar_system->addChild(animationGroup);

    // create sun

    settings.builder->shaderSet = settings.flatShaderSet;

    // vsg::Builder uses floats for sizing as it's intended for small local objects,
    // we'll ignore limitations for now as we won't be going close to sun's surface'
    vsg::GeometryInfo geom;
    geom.dx.set(2.0f*settings.sun_radius, 0.0f, 0.0f);
    geom.dy.set(0.0f, 2.0f*settings.sun_radius, 0.0f);
    geom.dz.set(0.0f, 0.0f, 2.0f*settings.sun_radius);
    geom.color = settings.sun_color;

    vsg::StateInfo state;
    state.lighting = false;

    auto sun = settings.builder->createSphere(geom, state);

    solar_system->addChild(sun);

    auto light = vsg::PointLight::create();
    light->intensity = settings.earth_to_sun_distance * settings.earth_to_sun_distance;
    light->position.set(0.0f, 0.0f, 0.0f);
    light->color = settings.sun_color.rgb;
    solar_system->addChild(light);

    return solar_system;
}

class FindViewpoints : public vsg::Visitor
{
public:

    std::multimap<std::string, vsg::RefObjectPath> viewpoints;
    vsg::ObjectPath objectPath;

    void apply(vsg::Object& object)
    {
        objectPath.push_back(&object);

        std::string name;
        if (object.getValue("viewpoint", name))
        {
            viewpoints.emplace(name, vsg::RefObjectPath(objectPath.begin(), objectPath.end()));
        }

        object.traverse(*this);

        objectPath.pop_back();

    }
};


class StellarView : public vsg::Inherit<vsg::ViewMatrix, StellarView>
{
public:

    StellarView() :
        position(0.0, 0.0, 0.0),
        rotation{},
        scale(1.0, 1.0, 1.0)
    {
    }

    StellarView(const StellarView& view, const vsg::CopyOp& copyop = {}) :
        Inherit(view, copyop),
        position(view.position),
        rotation(view.rotation),
        scale(view.scale)
    {
    }

    vsg::ref_ptr<vsg::Object> clone(const vsg::CopyOp& copyop = {}) const override { return StellarView::create(*this, copyop); }

    vsg::dvec3 position;
    vsg::dquat rotation;
    vsg::dvec3 scale;

    void set(const vsg::dmat4& matrix)
    {
        vsg::decompose(matrix, position, rotation, scale);
    }

    vsg::dmat4 transform() const override
    {
        return vsg::scale(1.0/scale.x, 1.0/scale.y, 1.0/scale.z) * vsg::rotate(-rotation) * vsg::translate(-position);
    }
};

class StellarManipulator : public vsg::Inherit<vsg::Visitor, StellarManipulator>
{
public:

    vsg::ref_ptr<StellarView> stellarView;
    std::multimap<std::string, vsg::RefObjectPath> viewpoints;
    vsg::RefObjectPath currentFocus;

    double animationDuration = 5.0;
    vsg::time_point startTime;
    vsg::RefObjectPath startViewpoint;
    vsg::RefObjectPath targetViewpoint;

    StellarManipulator(vsg::ref_ptr<StellarView> in_stellarView) :
        stellarView(in_stellarView)
    {
    }

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (keyPress.keyBase >= vsg::KEY_1 && keyPress.keyBase <= vsg::KEY_9)
        {
            if (viewpoints.empty()) return;

            uint16_t key = vsg::KEY_1;
            auto itr = viewpoints.begin();
            while(key < keyPress.keyBase  && itr != viewpoints.end())
            {
                ++itr;
                ++key;
            }

            if (itr == viewpoints.end()) return;

            if (animationDuration<=0.0 || currentFocus.empty())
            {
                currentFocus = itr->second;
            }
            else
            {
                startTime = keyPress.time;
                startViewpoint = currentFocus;
                targetViewpoint = itr->second;
            }

            //auto matrix = vsg::computeTransform(itr->second);
            //stellarView->set(matrix);
        }
    }


    void apply(vsg::FrameEvent& frame) override
    {
        if (!targetViewpoint.empty() && !startViewpoint.empty())
        {
            double timeSinceAnimationStart = std::chrono::duration<double, std::chrono::seconds::period>(frame.time - startTime).count();

            if (timeSinceAnimationStart >= animationDuration)
            {
                currentFocus = targetViewpoint;
                stellarView->set(vsg::computeTransform(currentFocus));

                startViewpoint.clear();
                targetViewpoint.clear();

                return;
            }

            auto startMatrix = vsg::computeTransform(startViewpoint);
            auto targetMatrix = vsg::computeTransform(targetViewpoint);

            vsg::dvec3 startTranslation, startScale, targetTranslation, targetScale;
            vsg::dquat startRotation, targetRotation;

            vsg::decompose(startMatrix, startTranslation, startRotation, startScale);
            vsg::decompose(targetMatrix, targetTranslation, targetRotation, targetScale);

            double tr = (timeSinceAnimationStart /animationDuration);
            double r = 1.0 - (1.0+cos(tr * vsg::PI))*0.5;

            stellarView->position = vsg::mix(startTranslation, targetTranslation, r);
            stellarView->rotation = vsg::mix(startRotation, targetRotation, r);
            stellarView->scale = vsg::mix(startScale, targetScale, r);

            return;
        }

        if (!currentFocus.empty())
        {
            stellarView->set(vsg::computeTransform(currentFocus));
            startViewpoint.clear();
            targetViewpoint.clear();
        }
    }
};


void powerscale_test()
{
    vsg::dvec3 dv1(1.0, 2.0, 3.0);
    vsg::dvec4 dv2(1.0, 2.0, 3.0, 4.0);

    auto psc_dv1 = vsg::psc_normalize(dv1);
    auto psc_dv2 = vsg::psc_normalize(dv2);

    auto psc_dv3 = psc_add(psc_dv1, psc_dv2);

    auto psc_linear_dv1 = psc_to_linear(psc_dv1);
    auto psc_linear_dv2 = psc_to_linear(psc_dv2);
    auto psc_linear_dv3 = psc_to_linear(psc_dv3);

    std::cout<<"dv1 = ("<<dv1<<")"<<std::endl;
    std::cout<<"dv2 = ("<<dv2<<")"<<std::endl;
    std::cout<<"psc_dv3 = ("<<psc_dv3<<")"<<std::endl;

    std::cout<<"psc_linear_dv1 = ("<<psc_linear_dv1<<")"<<std::endl;
    std::cout<<"psc_linear_dv2 = ("<<psc_linear_dv2<<")"<<std::endl;
    std::cout<<"psc_linear_dv3 = ("<<psc_linear_dv3<<")"<<std::endl;
}

int main(int argc, char** argv)
{
    if (vsg::isPowerScaleSupported())
    {
        std::cout<<"PowerScale is supported."<<std::endl;
    }
    else
    {
        std::cout<<"PowerScale is not supported by the VulkanSceneGraph that this example was built against. This example will not have handle power scaling matrices and result in visual errors."<<std::endl;
        std::cout<<"Please enable PowerScale support in the VulkanSceneGraph by building with VSG_SUPPORTS_PowerScale CMake variable set to 1, then rebuild all dependencies and this example.\n"<<std::endl;
    }

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    if (arguments.read({"--psc-test", "-p"}))
    {
        powerscale_test();
        return 0;
    }

    // set up vsg::Options to pass in filepaths, ReaderWriters and other IO related options to use when reading and writing files.
    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());

    arguments.read(options);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgpowerscaling";
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
    if (arguments.read("--rgb")) options->mapRGBtoRGBAHint = false;
    arguments.read("--file-cache", options->fileCache);

    bool depthClamp = arguments.read({"--dc", "--depthClamp"});
    if (depthClamp)
    {
        std::cout << "Enabled depth clamp." << std::endl;
        auto deviceFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();
        deviceFeatures->get().samplerAnisotropy = VK_TRUE;
        deviceFeatures->get().depthClamp = VK_TRUE;
    }

    VkClearColorValue clearColor{{0.0f, 0.0f, 0.0f, 1.0f}};


    double distance_between_systems = arguments.value<double>(1.0e9, "--distance");
    double speed = arguments.value<double>(1.0, "--speed");

    auto starfieldRadius = arguments.value(distance_between_systems * 4.0, {"--starfield-radius", "--sr"});
    auto numStars = arguments.value(1e5, {"--numstars", "--ns"});
    auto active_viewpoint = arguments.value<std::string>("", {"--viewpoint"});

    // set up the solar system paramaters
    SolarSystemSettings settings;

    settings.flatShaderSet = vsg::createFlatShadedShaderSet(options);
    settings.phongShaderSet = vsg::createPhongShaderSet(options);
    settings.pbrShaderSet = vsg::createPhysicsBasedRenderingShaderSet(options);


    if (depthClamp)
    {
        auto setUpDepthClamp = [](vsg::ShaderSet& shaderSet) -> void
        {
            auto rasterizationState = vsg::RasterizationState::create();
            rasterizationState->depthClampEnable = VK_TRUE;
            shaderSet.defaultGraphicsPipelineStates.push_back(rasterizationState);
        };


        setUpDepthClamp(*settings.flatShaderSet);
        setUpDepthClamp(*settings.phongShaderSet);
        setUpDepthClamp(*settings.pbrShaderSet);
    }


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

    settings.tileDatabaseSettings->shaderSet = settings.phongShaderSet;

    arguments.read("-t", settings.tileDatabaseSettings->lodTransitionScreenHeightRatio);
    arguments.read("-m", settings.tileDatabaseSettings->maxLevel);

    auto universe = vsg::Group::create();


    // create starfield

    if (auto starfield = createStarfield(starfieldRadius, numStars, settings.flatShaderSet))
    {
        universe->addChild(starfield);
    }


    //
    // create solar system one
    //
    settings.name = "1. ";
    settings.sun_color.set(1.0f, 1.0f, 0.7f, 1.0f);
    auto solar_system_one = creteSolarSystem(settings);
    solar_system_one->matrix = vsg::translate(-distance_between_systems * 0.5, 0.0, 0.0);

    settings.name = "2. ";
    settings.sun_color.set(1.0f, 0.8f, 0.7f, 1.0f);
    auto solar_system_two = creteSolarSystem(settings);
    solar_system_two->matrix = vsg::translate(distance_between_systems * 0.5, 0.0, 0.0);

    auto universe_view = vsg::MatrixTransform::create();
    universe_view->setValue("viewpoint", "0. universe_view");
    universe_view->matrix =  vsg::rotate(vsg::radians(70.0), 1.0, 0.0, 0.0) * vsg::translate(0.0, 0.0, distance_between_systems*3.0);

    universe->addChild(solar_system_one);
    universe->addChild(solar_system_two);
    universe->addChild(universe_view);


    //
    // end of creating solar system one
    //

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    if (outputFilename)
    {
        vsg::write(universe, outputFilename);
        return 0;
    }

    // get the viewpoints
    auto viewpoints = vsg::visit<FindViewpoints>(universe).viewpoints;
    for(auto& [name, objectPath] : viewpoints)
    {
        std::cout<<"viewoint ["<<name<<"] ";
        for(auto& obj : objectPath) std::cout<<obj<<" ";
        std::cout<<std::endl;
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
    auto stellarView = StellarView::create();
    stellarView->position = centre + vsg::dvec3(0.0, -radius * 3.5, 0.0);
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
    auto camera = vsg::Camera::create(perspective, stellarView, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    auto animations = vsg::visit<vsg::FindAnimations>(universe).animations;
    for(auto& animation : animations)
    {
        animation->speed = speed;
        viewer->animationManager->play(animation);
    }


    auto stellarManipulator = StellarManipulator::create(stellarView);
    stellarManipulator->viewpoints = viewpoints;
    viewer->addEventHandler(stellarManipulator);

    if (!active_viewpoint.empty())
    {
        auto itr = viewpoints.find(active_viewpoint);
        if (itr != viewpoints.end())
        {
            vsg::info("initial viewpoint : ", itr->first);
            stellarManipulator->currentFocus = itr->second;
        }
    }
    else if (!viewpoints.empty())
    {
        stellarManipulator->currentFocus = viewpoints.begin()->second;
        vsg::info("initial viewpoint : ", viewpoints.begin()->first);
    }


    auto renderGraph = vsg::createRenderGraphForView(window, camera, universe, VK_SUBPASS_CONTENTS_INLINE, false);
    renderGraph->setClearValues(clearColor);

    auto commandGraph = vsg::CommandGraph::create(window, renderGraph);

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

    return 0;
}

