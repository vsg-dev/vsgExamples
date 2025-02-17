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
    vsg::dvec3 position;
    vsg::dquat rotation;
    bool useCoordinateFrame = false;
    double windowAspectRatio = 1.25;
    double z_near = 1e2;
    double z_far = 1e15;

    vsg::ref_ptr<vsg::ShaderSet> flatShaderSet;
    vsg::ref_ptr<vsg::ShaderSet> phongShaderSet;
    vsg::ref_ptr<vsg::ShaderSet> pbrShaderSet;
};

vsg::ref_ptr<vsg::Node> createStarfield(double maxRadius, size_t numStars, vsg::ref_ptr<vsg::ShaderSet> shaderSet)
{
    if (numStars == 0) return {};

    // set up the star positions
    auto vertices = vsg::vec3Array::create(numStars);
    auto colors = vsg::vec4Array::create(numStars);
    for (auto& v : *vertices)
    {
        v.x = 2.0 * maxRadius * (static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX) - 0.5);
        v.y = 2.0 * maxRadius * (static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX) - 0.5);
        v.z = 2.0 * maxRadius * (static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX) - 0.5);
    }

    for (auto& c : *colors)
    {
        float brightness = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        c.set(brightness, brightness, brightness, 1.0);
    }

    auto normals = vsg::vec3Value::create(vsg::vec3(0.0f, 0.0f, 1.0f));
    auto texcoords = vsg::vec2Value::create(vsg::vec2(1.0f, 1.0f));

    // configure the state and array layouts to assign to the scene graph
    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(shaderSet);

    graphicsPipelineConfig->shaderHints->defines.insert("VSG_POINT_SPRITE");

    vsg::DataList vertexArrays;
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_INSTANCE, normals);
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

