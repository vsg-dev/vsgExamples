/* <editor-fold desc="MIT License">

Copyright(c) 2020 Julien Valentin & Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>

vsg::ref_ptr<vsg::Node> createScene(vsg::ref_ptr<const vsg::Options> options)
{
    // load shaders
    vsg::ref_ptr<vsg::ShaderStage> vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", options));
    vsg::ref_ptr<vsg::ShaderStage> fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", options));
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return {};
    }

    // read texture image
    vsg::Path textureFile("textures/lz.vsgb");
    auto textureData = vsg::read_cast<vsg::Data>(textureFile, options);
    if (!textureData)
    {
        std::cout << "Could not read texture file : " << textureFile << std::endl;
        return {};
    }

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorType, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection, view, and model matrices, actual push constant calls automatically provided by the VSG's RecordTraversal
    };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // tex coord data
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
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    // create texture image and associated DescriptorSets and binding
    auto texture = vsg::DescriptorImage::create(vsg::Sampler::create(), textureData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texture});
    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->layout, 0, vsg::DescriptorSets{descriptorSet});

    // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSets);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT

    // add transform to root of the scene graph
    scenegraph->addChild(transform);

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
        {{-0.5f, -0.5f, 0.0f},
         {0.5f, -0.5f, 0.05f},
         {0.5f, 0.5f, 0.0f},
         {-0.5f, 0.5f, 0.0f},
         {-0.5f, -0.5f, -0.5f},
         {0.5f, -0.5f, -0.5f},
         {0.5f, 0.5f, -0.5},
         {-0.5f, 0.5f, -0.5}}); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec3Array::create(
        {
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
        }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto texcoords = vsg::vec2Array::create(
        {{0.0f, 0.0f},
         {1.0f, 0.0f},
         {1.0f, 1.0f},
         {0.0f, 1.0f},
         {0.0f, 0.0f},
         {1.0f, 0.0f},
         {1.0f, 1.0f},
         {0.0f, 1.0f}}); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
         2, 3, 0,
         4, 5, 6,
         6, 7, 4}); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, colors, texcoords}));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(12, 1, 0, 0, 0));

    // add drawCommands to transform
    transform->addChild(drawCommands);

    return scenegraph;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto traits = vsg::WindowTraits::create();
    traits->windowTitle = "vsgexecutecommands window1";
    traits->width = 800;
    traits->height = 600;

    vsg::CommandLine arguments(&argc, argv);

    // set up vsg::Options to pass in filepaths, ReaderWriters and other IO related options to use when reading and writing files.
    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    traits->debugLayer = arguments.read({"--debug", "-d"});
    traits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read({"--window", "-w"}, traits->width, traits->height)) { traits->fullscreen = false; }
    if (arguments.read({"-t", "--test"})) { traits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; }
    bool multiThreading = arguments.read("--mt");
    bool useExecuteCommands = !arguments.read("--no-ec"); // by default use ExecuteCommands, but allow it to be disabled using --no-ec
    auto numFrames = arguments.value(-1, "-f");
    auto pathFilename = arguments.value<vsg::Path>("", "-p");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    vsg::Path filename;
    if (argc > 1) filename = arguments[1];

    vsg::ref_ptr<vsg::Node> vsg_scene;
    if (filename)
    {
        vsg_scene = vsg::read_cast<vsg::Node>(filename, options);
    }
    else
    {
        vsg_scene = createScene(options);
    }

    if (!vsg_scene)
    {
        std::cout << "Unable to load model." << std::endl;
        return 1;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    auto window1 = vsg::Window::create(traits);
    if (!window1)
    {
        std::cout << "Could not create window1." << std::endl;
        return 1;
    }

    auto traits2 = vsg::WindowTraits::create(*traits);
    traits2->windowTitle = "vsgexecutecommands window2";
    traits2->device = window1->getOrCreateDevice();

    auto window2 = vsg::Window::create(traits2);
    if (!window2)
    {
        std::cout << "Could not create window2." << std::endl;
        return 1;
    }

    viewer->addWindow(window1);
    viewer->addWindow(window2);

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // create camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    double aspectRatio = traits->fullscreen ? (1920.0 / 1080.0) : static_cast<double>(traits->width) / static_cast<double>(traits->height);
    if (auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel"))
    {
        double horizonMountainHeight = 0.0;
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, aspectRatio, nearFarRatio, horizonMountainHeight);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, aspectRatio, nearFarRatio * radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(0, 0, traits->width, traits->height));

    if (useExecuteCommands)
    {
        std::cout << "Using SecondaryCommandGraph and ExecuteCommands" << std::endl;
        auto secondaryCommandGraph = vsg::createSecondaryCommandGraphForView(window1, camera, vsg_scene, 0);

        auto scenegraphwin1 = vsg::Group::create();
        auto pass1 = vsg::ExecuteCommands::create();
        pass1->connect(secondaryCommandGraph);

        auto scenegraphwin2 = vsg::Group::create();
        auto pass2 = vsg::ExecuteCommands::create();
        pass2->connect(secondaryCommandGraph);

        scenegraphwin1->addChild(pass1);
        scenegraphwin2->addChild(pass2);

        auto commandGraphwin1 = vsg::createCommandGraphForView(window1, camera, scenegraphwin1, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        auto commandGraphwin2 = vsg::createCommandGraphForView(window2, camera, scenegraphwin2, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

        viewer->assignRecordAndSubmitTaskAndPresentation({secondaryCommandGraph, commandGraphwin1, commandGraphwin2});
    }
    else
    {
        std::cout << "Using Windows with separate full CommandGraph." << std::endl;
        auto commandGraphwin1 = vsg::createCommandGraphForView(window1, camera, vsg_scene);
        auto commandGraphwin2 = vsg::createCommandGraphForView(window2, camera, vsg_scene);

        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraphwin1, commandGraphwin2});
    }

    if (multiThreading) viewer->setupThreading();

    // compile the Vulkan objects
    viewer->compile();

    // assign a CloseHandler to the Viewer to respond to pressing Escape or the window close button
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    if (pathFilename)
    {
        auto cameraAnimation = vsg::CameraAnimationHandler::create(camera, pathFilename, options);
        viewer->addEventHandler(cameraAnimation);
        if (cameraAnimation->animation) cameraAnimation->play();
    }

    viewer->addEventHandler(vsg::Trackball::create(camera));

    // main frame loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
