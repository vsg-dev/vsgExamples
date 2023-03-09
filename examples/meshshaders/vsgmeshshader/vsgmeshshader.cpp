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
        bool barycentric = arguments.read({"--barycentric", "--bc"});

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

        windowTraits->vulkanVersion = VK_API_VERSION_1_1;
        windowTraits->deviceExtensionNames = { VK_EXT_MESH_SHADER_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME };
        if(barycentric)
            windowTraits->deviceExtensionNames.push_back(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);

        // set up features
        auto features = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();

        auto& meshFeatures = features->get<VkPhysicalDeviceMeshShaderFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT>();
        meshFeatures.meshShader = 1;
        meshFeatures.taskShader = 1;

        if(barycentric)
        {
            auto& barycentricFeatures = features->get<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR>();
            barycentricFeatures.fragmentShaderBarycentric = 1;
        }

        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }

        viewer->addWindow(window);

        auto mesh_features = window->getOrCreatePhysicalDevice()->getFeatures<VkPhysicalDeviceMeshShaderFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT>();
        std::cout << "mesh_features.meshShader = " << mesh_features.meshShader << std::endl;
        std::cout << "mesh_features.taskShader = " << mesh_features.taskShader << std::endl;

        if (!mesh_features.meshShader || !mesh_features.taskShader)
        {
            std::cout << "Mesh shaders not supported." << std::endl;
            return 1;
        }

        auto barycentric_features = window->getOrCreatePhysicalDevice()->getFeatures<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR>();
        std::cout << "barycentric_features.fragmentShaderBarycentric = " << barycentric_features.fragmentShaderBarycentric << std::endl;

        auto mesh_properites = window->getOrCreatePhysicalDevice()->getProperties<VkPhysicalDeviceMeshShaderPropertiesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT>();
        std::cout << "mesh_properites.maxTaskWorkGroupTotalCount = " << mesh_properites.maxTaskWorkGroupTotalCount << std::endl;
        std::cout << "mesh_properites.maxMeshSharedMemorySize = " << mesh_properites.maxMeshSharedMemorySize << std::endl;
        std::cout << "mesh_properites.maxMeshPayloadAndSharedMemorySize = " << mesh_properites.maxMeshPayloadAndSharedMemorySize << std::endl;
        std::cout << "mesh_properites.maxMeshOutputMemorySize = " << mesh_properites.maxMeshOutputMemorySize << std::endl;
        std::cout << "mesh_properites.maxMeshOutputVertices = " << mesh_properites.maxMeshOutputVertices << std::endl;
        std::cout << "mesh_properites.maxMeshOutputPrimitives = " << mesh_properites.maxMeshOutputPrimitives << std::endl;

        auto meshShaderPath = "shaders/meshshader.mesh";
        auto fragShaderPath = barycentric ? "shaders/barycentric.frag" : "shaders/meshshader.frag";
        // load shaders

        auto meshShader = vsg::read_cast<vsg::ShaderStage>(meshShaderPath, options);
        auto fragmentShader = vsg::read_cast<vsg::ShaderStage>(fragShaderPath, options);

        if (!meshShader || !fragmentShader)
        {
            std::cout << "Could not create shaders." << std::endl;
            return 1;
        }

        auto shaderStages = vsg::ShaderStages{meshShader, fragmentShader};

        vsg::PushConstantRanges pushConstantRanges{
            {VK_SHADER_STAGE_MESH_BIT_EXT, 0, 128} // projection view, and model matrices, actual push constant calls automatically provided by the VSG's DispatchTraversal
        };

        // set up graphics pipeline
        vsg::DescriptorSetLayoutBindings descriptorBindings{};
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        vsg::GraphicsPipelineStates pipelineStates{
            vsg::InputAssemblyState::create(),
            vsg::RasterizationState::create(),
            vsg::MultisampleState::create(),
            vsg::ColorBlendState::create(),
            vsg::DepthStencilState::create()};

        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
        auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, shaderStages, pipelineStates);
        auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

        auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{});
        auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, descriptorSet);

        // state group to bind the pipeline and descriptor set
        auto scenegraph = vsg::StateGroup::create();
        scenegraph->add(bindGraphicsPipeline);
        scenegraph->add(bindDescriptorSet);

        if (type <= 1)
        {
            std::cout << "Using vsg::DrawMeshTasks" << std::endl;
            scenegraph->addChild(vsg::DrawMeshTasks::create(2, 1, 1));
        }
        else if (type <= 2)
        {
            std::cout << "Using vsg::DrawMeshTasksIndirect" << std::endl;
            auto data = vsg::uivec3Value::create(3, 1, 1); // groupCountX = 3, groupCountY = 1, groupCountZ = 1
            scenegraph->addChild(vsg::DrawMeshTasksIndirect::create(data, 1, 12));
        }
        else
        {
            std::cout << "Using vsg::DrawMeshTasksIndirectCount" << std::endl;
            auto data = vsg::uivec3Value::create(5, 1, 1); // groupCountX = 5, groupCountY = 1, groupCountZ = 1
            auto count = vsg::uintValue::create(1);

            scenegraph->addChild(vsg::DrawMeshTasksIndirectCount::create(data, count, 1, 12));
        }

        if (outputFilename)
        {
            vsg::write(scenegraph, outputFilename);
            return 1;
        }


        auto perspective = vsg::Perspective::create(60.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), 0.001, 10.0);
        auto lookAt = vsg::LookAt::create(vsg::dvec3(0.0, 2.0, -5.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0));
        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
        viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});
        viewer->addEventHandler(vsg::Trackball::create(camera));

        // set up commandGraph for rendering
        auto commandGraph = vsg::createCommandGraphForView(window, camera, scenegraph);
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