vsg::ref_ptr<vsg::Node> creteSolarSystem(SolarSystemSettings& settings)
{
    double day = 24.0 * 60.0 * 60.0;
    double year = 365.25 * day;
    double earth_radius = settings.tileDatabaseSettings->ellipsoidModel->radiusEquator();

    // create earth one
    auto earth = vsg::TileDatabase::create();
    earth->settings = settings.tileDatabaseSettings;
    earth->readDatabase(settings.options);

    auto earth_view = vsg::MatrixTransform::create();
    earth_view->setValue("viewpoint", settings.name + "_3_palace_view");
    earth_view->setObject("projection", vsg::Perspective::create(0.005, settings.windowAspectRatio, settings.z_near, settings.z_far)); // min FOV 0.005
    earth_view->matrix = vsg::rotate(vsg::radians(-51.315), 0.0, 1.0, 0.0) * vsg::rotate(vsg::radians(89.915), 0.0, 0.0, 1.0) * vsg::rotate(vsg::radians(90.0), 1.0, 0.0, 0.0) * vsg::translate(0.0, 0.0, earth_radius * 5.0);

    auto earth_rotation_about_axis = vsg::MatrixTransform::create();
    earth_rotation_about_axis->addChild(earth);
    earth_rotation_about_axis->addChild(earth_view);

    auto orbit_view = vsg::MatrixTransform::create();
    orbit_view->setValue("viewpoint", settings.name + "_2_orbit_view");
    orbit_view->setObject("projection", vsg::Perspective::create(90, settings.windowAspectRatio, settings.z_near, settings.z_far));
    orbit_view->matrix = vsg::rotate(vsg::radians(45.0), 0.0, 0.0, 1.0) * vsg::rotate(vsg::radians(90.0), 1.0, 0.0, 0.0) * vsg::translate(0.0, 0.0, earth_radius * 5.0);

    auto earth_position_from_sun = vsg::MatrixTransform::create();
    earth_position_from_sun->addChild(earth_rotation_about_axis);
    earth_position_from_sun->addChild(orbit_view);
    earth_position_from_sun->matrix = vsg::translate(settings.earth_to_sun_distance, 0.0, 0.0);

    auto earth_orbit_transform = vsg::MatrixTransform::create();
    earth_orbit_transform->addChild(earth_position_from_sun);

    // animate the earths rotation around it's axis
    auto earth_keyframes = vsg::TransformKeyframes::create();
    earth_keyframes->add(0.0, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(0.0), vsg::dvec3(0.0, 0.0, 1.0)));
    earth_keyframes->add(day * 0.5, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(180.0), vsg::dvec3(0.0, 0.0, 1.0)));
    earth_keyframes->add(day, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(360.0), vsg::dvec3(0.0, 0.0, 1.0)));

    auto earth_rotation_about_axisSampler = vsg::TransformSampler::create();
    earth_rotation_about_axisSampler->keyframes = earth_keyframes;
    earth_rotation_about_axisSampler->object = earth_rotation_about_axis;

    auto earth_animation = vsg::Animation::create();
    earth_animation->samplers.push_back(earth_rotation_about_axisSampler);

    // animate the earths rotation around the sun
    auto orbit_keyframes = vsg::TransformKeyframes::create();
    orbit_keyframes->add(0.0, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(0.0), vsg::dvec3(0.0, 0.0, 1.0)));
    orbit_keyframes->add(year * 0.5, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(180.0), vsg::dvec3(0.0, 0.0, 1.0)));
    orbit_keyframes->add(year, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(360.0), vsg::dvec3(0.0, 0.0, 1.0)));

    auto orbit_transformSampler = vsg::TransformSampler::create();
    orbit_transformSampler->keyframes = orbit_keyframes;
    orbit_transformSampler->object = earth_orbit_transform;

    auto oribit_animation = vsg::Animation::create();
    oribit_animation->samplers.push_back(orbit_transformSampler);

    auto animationGroup = vsg::AnimationGroup::create();
    animationGroup->animations.push_back(earth_animation);
    animationGroup->animations.push_back(oribit_animation);

    // create sun

    settings.builder->shaderSet = settings.flatShaderSet;

    // vsg::Builder uses floats for sizing as it's intended for small local objects,
    // we'll ignore limitations for now as we won't be going close to sun's surface'
    vsg::GeometryInfo geom;
    geom.dx.set(2.0f * settings.sun_radius, 0.0f, 0.0f);
    geom.dy.set(0.0f, 2.0f * settings.sun_radius, 0.0f);
    geom.dz.set(0.0f, 0.0f, 2.0f * settings.sun_radius);
    geom.color = settings.sun_color;
    //geom.cullNode = false;

    vsg::StateInfo state;
    state.lighting = false;

    auto sun = settings.builder->createSphere(geom, state);

    auto sun_view = vsg::MatrixTransform::create();
    sun_view->setValue("viewpoint", settings.name + "_1_sun_view");
    sun_view->setObject("projection", vsg::Perspective::create(60, settings.windowAspectRatio, settings.z_near, settings.z_far)); // min FOV 0.005
    sun_view->matrix = vsg::rotate(vsg::radians(70.0), 1.0, 0.0, 0.0) * vsg::translate(0.0, 0.0, settings.earth_to_sun_distance * 3.0);

    auto light = vsg::PointLight::create();
    light->intensity = settings.earth_to_sun_distance * settings.earth_to_sun_distance;
    light->position.set(0.0f, 0.0f, 0.0f);
    light->color = settings.sun_color.rgb;

    if (settings.useCoordinateFrame)
    {
        auto solar_system = vsg::CoordinateFrame::create();

        solar_system->addChild(sun);
        solar_system->addChild(sun_view);
        solar_system->addChild(light);
        solar_system->addChild(earth_orbit_transform);
        solar_system->addChild(animationGroup);

        solar_system->name = settings.name;
        solar_system->origin = settings.position;
        solar_system->rotation = settings.rotation;

        return solar_system;
    }
    else
    {
        auto solar_system = vsg::MatrixTransform::create();

        solar_system->addChild(sun);
        solar_system->addChild(sun_view);
        solar_system->addChild(light);
        solar_system->addChild(earth_orbit_transform);
        solar_system->addChild(animationGroup);

        solar_system->matrix = vsg::translate(settings.position) * vsg::rotate(settings.rotation);

        return solar_system;
    }
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

