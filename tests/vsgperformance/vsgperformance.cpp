#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

vsg::ref_ptr<vsg::Node> createTextureQuad(vsg::ref_ptr<vsg::Data> sourceData, vsg::ref_ptr<vsg::Options> options)
{
    auto builder = vsg::Builder::create();
    builder->options = options;

    vsg::StateInfo state;
    state.image = sourceData;
    state.lighting = false;

    vsg::GeometryInfo geom;
    geom.dy.set(0.0f, 0.0f, 1.0f);
    geom.dz.set(0.0f, -1.0f, 0.0f);

    return builder->createQuad(geom, state);
}

void enableGenerateDebugInfo(vsg::ref_ptr<vsg::Options> options)
{
    auto shaderHints = vsg::ShaderCompileSettings::create();
    shaderHints->generateDebugInfo = true;

    auto& text = options->shaderSets["text"] = vsg::createTextShaderSet(options);
    text->defaultShaderHints = shaderHints;

    auto& flat = options->shaderSets["flat"] = vsg::createFlatShadedShaderSet(options);
    flat->defaultShaderHints = shaderHints;

    auto& phong = options->shaderSets["phong"] = vsg::createPhongShaderSet(options);
    phong->defaultShaderHints = shaderHints;

    auto& pbr = options->shaderSets["pbr"] = vsg::createPhysicsBasedRenderingShaderSet(options);
    pbr->defaultShaderHints = shaderHints;
}

class CameraPathBuilder : public vsg::Inherit<vsg::Object, CameraPathBuilder>
{
public:
    CameraPathBuilder();

    vsg::ref_ptr<vsg::Camera> camera;
    vsg::dbox bounds;
    vsg::dmat4 localToWorld;
    size_t numPoints = 256; // animation resolution
    double duration = 10.0; // seconds

    virtual void read(vsg::CommandLine& arguments);

    // build CameraSampler animation
    virtual vsg::ref_ptr<vsg::Animation> build() { return {}; }
};

CameraPathBuilder::CameraPathBuilder()
{
}

void CameraPathBuilder::read(vsg::CommandLine& arguments)
{
    arguments.read("--numPoints", numPoints);
    arguments.read("--duration", duration);
}

class OrbitPath : public vsg::Inherit<CameraPathBuilder, OrbitPath>
{
public:
    OrbitPath();

    double pitch = 10.0;        // degrees
    double distanceRatio = 1.0; // non dimensional

    void read(vsg::CommandLine& arguments) override;

    vsg::ref_ptr<vsg::Animation> build() override;
};

OrbitPath::OrbitPath()
{
}

void OrbitPath::read(vsg::CommandLine& arguments)
{
    CameraPathBuilder::read(arguments);

    arguments.read("--pitch", pitch);
    arguments.read("--distanceRatio", distanceRatio);
}

vsg::ref_ptr<vsg::Animation> OrbitPath::build()
{
    auto pathAnimation = vsg::Animation::create();

    auto cameraSampler = vsg::CameraSampler::create();
    pathAnimation->samplers.push_back(cameraSampler);

    cameraSampler->object = camera;
    auto& keyframes = cameraSampler->keyframes = vsg::CameraKeyframes::create();

    vsg::dvec3 center = ((bounds.min + bounds.max) * 0.5);
    double radius = vsg::length(bounds.max - bounds.min) * 0.6;

    vsg::dvec3 up = localToWorld[2].xyz;
    vsg::dvec3 axis(0.0, 0.0, 1.0);

    vsg::dquat tilt(-vsg::radians(pitch), vsg::dvec3(1.0, 0.0, 0.0));

    double time = 0.0;
    double angle = 0.0;
    double angleDelta = 2.0 * vsg::PI / static_cast<double>(numPoints - 1);
    double timeDelta = duration / static_cast<double>(numPoints - 1);

    for (size_t i = 0; i < numPoints; ++i)
    {
        vsg::dquat delta = tilt * vsg::dquat(angle, axis);
        vsg::dvec3 eye = center + delta * vsg::dvec3(0.0, -radius * distanceRatio, 0.0);

        auto matrix = vsg::lookAt(localToWorld * eye, localToWorld * center, up);

        vsg::dvec3 position, scale;
        vsg::dquat rotation;
        vsg::decompose(vsg::inverse(matrix), position, rotation, scale);

        keyframes->add(time, position, rotation);

        angle += angleDelta;
        time += timeDelta;
    }

    return pathAnimation;
}

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        if (arguments.read("--args")) std::cout << arguments << std::endl;

        auto windowTraits = vsg::WindowTraits::create(arguments);

        // if we want to redirect std::cout and std::cerr to the vsg::Logger call vsg::Logger::redirect_stdout()
        if (arguments.read({"--redirect-std", "-r"})) vsg::Logger::instance()->redirect_std();

        // set up vsg::Options to pass in filepaths, ReaderWriters and other IO related options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->sharedObjects = vsg::SharedObjects::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());
