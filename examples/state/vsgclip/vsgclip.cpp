#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

// Example that uses gl_ClipDistancep[] to do GPU clipping of primitives
// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_ClipDistance.xhtml
// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_CullDistance.xhtml


class ReplaceState : public vsg::Inherit<vsg::Visitor, ReplaceState>
{
public:

    vsg::ref_ptr<vsg::GraphicsPipeline> graphicsPipeline;

    ReplaceState(vsg::ref_ptr<vsg::GraphicsPipeline> gp) :
        graphicsPipeline(gp) {}

    void apply(vsg::Object& object) override
    {
        object.traverse(*this);
    }

    void apply(vsg::StateGroup& sg) override
    {
        for(auto& sc : sg.getStateCommands()) sc->accept(*this);

        sg.traverse(*this);
    }

    void apply(vsg::BindGraphicsPipeline& bgp) override
    {
        bgp.pipeline = graphicsPipeline;
    }
};

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // set up vsg::Options to pass in filepaths and ReaderWriter's and other IO realted options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());
#endif

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "vsgmmeshshader";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
        arguments.read("--screen", windowTraits->screenNum);
        auto type = arguments.value<int>(0, {"--type", "-t"});
        auto outputFilename = arguments.value<vsg::Path>("", "-o");

        auto numFrames = arguments.value(-1, "-f");
        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        // set up features
        auto requestFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();

        requestFeatures->get().samplerAnisotropy = VK_TRUE;
        requestFeatures->get().shaderClipDistance = VK_TRUE;

        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }

        viewer->addWindow(window);

        // get the Window to create the Instance & PhysicalDevice for us.
        auto& availableFeatures = window->getOrCreatePhysicalDevice()->getFeatures(); // VkPhysicalDeviceFeatures
        auto& limits = window->getOrCreatePhysicalDevice()->getProperties().limits; // VkPhysicalDeviceLimits

        std::cout<<"availableFeatures.samplerAnisotropy = "<<availableFeatures.samplerAnisotropy<<", limits.maxSamplerAnisotropy = "<<limits.maxSamplerAnisotropy<<std::endl;
        std::cout<<"availableFeatures.shaderClipDistance = "<<availableFeatures.shaderClipDistance<<", limits.maxClipDistances = "<<limits.maxClipDistances<<std::endl;
        std::cout<<"availableFeatures.shaderCullDistance = "<<availableFeatures.shaderCullDistance<<", limits.maxCullDistances = "<<limits.maxCullDistances<<std::endl;
        std::cout<<"limits.maxCombinedClipAndCullDistances = "<<limits.maxCombinedClipAndCullDistances<<std::endl;

        if (!availableFeatures.samplerAnisotropy || !availableFeatures.shaderClipDistance)
        {
            std::cout<<"Required features not supported."<<std::endl;
            return 1;
        }

        // load shaders
        auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/clip.vert", options);
        auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/clip.frag", options);

        if (!vertexShader || !fragmentShader)
        {
            std::cout << "Could not create shaders." << std::endl;
            return 1;
        }

        auto shaderStages = vsg::ShaderStages{vertexShader, fragmentShader};

        // set up graphics pipeline
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
        };

        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

        vsg::PushConstantRanges pushConstantRanges{
            {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
        };

        vsg::VertexInputState::Bindings vertexBindingsDescriptions{
            VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
            VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
            VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
        };

        vsg::VertexInputState::Attributes vertexAttributeDescriptions{
            VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
            VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
            VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0}    // tex coord data
        };

        vsg::GraphicsPipelineStates pipelineStates{
            vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
            vsg::InputAssemblyState::create(),
            vsg::RasterizationState::create(),
            vsg::MultisampleState::create(),
            vsg::ColorBlendState::create(),
            vsg::DepthStencilState::create()};

        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
        auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);

        if (argc <= 1)
        {
            std::cout << "Please specify a 3d model on the command line." << std::endl;
            return 1;
        }

        vsg::Path filename = arguments[1];
        auto vsg_scene = vsg::read_cast<vsg::Node>(filename, options);
        if (!vsg_scene)
        {
            std::cout << "Unable to load file " << filename << std::endl;
            return 1;
        }

        // replace the GraphicsPipeline in the loaded scene graph with the one created for clipping.
        auto replaceState = ReplaceState::create(graphicsPipeline);
        vsg_scene->accept(*replaceState);

        if (!outputFilename.empty())
        {
            vsg::write(vsg_scene, outputFilename);
            return 1;
        }


        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
        double nearFarRatio = 0.001;

        // set up the camera
        auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel(vsg_scene->getObject<vsg::EllipsoidModel>("EllipsoidModel"));
        if (ellipsoidModel)
        {
            perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, 0.0);
        }
        else
        {
            perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        // add trackbal to control the Camera
        viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

        // set up commandGraph for rendering
        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
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