struct MyComputeTransform : public vsg::Visitor
{
    vsg::ldvec3 origin;
    vsg::dmat4 matrix;

    vsg::ref_ptr<vsg::ProjectionMatrix> projection;

    void apply(vsg::Object& object) override
    {
        if (auto user_projection = object.getObject<vsg::ProjectionMatrix>("projection")) projection = user_projection;
    }

    void apply(vsg::Transform& transform) override
    {
        apply(static_cast<vsg::Object&>(transform));

        matrix = transform.transform(matrix);
    }

    void apply(vsg::CoordinateFrame& cf) override
    {
        apply(static_cast<vsg::Object&>(cf));

        origin = cf.origin;
        matrix = vsg::rotate(cf.rotation);
    }

    void apply(vsg::MatrixTransform& mt) override
    {
        apply(static_cast<vsg::Object&>(mt));

        matrix = matrix * mt.matrix;
    }

    void apply(vsg::Camera& camera) override
    {
        if (camera.projectionMatrix)
        {
            projection = camera.projectionMatrix;
        }
        if (camera.viewMatrix)
        {
            matrix = matrix * camera.viewMatrix->inverse();
        }
    }

    template<typename T>
    void apply(T& nodePath)
    {
        for (auto& node : nodePath) node->accept(*this);
    }
};

class StellarManipulator : public vsg::Inherit<vsg::Visitor, StellarManipulator>
{
public:
    vsg::ref_ptr<vsg::Camera> camera;
    vsg::ref_ptr<vsg::ViewMatrix> viewMatrix;
    vsg::ref_ptr<vsg::ProjectionMatrix> projectionMatrix;
    double windowAspectRatio = 1.0;

    std::multimap<std::string, vsg::RefObjectPath> viewpoints;
    vsg::RefObjectPath currentFocus;

    double animationDuration = 5.0;
    vsg::time_point startTime;
    vsg::RefObjectPath startViewpoint;
    vsg::RefObjectPath targetViewpoint;

