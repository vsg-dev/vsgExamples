#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif


class DrawMeshTasks : public vsg::Inherit<vsg::Command, DrawMeshTasks>
{
public:
    DrawMeshTasks() {}

    DrawMeshTasks(uint32_t in_taskCount, uint32_t in_firstTask) :
        taskCount(in_taskCount),
        firstTask(in_firstTask)
    {
    }

    uint32_t taskCount = 0;
    uint32_t firstTask = 0;

    PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNV = nullptr;

    void compile(vsg::Context& context) override
    {
        vkCmdDrawMeshTasksNV = reinterpret_cast<PFN_vkCmdDrawMeshTasksNV>(vkGetDeviceProcAddr(*context.device, "vkCmdDrawMeshTasksNV"));
        std::cout<<"this->vkCmdDrawMeshTasksNV = "<<this->vkCmdDrawMeshTasksNV<<std::endl;
    }

    void record(vsg::CommandBuffer& commandBuffer) const override
    {
        // assume that point is valid.
        vkCmdDrawMeshTasksNV(commandBuffer, taskCount, firstTask);
    }
};


int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "vsgmmeshshader";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
        arguments.read("--screen", windowTraits->screenNum);

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

        windowTraits->queueFlags = VK_QUEUE_GRAPHICS_BIT;
        windowTraits->instanceExtensionNames = { VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME };

        windowTraits->deviceExtensionNames = {
                VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
                VK_NV_MESH_SHADER_EXTENSION_NAME
        };

        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }

        viewer->addWindow(window);

        auto nv_features = window->getOrCreatePhysicalDevice()->getFeatures<VkPhysicalDeviceMeshShaderFeaturesNV, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV>();
        std::cout<<"nv_features.meshShader = "<<nv_features.meshShader<<std::endl;
        std::cout<<"nv_features.taskShader = "<<nv_features.taskShader<<std::endl;

        if (!nv_features.meshShader || !nv_features.taskShader)
        {
            std::cout<<"Mesh shaders not supported."<<std::endl;
            return 1;
        }

        auto barycentric_features = window->getOrCreatePhysicalDevice()->getFeatures<VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV>();
        std::cout<<"barycentric_features.fragmentShaderBarycentric = "<<barycentric_features.fragmentShaderBarycentric<<std::endl;
        if (!barycentric_features.fragmentShaderBarycentric)
        {
            std::cout<<"fragmentShaderBarycentric not supported."<<std::endl;
            return 1;
        }


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

        // state group to bind the pipeline and descriptorset
        auto scenegraph = vsg::Commands::create();
        scenegraph->addChild(bindGraphicsPipeline);
        scenegraph->addChild(DrawMeshTasks::create(1, 0));


        // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
        viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

        // set up commandGraph for rendering
        auto commandGraph = vsg::CommandGraph::create(window);
        commandGraph->addChild(scenegraph);

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
