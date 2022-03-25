#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include "ShaderSet.h"

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->objectCache = vsg::ObjectCache::create();

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);

    auto textureFile = arguments.value<vsg::Path>("", "-t");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // load shaders
    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/assimp.vert", options);
    //auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/assimp_flat_shaded.frag", options);
    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/assimp_phong.frag", options);
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return 1;
    }

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_TexCoord0", "", 2, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Array::create(1));
    shaderSet->addAttributeBinding("vsg_Color", "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));
    shaderSet->addAttributeBinding("vsg_position", "VSG_INSTANCE_POSITIONS", 3, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

    shaderSet->addUniformBinding("displacementMap", "VSG_DISPLACEMENT_MAP", 0, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::vec4Array2D::create(1,1));
    shaderSet->addUniformBinding("diffuseMap", "VSG_DIFFUSE_MAP", 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1,1));
    shaderSet->addUniformBinding("normalMap", "VSG_NORMAL_MAP", 0, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec3Array2D::create(1,1));
    shaderSet->addUniformBinding("aoMap", "VSG_LIGHTMAP_MAP", 0, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1,1));
    shaderSet->addUniformBinding("emissiveMap", "VSG_EMISSIVE_MAP", 0, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1,1));
    shaderSet->addUniformBinding("material", "", 0, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PhongMaterialValue::create());
    shaderSet->addUniformBinding("lightData", "VSG_VIEW_LIGHT_DATA", 1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));


    // set up shader hints
    auto shaderHints = vsg::ShaderCompileSettings::create();
    auto& defines = shaderHints->defines;

    //defines.push_back("VSG_VIEW_LIGHT_DATA");

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings;
    vsg::Descriptors descriptors;

    // read texture image
    if (!textureFile.empty())
    {
        auto textureData = vsg::read_cast<vsg::Data>(textureFile, options);
        if (!textureData)
        {
            std::cout << "Could not read texture file : " << textureFile << std::endl;
            return 1;
        }

        // enable texturing
        if (auto& textureBinding = shaderSet->getUniformBinding("diffuseMap"))
        {
            descriptorBindings.push_back(VkDescriptorSetLayoutBinding{textureBinding.binding, textureBinding.descriptorType, textureBinding.descriptorCount, textureBinding.stageFlags, nullptr});
            if (!textureBinding.define.empty()) defines.push_back(textureBinding.define);

            // create texture image and associated DescriptorSets and binding
            auto texture = vsg::DescriptorImage::create(vsg::Sampler::create(), textureData, textureBinding.binding, 0, textureBinding.descriptorType);
            descriptors.push_back(texture);
        }
    }

    // set up pass of material
    if (auto& materialBinding = shaderSet->getUniformBinding("material"))
    {
        descriptorBindings.push_back(VkDescriptorSetLayoutBinding{materialBinding.binding, materialBinding.descriptorType, materialBinding.descriptorCount, materialBinding.stageFlags, nullptr});
        if (!materialBinding.define.empty()) defines.push_back(materialBinding.define);

#if 0
        // use the default maerialBinding.data
        auto material = vsg::DescriptorBuffer::create(materialBinding.data, materialBinding.binding);
        descriptors.push_back(material);
#else
        // create texture image and associated DescriptorSets and binding
        auto mat = vsg::PhongMaterialValue::create();
        mat->value().diffuse.set(1.0f, 1.0f, 0.0f, 1.0f);
        mat->value().specular.set(1.0f, 0.0f, 0.0f, 1.0f);

        auto material = vsg::DescriptorBuffer::create(mat, materialBinding.binding);
        descriptors.push_back(material);
#endif
    }

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls automatically provided by the VSG's DispatchTraversal
    };

    vsg::VertexInputState::Bindings vertexBindingDescriptions;
    vsg::VertexInputState::Attributes vertexAttributeDescriptions;
    uint32_t attributeBinding = 0;
    if (auto& vertexBinding = shaderSet->getAttributeBinding("vsg_Vertex"))
    {
        vertexBindingDescriptions.push_back(VkVertexInputBindingDescription{attributeBinding, vertexBinding.data->getLayout().stride, VK_VERTEX_INPUT_RATE_VERTEX}); // should use actual Array stride
        vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{vertexBinding.location, attributeBinding, vertexBinding.format, 0}); // 0 shhould be the offset for interlaved arrays
        ++attributeBinding;
    }

    if (auto& normalBinding = shaderSet->getAttributeBinding("vsg_Normal"))
    {
        vertexBindingDescriptions.push_back(VkVertexInputBindingDescription{attributeBinding, normalBinding.data->getLayout().stride, VK_VERTEX_INPUT_RATE_VERTEX}); // should use actual Array stride
        vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{normalBinding.location, attributeBinding, normalBinding.format, 0}); // 0 shhould be the offset for interlaved arrays
        ++attributeBinding;
    }

    if (auto& texCoordBinding = shaderSet->getAttributeBinding("vsg_TexCoord0"))
    {
        vertexBindingDescriptions.push_back(VkVertexInputBindingDescription{attributeBinding, texCoordBinding.data->getLayout().stride, VK_VERTEX_INPUT_RATE_VERTEX}); // should use actual Array stride
        vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{texCoordBinding.location, attributeBinding, texCoordBinding.format, 0}); // 0 shhould be the offset for interlaved arrays
        ++attributeBinding;
    }

    if (auto& colorBinding = shaderSet->getAttributeBinding("vsg_Color"))
    {
        vertexBindingDescriptions.push_back(VkVertexInputBindingDescription{attributeBinding, colorBinding.data->getLayout().stride, VK_VERTEX_INPUT_RATE_VERTEX}); // should use actual Array stride
        vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{colorBinding.location, attributeBinding, colorBinding.format, 0}); // 0 shhould be the offset for interlaved arrays
        ++attributeBinding;
    }

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(vertexBindingDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::ColorBlendState::create(),
        vsg::MultisampleState::create(),
        vsg::DepthStencilState::create()};


    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);

    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, shaderSet->getShaderStages(shaderHints), pipelineStates);

    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);



    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, descriptors);
    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, descriptorSet);

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSet);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT

    // add transform to root of the scene graph
    scenegraph->addChild(transform);

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
        {{-0.5f, -0.5f, 0.0f},
         {0.5f, -0.5f, 0.0f},
         {0.5f, 0.5f, 0.0f},
         {-0.5f, 0.5f, 0.0f},
         {-0.5f, -0.5f, -0.5f},
         {0.5f, -0.5f, -0.5f},
         {0.5f, 0.5f, -0.5f},
         {-0.5f, 0.5f, -0.5f}}); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto normals = vsg::vec3Array::create(
        {{0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f}}); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto texcoords = vsg::vec2Array::create(
        {{0.0f, 0.0f},
         {1.0f, 0.0f},
         {1.0f, 1.0f},
         {0.0f, 1.0f},
         {0.0f, 0.0f},
         {1.0f, 0.0f},
         {1.0f, 1.0f},
         {0.0f, 1.0f}}); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec4Array::create(
        {{1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
        }); // VK_FORMAT_R32G32B32A32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
         2, 3, 0,
         4, 5, 6,
         6, 7, 4}); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, normals, texcoords, colors}));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(12, 1, 0, 0, 0));

    // add drawCommands to transform
    transform->addChild(drawCommands);

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // camera related details
    auto viewport = vsg::ViewportState::create(0, 0, window->extent2D().width, window->extent2D().height);
    auto perspective = vsg::Perspective::create(60.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), 0.1, 10.0);
    auto lookAt = vsg::LookAt::create(vsg::dvec3(1.0, 1.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scenegraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // compile the Vulkan objects
    viewer->compile();

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    viewer->addEventHandler(vsg::Trackball::create(camera));

    // main frame loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        // animate the transform
        float time = std::chrono::duration<float, std::chrono::seconds::period>(viewer->getFrameStamp()->time - viewer->start_point()).count();
        transform->matrix = vsg::rotate(time * vsg::radians(90.0f), vsg::vec3(0.0f, 0.0, 1.0f));

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
