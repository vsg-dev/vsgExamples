#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "vsgmeshshader";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
        arguments.read("--screen", windowTraits->screenNum);
        auto type = arguments.value<int>(0, {"--type", "-t"});
        auto outputFilename = arguments.value<vsg::Path>("", "-o");

        auto numFrames = arguments.value(-1, "-f");
        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        // set up search paths to SPIRV shaders and textures
        auto options = vsg::Options::create();
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
#ifdef vsgXchange_all
        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());
#endif

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        windowTraits->deviceExtensionNames = {
            VK_NV_MESH_SHADER_EXTENSION_NAME,
            VK_NV_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME};

        // set up features
        auto features = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();

        auto& meshFeatures = features->get<VkPhysicalDeviceMeshShaderFeaturesNV, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV>();
        meshFeatures.meshShader = 1;
        meshFeatures.taskShader = 1;

        auto& barycentricFeatures = features->get<VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV>();
        barycentricFeatures.fragmentShaderBarycentric = 1;

        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }

        viewer->addWindow(window);

        auto mesh_features = window->getOrCreatePhysicalDevice()->getFeatures<VkPhysicalDeviceMeshShaderFeaturesNV, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV>();
        std::cout << "nv_features.meshShader = " << mesh_features.meshShader << std::endl;
        std::cout << "nv_features.taskShader = " << mesh_features.taskShader << std::endl;

        if (!mesh_features.meshShader || !mesh_features.taskShader)
        {
            std::cout << "Mesh shaders not supported." << std::endl;
            return 1;
        }

        auto barycentric_features = window->getOrCreatePhysicalDevice()->getFeatures<VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV>();
        std::cout << "barycentric_features.fragmentShaderBarycentric = " << barycentric_features.fragmentShaderBarycentric << std::endl;
        if (!barycentric_features.fragmentShaderBarycentric)
        {
            std::cout << "fragmentShaderBarycentric not supported." << std::endl;
            return 1;
        }

        auto mesh_properites = window->getOrCreatePhysicalDevice()->getProperties<VkPhysicalDeviceMeshShaderPropertiesNV, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV>();
        std::cout << "mesh_properites.maxDrawMeshTasksCount = " << mesh_properites.maxDrawMeshTasksCount << std::endl;
        std::cout << "mesh_properites.maxTaskTotalMemorySize = " << mesh_properites.maxTaskTotalMemorySize << std::endl;
        std::cout << "mesh_properites.maxMeshTotalMemorySize = " << mesh_properites.maxMeshTotalMemorySize << std::endl;
        std::cout << "mesh_properites.maxMeshOutputVertices = " << mesh_properites.maxMeshOutputVertices << std::endl;
        std::cout << "mesh_properites.maxMeshOutputPrimitives = " << mesh_properites.maxMeshOutputPrimitives << std::endl;

        // load shaders
        auto meshShader = vsg::read_cast<vsg::ShaderStage>("shaders/meshshader.mesh", options);
        auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/meshshader.frag", options);

        if (!meshShader || !fragmentShader)
        {
            std::cout << "Could not create shaders." << std::endl;
            return 1;
        }

        auto shaderStages = vsg::ShaderStages{meshShader, fragmentShader};

        // set up graphics pipeline
        vsg::DescriptorSetLayoutBindings descriptorBindings{};
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        vsg::GraphicsPipelineStates pipelineStates{
            vsg::InputAssemblyState::create(),
            vsg::RasterizationState::create(),
            vsg::MultisampleState::create(),
            vsg::ColorBlendState::create(),
            vsg::DepthStencilState::create()};

        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{});
        auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, shaderStages, pipelineStates);
        auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

        // state group to bind the pipeline and descriptor set
        auto scenegraph = vsg::Commands::create();
        scenegraph->addChild(bindGraphicsPipeline);

        if (type <= 1)
        {
            std::cout << "Using vsg::DrawMeshTasks" << std::endl;
            scenegraph->addChild(vsg::DrawMeshTasks::create(1, 0));
        }
        else if (type <= 2)
        {
            std::cout << "Using vsg::DrawMeshTasksIndirect" << std::endl;
            auto data = vsg::DrawMeshTasksIndirectCommandArray::create(1);
            data->at(0).taskCount = 1;
            data->at(0).firstTask = 0;
            scenegraph->addChild(vsg::DrawMeshTasksIndirect::create(data, 1, 8));
        }
        else
        {
            std::cout << "Using vsg::DrawMeshTasksIndirectCount" << std::endl;
            auto data = vsg::DrawMeshTasksIndirectCommandArray::create(1);
            data->at(0).taskCount = 1;
            data->at(0).firstTask = 0;
            auto count = vsg::uintArray::create(1);
            count->at(0) = 1;
            scenegraph->addChild(vsg::DrawMeshTasksIndirectCount::create(data, count, 1, 8));
        }

        if (!outputFilename.empty())
        {
            vsg::write(scenegraph, outputFilename);
            return 1;
        }

        // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
        viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

        // set up commandGraph for rendering
        auto commandGraph = vsg::createCommandGraphForView(window, {}, scenegraph);
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
    catch (const vsg::Exception& exception)
    {
        std::cout << exception.message << " VkResult = " << exception.result << std::endl;
        return 0;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
