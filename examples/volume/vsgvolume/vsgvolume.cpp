#include <iostream>
#include <vsg/all.h>
#include <vsgXchange/all.h>

char volume_vert[] = R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 cameraPos;
layout(location = 1) out vec4 vertexPos;
layout(location = 2) out mat4 texgen;

out gl_PerVertex{
    vec4 gl_Position;
};

void main() {
    gl_Position = (pc.projection * pc.modelview) * vec4(inPosition, 1.0);
    cameraPos = inverse(pc.modelview) * vec4(0,0,0,1);
    vertexPos = vec4(inPosition, 1.0);
    texgen = mat4(1.0);
})";

char volume_frag[] = R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler3D volume;

layout(location = 0) in vec4 cameraPos;
layout(location = 1) in vec4 vertexPos;
layout(location = 2) in mat4 texgen;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 t0 = vertexPos;
    vec4 te = cameraPos;
    if( te.x>=0.0 && te.x<=1.0 &&
        te.y>=0.0 && te.y<=1.0 &&
        te.z>=0.0 && te.z<=1.0
        )
    { } else {
        if (te.x<0.0){
            float r = -te.x / (t0.x-te.x);
            te = te + (t0-te)*r;
        }
        if (te.x>1.0){
            float r = (1.0-te.x) / (t0.x-te.x);
            te = te + (t0-te)*r;
        }
        if (te.y<0.0){
            float r = -te.y / (t0.y-te.y);
            te = te + (t0-te)*r;
        }
        if (te.y>1.0){
            float r = (1.0-te.y) / (t0.y-te.y);
            te = te + (t0-te)*r;
        }
        if (te.z<0.0){
            float r = -te.z / (t0.z-te.z);
            te = te + (t0-te)*r;
        }
        if (te.z>1.0){
            float r = (1.0-te.z) / (t0.z-te.z);
            te = te + (t0-te)*r;
        }
    }
    t0 = t0 * texgen;
    te = te * texgen;

    const float min_iteratrions = 2.0;
    const float max_iteratrions = 2048.0;

    float TransparencyValue = 0.2;
    float AlphaFuncValue = 0.1;
    float SampleDensityValue = 0.005; // 0.5 / texture_sample_count

    float num_iterations = ceil(length((te-t0).xyz)/SampleDensityValue);
    if (num_iterations<min_iteratrions) num_iterations = min_iteratrions;
    else if (num_iterations>max_iteratrions) num_iterations = max_iteratrions;

    vec3 deltaTexCoord=(te-t0).xyz/(num_iterations-1.0);
    vec3 texcoord = t0.xyz;

    vec4 fragColor = vec4(0.0, 0.0, 0.0, 0.0);
    while(num_iterations>0.0)
    {
        float alpha = texture(volume, texcoord).r;
        vec4 color = vec4(alpha, alpha, alpha, alpha * TransparencyValue);
        float r = color.a;
        if (r > AlphaFuncValue)
        {
            fragColor.rgb = mix(fragColor.rgb, color.rgb, r);
            fragColor.a += r;
        }

        if (color.a > fragColor.a)
        {
            fragColor = color;
        }

        texcoord += deltaTexCoord;
        --num_iterations;
    }
    if (fragColor.a>1.0) fragColor.a = 1.0;
    if (fragColor.a<AlphaFuncValue) discard;
    outColor = fragColor;
})";

void updateBaseTexture3D(vsg::floatArray3D& image, float value)
{
    for (size_t d = 0; d < image.depth(); ++d)
    {
        float d_ratio = static_cast<float>(d) / static_cast<float>(image.depth() - 1);
        for (size_t r = 0; r < image.height(); ++r)
        {
            float r_ratio = static_cast<float>(r) / static_cast<float>(image.height() - 1);
            for (size_t c = 0; c < image.width(); ++c)
            {
                float c_ratio = static_cast<float>(c) / static_cast<float>(image.width() - 1);

                vsg::vec3 delta((r_ratio - 0.5f), (c_ratio - 0.5f), (d_ratio - 0.5));
                float angle = atan2(delta.x, delta.y);
                float distance_from_center = sqrt(d_ratio * d_ratio + r_ratio * r_ratio);

                float intensity = (sin(1.0 * angle + 30.0f * distance_from_center + 10.0 * value) + 1.0f) * 0.5f;
                image.set(c, r, d, 1.0f - intensity);
            }
        }
    }

    image.dirty();
}

