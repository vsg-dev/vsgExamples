#include <iostream>
#include <vsg/all.h>


class DrawMeshTasks : public vsg::Inherit<vsg::Command, DrawMeshTasks>
{
public:
    DrawMeshTasks() {}

    DrawMeshTasks(uint32_t in_first, uint32_t in_count) :
        first(in_first),
        count(in_count)
    {
    }

    uint32_t first = 0;
    uint32_t count = 0;

    PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNV = nullptr;

    void compile(vsg::Context& context) override
    {
        vkCmdDrawMeshTasksNV = reinterpret_cast<PFN_vkCmdDrawMeshTasksNV>(vkGetDeviceProcAddr(*context.device, "vkCmdDrawMeshTasksNV"));
        std::cout<<"this->vkCmdDrawMeshTasksNV = "<<this->vkCmdDrawMeshTasksNV<<std::endl;
    }

    void record(vsg::CommandBuffer& commandBuffer) const override
    {
        // assume that point is valid.
        vkCmdDrawMeshTasksNV(commandBuffer, first, count);
    }
};


int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);
        auto debugLayer = arguments.read({"--debug", "-d"});
        auto apiDumpLayer = arguments.read({"--api", "-a"});
        auto [width, height] = arguments.value(std::pair<uint32_t, uint32_t>(1280, 720), {"--window", "-w"});
        auto numFrames = arguments.value(-1, "-f");
        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        // set up search paths to SPIRV shaders and textures
        vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "vsgmeshshader";
        windowTraits->debugLayer = debugLayer;
        windowTraits->apiDumpLayer = apiDumpLayer;
        windowTraits->width = width;
        windowTraits->height = height;
        windowTraits->queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

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

        // load shaders
        vsg::ref_ptr<vsg::ShaderStage> meshShader = vsg::ShaderStage::read(VK_SHADER_STAGE_MESH_BIT_NV, "main", vsg::findFile("shaders/meshshader.mesh", searchPaths));
        vsg::ref_ptr<vsg::ShaderStage> fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/meshshader.frag", searchPaths));

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
        scenegraph->addChild(DrawMeshTasks::create(0, 1));


        // camera related details
        auto viewport = vsg::ViewportState::create(0, 0, width, height);

        // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
        viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

        // set up commandGraph to rendering viewport
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
