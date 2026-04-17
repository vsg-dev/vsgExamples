#include "depthpeeling/Builder.h"

#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

vsg::ref_ptr<vsg::Node> createBox(vsg::oit::depthpeeling::Builder& builder, 
    vsg::ref_ptr<vsg::Data> texture, vsg::vec4 color, vsg::vec3 offset)
{
    using namespace vsg;

    vsg::ref_ptr<vsg::Data> positions;

    auto colors = vsg::vec4Array::create(1, color);

    const vsg::vec3 dx{1.0f, 0.0f, 0.0f};
    const vsg::vec3 dy{0.0f, 1.0f, 0.0f};
    const vsg::vec3 dz{0.0f, 0.0f, 1.0f};
    const auto origin = offset - dx * 0.5f - dy * 0.5f - dz * 0.5f;

    auto [t_origin, t_scale, t_top] = vsg::vec3{0.0f, 1.0f, 1.0f}.value;

    vsg::vec3 v000(origin);
    vsg::vec3 v100(origin + dx);
    vsg::vec3 v110(origin + dx + dy);
    vsg::vec3 v010(origin + dy);
    vsg::vec3 v001(origin + dz);
    vsg::vec3 v101(origin + dx + dz);
    vsg::vec3 v111(origin + dx + dy + dz);
    vsg::vec3 v011(origin + dy + dz);

    vsg::vec2 t00(0.0f, t_top);
    vsg::vec2 t01(0.0f, t_origin);
    vsg::vec2 t10(1.0f, t_top);
    vsg::vec2 t11(1.0f, t_origin);

    vsg::ref_ptr<vsg::vec3Array> vertices;
    vsg::ref_ptr<vsg::vec3Array> normals;
    vsg::ref_ptr<vsg::vec2Array> texcoords;
    vsg::ref_ptr<vsg::ushortArray> indices;

    vsg::vec3 n0 = vsg::normalize(vsg::cross(dx, dz));
    vsg::vec3 n1 = vsg::normalize(vsg::cross(dy, dz));
    vsg::vec3 n2 = -n0;
    vsg::vec3 n3 = -n1;
    vsg::vec3 n4 = vsg::normalize(vsg::cross(dy, dx));
    vsg::vec3 n5 = -n4;

    // set up vertex and index arrays
    vertices = vsg::vec3Array::create(
        {v000, v100, v101, v001,   // front
         v100, v110, v111, v101,   // right
         v110, v010, v011, v111,   // far
         v010, v000, v001, v011,   // left
         v010, v110, v100, v000,   // bottom
         v001, v101, v111, v011}); // top

    normals = vsg::vec3Array::create(
        {n0, n0, n0, n0,
         n1, n1, n1, n1,
         n2, n2, n2, n2,
         n3, n3, n3, n3,
         n4, n4, n4, n4,
         n5, n5, n5, n5});

    texcoords = vsg::vec2Array::create(
        {t00, t10, t11, t01,
         t00, t10, t11, t01,
         t00, t10, t11, t01,
         t00, t10, t11, t01,
         t00, t10, t11, t01,
         t00, t10, t11, t01});

    indices = vsg::ushortArray::create(
        {0, 1, 2, 0, 2, 3,
         4, 5, 6, 4, 6, 7,
         8, 9, 10, 8, 10, 11,
         12, 13, 14, 12, 14, 15,
         16, 17, 18, 16, 18, 19,
         20, 21, 22, 20, 22, 23});

    auto vid = vsg::VertexIndexDraw::create();

    vsg::DataList arrays;
    arrays.push_back(vertices);
    if (normals) arrays.push_back(normals);
    if (texcoords) arrays.push_back(texcoords);
    if (colors) arrays.push_back(colors);
    if (positions) arrays.push_back(positions);
    vid->assignArrays(arrays);

    vid->assignIndices(indices);
    vid->indexCount = static_cast<uint32_t>(indices->size());
    vid->instanceCount = 1;

    auto box = vsg::StateGroup::create();
    box->add(builder.getOrCreateMaterialBinding([&texture](auto& descriptorConfigurator, auto& options) {
        auto material = vsg::PhongMaterialValue::create();
        material->value().alphaMaskCutoff = 0.95f;

        descriptorConfigurator.assignDescriptor("material", material);
        descriptorConfigurator.assignDescriptor("texCoordIndices", vsg::TexCoordIndicesValue::create());

        auto sampler = vsg::Sampler::create();
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        if (options.valid() && options->sharedObjects.valid())
        {
            options->sharedObjects->share(sampler);
        }

        descriptorConfigurator.assignTexture("diffuseMap", texture, sampler);
    }));

    box->addChild(vid);
    return box;
}