vsg::ref_ptr<vsg::Node> get_volume_rendering()
{
    vsg::vec3 origin = vsg::vec3(0.0, 0.0, 0.0);
    vsg::vec3 dx = vsg::vec3(1.0, 0.0, 0.0);
    vsg::vec3 dy = vsg::vec3(0.0, 1.0, 0.0);
    vsg::vec3 dz = vsg::vec3(0.0, 0.0, 1.0);
    vsg::vec3 v000(origin);
    vsg::vec3 v100(origin + dx);
    vsg::vec3 v110(origin + dx + dy);
    vsg::vec3 v010(origin + dy);
    vsg::vec3 v001(origin + dz);
    vsg::vec3 v101(origin + dx + dz);
    vsg::vec3 v111(origin + dx + dy + dz);
    vsg::vec3 v011(origin + dy + dz);

    vsg::ref_ptr<vsg::vec3Array> vertices;
    vsg::ref_ptr<vsg::ushortArray> indices;

    // set up vertex and index arrays
    vertices = vsg::vec3Array::create(
        {v000, v100, v101, v001,   // front
         v100, v110, v111, v101,   // right
         v110, v010, v011, v111,   // far
         v010, v000, v001, v011,   // left
         v010, v110, v100, v000,   // bottom
         v001, v101, v111, v011}); // top

    indices = vsg::ushortArray::create(
        {0, 1, 2, 0, 2, 3,
         4, 5, 6, 4, 6, 7,
         8, 9, 10, 8, 10, 11,
         12, 13, 14, 12, 14, 15,
         16, 17, 18, 16, 18, 19,
         20, 21, 22, 20, 22, 23});

    vsg::DataList arrays;
    arrays.push_back(vertices);

    auto vid = vsg::VertexIndexDraw::create();
    vid->assignArrays(arrays);
    vid->assignIndices(indices);
    vid->indexCount = static_cast<uint32_t>(indices->size());
    uint32_t instanceCount = 1;
    vid->instanceCount = instanceCount;
    return vid;
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->add(vsgXchange::all::create());

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->debugLayer = true;
    windowTraits->apiDumpLayer = false;
    windowTraits->width = 1000;
    windowTraits->height = 1000;

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // load shaders
    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/volume.vert", options);
    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/volume.frag", options);

    if (!vertexShader) vertexShader = vsg::ShaderStage::create(VK_SHADER_STAGE_VERTEX_BIT, "main", volume_vert);
    if (!fragmentShader) fragmentShader = vsg::ShaderStage::create(VK_SHADER_STAGE_FRAGMENT_BIT, "main", volume_frag);

    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return {};
    }


    vsg::ref_ptr<vsg::Data> textureData;
    if (auto texturePath = arguments.value<vsg::Path>("", "-i"))
    {
        textureData = vsg::read_cast<vsg::Data>(texturePath, options);
        std::cout<<"Reading "<<textureData<<" from "<<texturePath<<std::endl;
    }

    if (!textureData)
    {
        // read texture image
        auto data = vsg::floatArray3D::create(100, 100, 100);
        data->properties.format = VK_FORMAT_R32_SFLOAT;
        data->properties.dataVariance = vsg::DYNAMIC_DATA;

        updateBaseTexture3D(*data, 1.0f);
        if (!data)
        {
            std::cout << "Could not create texture" << std::endl;
            return {};
        }

        textureData = data;
    }

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls automatically provided by the VSG's DispatchTraversal
    };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
    };


    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->cullMode = VK_CULL_MODE_FRONT_BIT;

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        rasterizationState,
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create()};

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    // create texture image and associated DescriptorSets and binding
    auto clampToEdge_sampler = vsg::Sampler::create();
    clampToEdge_sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    clampToEdge_sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    auto texture = vsg::DescriptorImage::create(clampToEdge_sampler, textureData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texture});
    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->layout, 0, descriptorSet);

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSet);

    auto geometry = get_volume_rendering();
    vsg::dvec3 position(0.0f, 0.0f, 0.0f);
    auto transform = vsg::MatrixTransform::create(vsg::translate(position));

    // add geometry
    transform->addChild(geometry);

    scenegraph->addChild(transform);

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
    auto perspective = vsg::Perspective::create(60.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), 0.1, 20.0);
    auto lookAt = vsg::LookAt::create(vsg::dvec3(1.0, 1.0, 1.0)*2.0, vsg::dvec3(0.5, 0.5, 0.5), vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scenegraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // compile the Vulkan objects
    viewer->compile();

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));
    viewer->addEventHandler(vsg::Trackball::create(camera));

    // main frame loop
    while (viewer->advanceToNextFrame())
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