    StellarManipulator(vsg::ref_ptr<vsg::Camera> in_camera) :
        camera(in_camera)
    {
        viewMatrix = camera->viewMatrix;
        projectionMatrix = camera->projectionMatrix;

        auto viewport = camera->getViewport();
        windowAspectRatio = static_cast<double>(viewport.width) / static_cast<double>(viewport.height);
    }

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (keyPress.keyBase >= vsg::KEY_1 && keyPress.keyBase <= vsg::KEY_9)
        {
            if (viewpoints.empty()) return;

            uint16_t key = vsg::KEY_1;
            auto itr = viewpoints.begin();
            while (key < keyPress.keyBase && itr != viewpoints.end())
            {
                ++itr;
                ++key;
            }

            if (itr == viewpoints.end()) return;

            if (animationDuration <= 0.0 || currentFocus.empty())
            {
                currentFocus = itr->second;
            }
            else
            {
                startTime = keyPress.time;
                startViewpoint = currentFocus;
                targetViewpoint = itr->second;
            }
        }
    }

    void apply(vsg::ConfigureWindowEvent& congfigureWindow) override
    {
        windowAspectRatio = static_cast<double>(congfigureWindow.width) / static_cast<double>(congfigureWindow.height);
        vsg::info("windowAspectRatio = ", windowAspectRatio);
    }

    void apply(vsg::FrameEvent& frame) override
    {
        using origin_value_type = decltype(MyComputeTransform::origin)::value_type;

        if (!targetViewpoint.empty() && !startViewpoint.empty())
        {
            double timeSinceAnimationStart = std::chrono::duration<double, std::chrono::seconds::period>(frame.time - startTime).count();

            if (timeSinceAnimationStart < animationDuration)
            {
                MyComputeTransform startTransform;
                startTransform.apply(startViewpoint);

                MyComputeTransform targetTransform;
                targetTransform.apply(targetViewpoint);

                vsg::dvec3 startTranslation, startScale, targetTranslation, targetScale;
                vsg::dquat startRotation, targetRotation;

                vsg::decompose(startTransform.matrix, startTranslation, startRotation, startScale);
                vsg::decompose(targetTransform.matrix, targetTranslation, targetRotation, targetScale);

                double tr = (timeSinceAnimationStart / animationDuration);
                double r = 1.0 - (1.0 + cos(tr * vsg::PI)) * 0.5;

                auto perspective = vsg::cast<vsg::Perspective>(projectionMatrix);
                auto startPerspective = vsg::cast<vsg::Perspective>(startTransform.projection);
                auto targetPerspective = vsg::cast<vsg::Perspective>(targetTransform.projection);
                if (perspective && startPerspective && targetPerspective)
                {
                    perspective->fieldOfViewY = vsg::mix(startPerspective->fieldOfViewY, targetPerspective->fieldOfViewY, r);
                }

                if (auto lookDirection = vsg::cast<vsg::LookDirection>(viewMatrix))
                {
                    lookDirection->origin = vsg::mix(startTransform.origin, targetTransform.origin, static_cast<origin_value_type>(r));
                    lookDirection->position = vsg::mix(startTranslation, targetTranslation, r);
                    lookDirection->rotation = vsg::mix(startRotation, targetRotation, r);
                }
                else if (auto lookAt = vsg::cast<vsg::LookAt>(viewMatrix))
                {
                    vsg::dvec3 position = vsg::mix(startTranslation, targetTranslation, r);
                    vsg::dquat rotation = vsg::mix(startRotation, targetRotation, r);

                    vsg::dvec3 lookVector(0.0, 0.0, -1.0);
                    vsg::dvec3 upVector(0.0, 1.0, 0.0);
                    double lookDistance = vsg::length(lookAt->center - lookAt->eye);

                    lookVector = rotation * lookVector;
                    upVector = rotation * upVector;

                    lookAt->origin = vsg::mix(startTransform.origin, targetTransform.origin, static_cast<origin_value_type>(r));
                    lookAt->eye = position;
                    lookAt->center = position + lookVector * lookDistance;
                    lookAt->up = upVector;
                }

                return;
            }
            else
            {
                currentFocus = targetViewpoint;
            }
        }

        if (!currentFocus.empty())
        {
            MyComputeTransform mct;
            mct.apply(currentFocus);

            viewMatrix->origin = mct.origin;
            if (auto lookDirection = vsg::cast<vsg::LookDirection>(viewMatrix))
            {
                lookDirection->set(mct.matrix);
            }
            else if (auto lookAt = vsg::cast<vsg::LookAt>(viewMatrix))
            {
                lookAt->set(mct.matrix);
            }

            auto perspective = vsg::cast<vsg::Perspective>(projectionMatrix);
            auto targetPerspective = vsg::cast<vsg::Perspective>(mct.projection.get());
            if (targetPerspective)
            {
                perspective->fieldOfViewY = targetPerspective->fieldOfViewY;
            }
            if (perspective) perspective->aspectRatio = windowAspectRatio;

            startViewpoint.clear();
            targetViewpoint.clear();
        }
    }
};

template<typename T>
T precision(T v)
{
    T delta = v;
    while (v < (v + delta))
    {
        delta /= static_cast<T>(2.0);
    }
    //
    while (v == (v + delta))
    {
        delta *= static_cast<T>(1.001);
    }

    return delta;
}

template<typename T>
void print_bytes(std::ostream& out, T value)
{
    out << value << " {\t";
    uint8_t* ptr = reinterpret_cast<uint8_t*>(&value);
    for (size_t i = 0; i < sizeof(T); ++i)
    {
        out << static_cast<uint32_t>(*(ptr + i)) << "\t";
    }
    out << "}" << std::endl;
}