vsg::ref_ptr<vsg::Group> createScene(vsg::oit::depthpeeling::Builder& builder, vsg::ref_ptr<vsg::Data> texture, bool largeScene)
{
    auto scene = vsg::Group::create();

    static const std::array<vsg::vec4, 5> colors = {{
        vsg::vec4(1.0, 1.0, 0.0, 1.0),
        vsg::vec4(0.0, 1.0, 1.0, 1.0),
        vsg::vec4(1.0, 0.0, 0.0, 0.5),
        vsg::vec4(0.0, 1.0, 0.0, 0.5),
        vsg::vec4(0.0, 0.0, 1.0, 0.5)}};

    if (largeScene)
    {
        auto object = 0u;
        for (auto z = -3; z <= 3; ++z)
        {
            for (auto x = -3; x <= 3; ++x)
            {
                for (auto y = -3; y <= 3; ++y, ++object)
                {
                    scene->addChild(createBox(
                        builder, texture, colors[object % colors.size()], vsg::vec3(static_cast<float>(x) * 1.5f, static_cast<float>(y) * 1.5f, static_cast<float>(z) * 1.5f)));
                }
            }
        }
    }
    else
    {
        scene->addChild(createBox(builder, texture, colors[0], vsg::vec3(1.25, 0.0, 0.0)));
        scene->addChild(createBox(builder, texture, colors[1], vsg::vec3(0.0, -1.25, 0.0)));
        scene->addChild(createBox(builder, texture, colors[2], vsg::vec3(-1.25, 0.0, 0.0)));
        scene->addChild(createBox(builder, texture, colors[3], vsg::vec3(0.0, 0.0, 0.0)));
        scene->addChild(createBox(builder, texture, colors[4], vsg::vec3(0.0, 1.25, 0.0)));
    }

    return scene;
}

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // create windowTraits using the any command line arugments to configure settings
        auto windowTraits = vsg::WindowTraits::create(arguments);

        // NOTE: 
        // depth peeling requires the use of input attachments to read back the depth and color information from previous peels, 
        // so we need to ensure that the swapchain image usage and depth image usage include VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
        // and we also need to ensure that the depth image usage includes VK_IMAGE_USAGE_TRANSFER_SRC_BIT so that we can use
        // the depth image in transparency passes to reject fragments that are behind the nearest opaque fragments.
        windowTraits->swapchainPreferences.imageUsage |=
            (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
        windowTraits->depthImageUsage |= 
            (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

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

        if (arguments.read({ "-s", "--sampled" }))
        {
            windowTraits->samples = VK_SAMPLE_COUNT_8_BIT;
        }

        bool reportAverageFrameRate = arguments.read("--fps");
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

        auto largeScene = arguments.read({"--ls", "--large-scene"});

        bool multiThreading = arguments.read("--mt");
        auto maxTime = arguments.value(std::numeric_limits<double>::max(), "--max-time");

        if (arguments.read("--ThreadLogger")) vsg::Logger::instance() = vsg::ThreadLogger::create();
        if (int log_level = 0; arguments.read("--log-level", log_level)) vsg::Logger::instance()->level = vsg::Logger::Level(log_level);
        auto numFrames = arguments.value(-1, "-f");

        if (int log_level = 0; arguments.read("--log-level", log_level)) vsg::Logger::instance()->level = vsg::Logger::Level(log_level);
        auto logFilename = arguments.value<vsg::Path>("", "--log");

        auto nearFarRatio = arguments.value<double>(0.001, "--nfr");

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

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        // read first texture file specified on command line
        vsg::ref_ptr<vsg::ubvec4Array2D> texture;
        for (int i = 1; i < argc; ++i)
        {
            auto object = vsg::read(arguments[i], options);
            if (texture = object.cast<vsg::ubvec4Array2D>(); texture && texture->available())
            {
                break;
            }

            texture.reset();
        }

        if (!texture)
        {
            texture = vsg::read("textures/wood.png", options).cast<vsg::ubvec4Array2D>();
        }

        if (!texture || texture->empty())
        {
            std::cout << "Please specify a texture file on the command line or ensure the default texture 'textures/wood.png' is available." << std::endl;
            return 1;
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

        // create depth peeling builder
        auto builder = vsg::oit::depthpeeling::Builder::create(
            vsg::oit::depthpeeling::Builder::Settings::create(window, options));

        auto scene = createScene(*builder, texture, largeScene);

        vsg::ref_ptr<vsg::LookAt> lookAt;
        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;

        {
            // compute the bounds of the scene graph to help position camera
            vsg::ComputeBounds computeBounds;
            scene->accept(computeBounds);

            vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
            double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

            // set up the camera
            lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
            perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.5);
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // assign the camera and scenes to the builder - NOTE: the same scene can be used for both the opaque and transparency
        builder->setCamera(camera);
        builder->setScene(vsg::oit::depthpeeling::Builder::Pass::Opaque, scene);
        builder->setScene(vsg::oit::depthpeeling::Builder::Pass::Transparency, scene);

        // add close handler to respond to the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));
        // add trackball handler to respond to mouse events for interactive camera control
        viewer->addEventHandler(vsg::Trackball::create(camera));

        // create the needed render graphs used for depth peeling 
    	auto renderGraphs = builder->createRenderGraphs();

        // create command graph and assign the render graphs to it
        auto commandGraph = vsg::CommandGraph::create(window);
        commandGraph->addChild(renderGraphs.opaque);
        commandGraph->addChild(renderGraphs.transparency);

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

        viewer->compile();
        viewer->start_point() = vsg::clock::now();

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0) && (viewer->getFrameStamp()->simulationTime < maxTime))
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->update();

            viewer->recordAndSubmit();

            viewer->present();
        }

        if (reportAverageFrameRate)
        {
            auto fs = viewer->getFrameStamp();
            double fps = static_cast<double>(fs->frameCount) / std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - viewer->start_point()).count();
            std::cout << "Average frame rate = " << fps << " fps" << std::endl;
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