#endif

        options->readOptions(arguments);

        if (uint32_t numOperationThreads = 0; arguments.read("--ot", numOperationThreads)) options->operationThreads = vsg::OperationThreads::create(numOperationThreads);

        bool reportAverageFrameRate = true;
        bool reportMemoryStats = arguments.read("--rms");
        if (arguments.read({"-t", "--test"}))
        {
            windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            windowTraits->fullscreen = true;
            reportAverageFrameRate = true;
        }
        if (arguments.read({"--st", "--small-test"}))
        {
            windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            windowTraits->width = 192, windowTraits->height = 108;
            windowTraits->decoration = false;
            reportAverageFrameRate = true;
        }

        uint64_t initialFrameCycleCount = arguments.value(10, "--ifcc");

        bool multiThreading = arguments.read("--mt");
        auto maxTime = arguments.value(std::numeric_limits<double>::max(), "--max-time");

        if (arguments.read("--ThreadLogger")) vsg::Logger::instance() = vsg::ThreadLogger::create();
        auto numFrames = arguments.value(-1, "-f");
        auto pathFilename = arguments.value<vsg::Path>("", "-p");
        auto loadLevels = arguments.value(0, "--load-levels");
        auto maxPagedLOD = arguments.value(0, "--maxPagedLOD");
        auto horizonMountainHeight = arguments.value(0.0, "--hmh");
        auto nearFarRatio = arguments.value<double>(0.001, "--nfr");
        if (arguments.read("--rgb")) options->mapRGBtoRGBAHint = false;

        vsg::ref_ptr<CameraPathBuilder> cameraPathBuilder;
        if (arguments.read("--orbit-path"))
        {
            cameraPathBuilder = OrbitPath::create();
        }

        if (cameraPathBuilder) cameraPathBuilder->read(arguments);

        bool depthClamp = arguments.read({"--dc", "--depthClamp"});
        if (depthClamp)
        {
            std::cout << "Enabled depth clamp." << std::endl;
            auto deviceFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();
            deviceFeatures->get().samplerAnisotropy = VK_TRUE;
            deviceFeatures->get().depthClamp = VK_TRUE;
        }

        vsg::ref_ptr<vsg::ResourceHints> resourceHints;
        if (auto resourceHintsFilename = arguments.value<vsg::Path>("", "--rh"))
        {
            resourceHints = vsg::read_cast<vsg::ResourceHints>(resourceHintsFilename, options);
        }

        if (auto outputResourceHintsFilename = arguments.value<vsg::Path>("", "--orh"))
        {
            if (!resourceHints) resourceHints = vsg::ResourceHints::create();
            vsg::write(resourceHints, outputResourceHintsFilename, options);
            return 0;
        }

        if (arguments.read({"--shader-debug-info", "--sdi"}))
        {
            enableGenerateDebugInfo(options);
            windowTraits->deviceExtensionNames.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
        }

        if (int log_level = 0; arguments.read("--log-level", log_level)) vsg::Logger::instance()->level = vsg::Logger::Level(log_level);
        auto logFilename = arguments.value<vsg::Path>("", "--log");

        vsg::ref_ptr<vsg::Instrumentation> instrumentation;
        if (arguments.read({"--gpu-annotation", "--ga"}) && vsg::isExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        {
            windowTraits->debugUtils = true;

            auto gpu_instrumentation = vsg::GpuAnnotation::create();
            if (arguments.read("--name"))
                gpu_instrumentation->labelType = vsg::GpuAnnotation::SourceLocation_name;
            else if (arguments.read("--className"))
                gpu_instrumentation->labelType = vsg::GpuAnnotation::Object_className;
            else if (arguments.read("--func"))
                gpu_instrumentation->labelType = vsg::GpuAnnotation::SourceLocation_function;

            instrumentation = gpu_instrumentation;
        }
        else if (arguments.read({"--profiler", "--pr"}))
        {
            // set Profiler options
            auto settings = vsg::Profiler::Settings::create();
            arguments.read("--cpu", settings->cpu_instrumentation_level);
            arguments.read("--gpu", settings->gpu_instrumentation_level);
            arguments.read("--log-size", settings->log_size);

            // create the profiler
            instrumentation = vsg::Profiler::create(settings);
        }

        vsg::Affinity affinity;
        uint32_t cpu = 0;
        while (arguments.read("-c", cpu))
        {
            affinity.cpus.insert(cpu);
        }

        // should animations be automatically played
        auto autoPlay = !arguments.read({"--no-auto-play", "--nop"});

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        if (argc <= 1)
        {
            std::cout << "Please specify a 3d model or image file on the command line." << std::endl;
            return 1;
        }

        auto group = vsg::Group::create();

        vsg::Path path;

        // read any vsg files
        for (int i = 1; i < argc; ++i)
        {
            vsg::Path filename = arguments[i];
            path = vsg::filePath(filename);

            auto object = vsg::read(filename, options);
            if (auto node = object.cast<vsg::Node>())
            {
                group->addChild(node);
            }
            else if (auto data = object.cast<vsg::Data>())
            {
                if (auto textureGeometry = createTextureQuad(data, options))
                {
                    group->addChild(textureGeometry);
                }
            }
            else if (object)
            {
                std::cout << "Unable to view object of type " << object->className() << std::endl;
            }
            else
            {
                std::cout << "Unable to load file " << filename << std::endl;
            }
        }

        if (group->children.empty())
        {
            return 1;
        }

        vsg::ref_ptr<vsg::Node> vsg_scene;
        if (group->children.size() == 1)
            vsg_scene = group->children[0];
        else
            vsg_scene = group;

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create window." << std::endl;
            return 1;
        }

        viewer->addWindow(window);

        // get the extents of the scene, and use localToWorld transform if the scene contains an region of a ECEF database.
        vsg::dmat4 localToWorld;
        vsg::ComputeBounds computeBounds;

        vsg::ref_ptr<vsg::LookAt> lookAt;
        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;

        auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
        if (ellipsoidModel)
        {
            // compute the bounds of the scene graph to help position camera
            vsg_scene->accept(computeBounds);

            double initialRadius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5;
            double modelToEarthRatio = (initialRadius / ellipsoidModel->radiusEquator());

            // if the model is small compared to the radius of the earth position camera in local coordinate frame of the model rather than ECEF.
            if (modelToEarthRatio < 1.0)
            {
                vsg::dvec3 lla = ellipsoidModel->convertECEFToLatLongAltitude((computeBounds.bounds.min + computeBounds.bounds.max) * 0.5);

                localToWorld = ellipsoidModel->computeLocalToWorldTransform(lla);
                auto worldToLocal = ellipsoidModel->computeWorldToLocalTransform(lla);

                // recompute the bounds of the model in the local coordinate frame of the model, rather than ECEF
                // to give a tigher bound around the dataset.
                computeBounds.matrixStack.clear();
                computeBounds.matrixStack.push_back(worldToLocal);
                computeBounds.bounds.reset();
                computeBounds.useNodeBounds = false;
                vsg_scene->accept(computeBounds);

                auto bounds = computeBounds.bounds;
                vsg::dvec3 center = (bounds.min + bounds.max) * 0.5;
                double radius = vsg::length(bounds.max - bounds.min) * 0.5;

                lookAt = vsg::LookAt::create(localToWorld * (center + vsg::dvec3(0.0, 0.0, radius)), localToWorld * center, vsg::dvec3(0.0, 1.0, 0.0) * worldToLocal);
                perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.5);
            }
            else
            {
                lookAt = vsg::LookAt::create(vsg::dvec3(initialRadius * 2.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0));
                perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
            }
        }
        else
        {
            // compute the bounds of the scene graph to help position camera
            vsg_scene->accept(computeBounds);

            vsg::dvec3 center = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
            double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

            // set up the camera
            lookAt = vsg::LookAt::create(center + vsg::dvec3(0.0, -radius * 3.5, 0.0), center, vsg::dvec3(0.0, 0.0, 1.0));
            perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.5);
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        vsg::ref_ptr<vsg::Animation> pathAnimation;
        if (cameraPathBuilder)
        {
            cameraPathBuilder->bounds = computeBounds.bounds;
            cameraPathBuilder->localToWorld = localToWorld;
            pathAnimation = cameraPathBuilder->build();
        }

        // add close handler to respond to the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        vsg::ref_ptr<vsg::CameraAnimationHandler> cameraAnimation;

        if (pathFilename)
        {
            cameraAnimation = vsg::CameraAnimationHandler::create(camera, pathFilename, options);
        }
        else if (pathAnimation)
        {
            cameraAnimation = vsg::CameraAnimationHandler::create(camera, pathAnimation, "saved_animation.vsgt", options);
        }
        else
        {
            cameraAnimation = vsg::CameraAnimationHandler::create(camera, "saved_animation.vsgt", options);
        }

        viewer->addEventHandler(cameraAnimation);
        if (autoPlay && cameraAnimation->animation)
        {
            cameraAnimation->play();

            if (reportAverageFrameRate && maxTime == std::numeric_limits<double>::max())
            {
                maxTime = cameraAnimation->animation->maxTime();
            }
        }

        viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

        // if required preload specific number of PagedLOD levels.
        if (loadLevels > 0)
        {
            vsg::LoadPagedLOD loadPagedLOD(camera, loadLevels);

            auto startTime = vsg::clock::now();

            vsg_scene->accept(loadPagedLOD);

            auto time = std::chrono::duration<float, std::chrono::milliseconds::period>(vsg::clock::now() - startTime).count();
            std::cout << "No. of tiles loaded " << loadPagedLOD.numTiles << " in " << time << "ms." << std::endl;
        }

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        if (instrumentation) viewer->assignInstrumentation(instrumentation);

        if (multiThreading)
        {
            viewer->setupThreading();

            if (affinity)
            {
                auto cpu_itr = affinity.cpus.begin();

                // set affinity of main thread
                if (cpu_itr != affinity.cpus.end())
                {
                    std::cout << "vsg::setAffinity() " << *cpu_itr << std::endl;
                    vsg::setAffinity(vsg::Affinity(*cpu_itr++));
                }

                for (auto& thread : viewer->threads)
                {
                    if (thread.joinable() && cpu_itr != affinity.cpus.end())
                    {
                        std::cout << "vsg::setAffinity(" << thread.get_id() << ") " << *cpu_itr << std::endl;
                        vsg::setAffinity(thread, vsg::Affinity(*cpu_itr++));
                    }
                }
            }
        }
        else if (affinity)
        {
            std::cout << "vsg::setAffinity(";
            for (auto cpu_num : affinity.cpus)
            {
                std::cout << " " << cpu_num;
            }
            std::cout << " )" << std::endl;

            vsg::setAffinity(affinity);
        }

        viewer->compile(resourceHints);

        if (maxPagedLOD > 0)
        {
            // set targetMaxNumPagedLODWithHighResSubgraphs after Viewer::compile() as it will assign any DatabasePager if required.
            for (auto& task : viewer->recordAndSubmitTasks)
            {
                if (task->databasePager) task->databasePager->targetMaxNumPagedLODWithHighResSubgraphs = maxPagedLOD;
            }
        }

        if (autoPlay)
        {
            // find any animation groups in the loaded scene graph and play the first animation in each of the animation groups.
            auto animationGroups = vsg::visit<vsg::FindAnimations>(vsg_scene).animationGroups;
            for (auto ag : animationGroups)
            {
                if (!ag->animations.empty()) viewer->animationManager->play(ag->animations.front());
            }
        }

        if (initialFrameCycleCount > 0)
        {
            // run an initial set of frames to get past the intiial frame time variability so we get stable frame rate stats
            while ((initialFrameCycleCount-- > 0) && viewer->advanceToNextFrame())
            {
                viewer->getFrameStamp()->simulationTime = 0.0;
                viewer->handleEvents();
                viewer->update();
                viewer->recordAndSubmit();
                viewer->present();
            }
        }

        if (viewer->active())
        {
            // reset to start timing.
            viewer->getFrameStamp()->frameCount = 0;
            viewer->getFrameStamp()->simulationTime = 0.0;
            auto start_point = viewer->start_point() = vsg::clock::now();

            uint64_t frameCount = 0;

            // rendering main loop
            while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0) && (viewer->getFrameStamp()->simulationTime < maxTime))
            {
                if (frameCount == 0) start_point = viewer->getFrameStamp()->time;

                viewer->handleEvents();
                viewer->update();
                viewer->recordAndSubmit();
                viewer->present();

                ++frameCount;
            }

            if (reportAverageFrameRate)
            {
                auto fs = viewer->getFrameStamp();
                double fps = static_cast<double>(frameCount) / std::chrono::duration<double, std::chrono::seconds::period>(fs->time - start_point).count();
                std::cout << "Num of frames = " << fs->frameCount << ", average frame rate = " << fps << " fps" << std::endl;
                std::cout << "frameCount = " << frameCount << std::endl;
            }
        }
        else
        {
            std::cout << "Insufficient runtime, no frame stats collected." << std::endl;
        }

        if (reportMemoryStats)
        {
            if (options->sharedObjects)
            {
                vsg::LogOutput output;
                options->sharedObjects->report(output);
            }
        }

        if (auto profiler = instrumentation.cast<vsg::Profiler>())
        {
            instrumentation->finish();
            if (logFilename)
            {
                std::ofstream fout(logFilename);
                profiler->log->report(fout);
            }
            else
            {
                profiler->log->report(std::cout);
            }
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