void numerical_test()
{

    std::cout << "\nnative_long_double_bits() = " << vsg::native_long_double_bits() << std::endl;

    std::cout << "\nnumeric_limits<>" << std::endl;
    std::cout << "std::numeric_limits<float>::max() = " << std::numeric_limits<float>::max() << ", digits " << std::numeric_limits<float>::digits << ", digits10 " << std::numeric_limits<float>::digits10 << ", sizeof<float> = " << sizeof(float) << std::endl;
    std::cout << "std::numeric_limits<double>::max() = " << std::numeric_limits<double>::max() << ", digits " << std::numeric_limits<double>::digits << ", digits10 " << std::numeric_limits<double>::digits10 << ", sizeof<double> = " << sizeof(double) << std::endl;
    std::cout << "std::numeric_limits<long double>::max() = " << std::numeric_limits<long double>::max() << ", digits " << std::numeric_limits<long double>::digits << ", digits10 " << std::numeric_limits<long double>::digits10 << ", sizeof<long double> = " << sizeof(long double) << std::endl;

    std::cout << "\nprecision around offset " << std::endl;
    std::cout << "    precision<float>(1e26) = " << precision<float>(1.0e26) << std::endl;
    std::cout << "    precision<double>(1e26) = " << precision<double>(1.0e26) << std::endl;
    std::cout << "    precision<double>(1e26) = " << precision<long double>(1.0e26) << std::endl;

    double d_0 = 0.0L;
    double d_half = 0.5L;
    double d_1 = 1.0L;
    double d_2 = 2.0L;
    double d_1024 = 1024.0L;
    double d_max = std::numeric_limits<double>::max();

    double d_n0 = -0.0L;
    double d_nhalf = -0.5L;
    double d_n1 = -1.0L;
    double d_n2 = -2.0L;
    double d_n1024 = -1024.0L;
    double d_lowest = std::numeric_limits<double>::lowest();

    std::cout << "\ndoubles:" << std::endl;

    print_bytes(std::cout, d_0);
    print_bytes(std::cout, d_half);
    print_bytes(std::cout, d_1);
    print_bytes(std::cout, d_2);
    print_bytes(std::cout, d_1024);

    print_bytes(std::cout, d_n0);
    print_bytes(std::cout, d_nhalf);
    print_bytes(std::cout, d_n1);
    print_bytes(std::cout, d_n2);
    print_bytes(std::cout, d_n1024);

    print_bytes(std::cout, d_max);
    print_bytes(std::cout, d_lowest);
    print_bytes(std::cout, std::numeric_limits<double>::max());

    std::cout << "\nlong doubles:" << std::endl;
    long double ld_0 = 0.0L;
    long double ld_half = 0.5L;
    long double ld_1 = 1.0L;
    long double ld_2 = 2.0L;
    long double ld_1024 = 1024.0L;
    long double ld_max = std::numeric_limits<long double>::max();

    long double ld_n0 = -0.0L;
    long double ld_nhalf = -0.5L;
    long double ld_n1 = -1.0L;
    long double ld_n2 = -2.0L;
    long double ld_n1024 = -1024.0L;
    long double ld_lowest = std::numeric_limits<long double>::lowest();

    print_bytes(std::cout, ld_0);
    print_bytes(std::cout, ld_half);
    print_bytes(std::cout, ld_1);
    print_bytes(std::cout, ld_2);
    print_bytes(std::cout, ld_1024);

    print_bytes(std::cout, ld_n0);
    print_bytes(std::cout, ld_nhalf);
    print_bytes(std::cout, ld_n1);
    print_bytes(std::cout, ld_n2);
    print_bytes(std::cout, ld_n1024);

    print_bytes(std::cout, ld_max);
    print_bytes(std::cout, ld_lowest);
    print_bytes(std::cout, std::numeric_limits<long double>::max());

    std::cout << "sizeof(double) = " << sizeof(double) << ", " << typeid(double).name() << std::endl;
    std::cout << "sizeof(long double) = " << sizeof(long double) << ", " << typeid(long double).name() << std::endl;
}

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        if (arguments.read({"--numerical-test", "--nt"}))
        {
            numerical_test();
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
        windowTraits->depthFormat = VK_FORMAT_D32_SFLOAT;
        windowTraits->windowTitle = "vsgcoordinateframe";
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

        auto clearColor = arguments.value(vsg::vec4(0.0f, 0.0f, 0.0f, 1.0f), "--clear");

        bool playAnimations = arguments.read("--play");

        double distance_between_systems = arguments.value<double>(1.0e9, "--distance");
        if (arguments.read({"--worst-cast", "--wc"})) distance_between_systems = 8.514e25;

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
            auto setUpDepthClamp = [](vsg::ShaderSet& shaderSet) -> void {
                auto rasterizationState = vsg::RasterizationState::create();
                rasterizationState->depthClampEnable = VK_TRUE;
                shaderSet.defaultGraphicsPipelineStates.push_back(rasterizationState);
            };

            setUpDepthClamp(*settings.flatShaderSet);
            setUpDepthClamp(*settings.phongShaderSet);
            setUpDepthClamp(*settings.pbrShaderSet);
        }

        settings.windowAspectRatio = static_cast<double>(windowTraits->width) / static_cast<double>(windowTraits->height);
        settings.options = options;
        settings.builder = vsg::Builder::create();
        settings.builder->options = options;

        settings.sun_radius = arguments.value<double>(6.957e7, "--sun-radius");
        settings.earth_to_sun_distance = arguments.value<double>(1.49e8, {"--earth-to-sun-distance", "--sd"}); //1.49e11;
        settings.useCoordinateFrame = !arguments.read("--mt");

        settings.z_near = 1e3; // 1000m
        settings.z_far = starfieldRadius * 2.0;

        arguments.read({"--near-far", "--nf"}, settings.z_near, settings.z_far);
        arguments.read({"--near", "-n"}, settings.z_near);
        arguments.read({"--far", "-f"}, settings.z_far);

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
            settings.tileDatabaseSettings->originTopLeft = false;
            settings.tileDatabaseSettings->imageLayer = "http://readymap.org/readymap/tiles/1.0.0/7/{z}/{x}/{y}.jpeg";
        }

        if (arguments.read("--rme"))
        {
            // setup ready map settings
            settings.tileDatabaseSettings = vsg::TileDatabaseSettings::create();
            settings.tileDatabaseSettings->extents = {{-180.0, -90.0, 0.0}, {180.0, 90.0, 1.0}};
            settings.tileDatabaseSettings->noX = 2;
            settings.tileDatabaseSettings->noY = 1;
            settings.tileDatabaseSettings->maxLevel = 10;
            settings.tileDatabaseSettings->originTopLeft = false;
            settings.tileDatabaseSettings->imageLayer = "http://readymap.org/readymap/tiles/1.0.0/7/{z}/{x}/{y}.jpeg";
            settings.tileDatabaseSettings->elevationLayer = "http://readymap.org/readymap/tiles/1.0.0/116/{z}/{x}/{y}.tif";
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

        float ambientLightIntensity = arguments.value<float>(0.0f, "--ambient");
        if (ambientLightIntensity > 0.0f)
        {
            auto ambientLight = vsg::AmbientLight::create();
            ambientLight->intensity = ambientLightIntensity;
            universe->addChild(ambientLight);
        }

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
        settings.position.set(-distance_between_systems * 0.5, 0.0, 0.0);
        auto solar_system_one = creteSolarSystem(settings);

        settings.name = "2. ";
        settings.sun_color.set(1.0f, 0.8f, 0.7f, 1.0f);
        settings.position.set(distance_between_systems * 0.5, 0.0, 0.0);
        settings.rotation.set(vsg::radians(90.0), vsg::dvec3(1.0, 0.0, 0.0));
        auto solar_system_two = creteSolarSystem(settings);

        auto universe_view = vsg::MatrixTransform::create();
        universe_view->setValue("viewpoint", "0. universe_view");
        universe_view->setObject("projection", vsg::Perspective::create(60, settings.windowAspectRatio, settings.z_near, settings.z_far));
        universe_view->matrix = vsg::rotate(vsg::radians(70.0), 1.0, 0.0, 0.0) * vsg::translate(0.0, 0.0, distance_between_systems * 3.0);

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
        for (auto& [name, objectPath] : viewpoints)
        {
            std::cout << "viewpoint [" << name << "] ";
            for (auto& obj : objectPath) std::cout << obj << " ";
            std::cout << std::endl;
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

        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        universe->accept(computeBounds);
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
        vsg::dvec3 center = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        vsg::dvec3 eye = center + vsg::dvec3(0.0, -radius * 3.5, 0.0);

        vsg::info("universe bounds computeBounds.bounds = ", computeBounds.bounds);

        // set up the camera
        vsg::ref_ptr<vsg::ViewMatrix> viewMatrix;
        if (arguments.read("--lookAt"))
        {
            auto lookAt = vsg::LookAt::create();
            lookAt->origin.set(0.0, 0.0, 0.0);
            lookAt->eye = eye;
            lookAt->center = center;
            lookAt->up.set(0.0, 0.0, 1.0);
            viewMatrix = lookAt;
        }
        else
        {
            auto lookDirection = vsg::LookDirection::create();
            lookDirection->origin.set(0.0, 0.0, 0.0);
            lookDirection->position = eye;
            viewMatrix = lookDirection;
        }

        settings.windowAspectRatio = static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height);

        auto perspectve = vsg::Perspective::create(30.0, settings.windowAspectRatio, settings.z_near, settings.z_far);
        auto camera = vsg::Camera::create(perspectve, viewMatrix, vsg::ViewportState::create(window->extent2D()));

        // add close handler to respond to the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        if (playAnimations)
        {
            auto animations = vsg::visit<vsg::FindAnimations>(universe).animations;
            for (auto& animation : animations)
            {
                animation->speed = speed;
                viewer->animationManager->play(animation);
            }
        }

        if (arguments.read({"--interactive", "-i"}))
        {
            // set up manipulator for interactively moving between viewpoints
            auto stellarManipulator = StellarManipulator::create(camera);
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
        }
        else
        {
            // set up camera animation to take you between each viewpoint
            auto cameraAnimationHandler = vsg::CameraAnimationHandler::create();
            cameraAnimationHandler->animation = vsg::Animation::create();
            cameraAnimationHandler->object = camera;

            auto cameraSampler = cameraAnimationHandler->cameraSampler = vsg::CameraSampler::create();
            cameraSampler->object = camera;
            cameraAnimationHandler->animation->samplers.push_back(cameraSampler);

            auto cameraKeyframes = cameraSampler->keyframes = vsg::CameraKeyframes::create();
            double time = 0.0;
            double time_pause = 3.0;
            double time_moving = 6.0;
            for (auto& [name, objectPath] : viewpoints)
            {
                cameraKeyframes->tracking.push_back(vsg::time_path{time, objectPath});
                cameraKeyframes->tracking.push_back(vsg::time_path{time + time_pause, objectPath});

                for (auto& object : objectPath)
                {
                    if (auto perspective = object->getRefObject<vsg::Perspective>("projection"))
                    {
                        cameraKeyframes->fieldOfViews.push_back(vsg::time_double{time, perspective->fieldOfViewY});
                        cameraKeyframes->fieldOfViews.push_back(vsg::time_double{time + time_pause, perspective->fieldOfViewY});
                    }
                }

                time += (time_pause + time_moving);
            }

            cameraAnimationHandler->play();

            viewer->addEventHandler(cameraAnimationHandler);
        }

        auto renderGraph = vsg::createRenderGraphForView(window, camera, universe, VK_SUBPASS_CONTENTS_INLINE, false);
        renderGraph->setClearValues(vsg::sRGB_to_linear(clearColor));

        auto commandGraph = vsg::CommandGraph::create(window, renderGraph);

        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        viewer->compile();

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
        {
            // update all the node positions, do this first as event handlers update the camera based on these positions
            viewer->update();

            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->recordAndSubmit();

            viewer->present();
        }

        return 0;
    }
    catch (const vsg::Exception& ve)
    {
        for (int i = 0; i < argc; ++i) std::cerr << argv[i] << " ";
        std::cerr << "\n[Exception] - " << ve.message << " result = " << ve.result << std::endl;
        return 1;
    }
}
