#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

namespace CustomText
{

    struct BillboardUniform
    {
        BillboardUniform() {}
        BillboardUniform(float s) : scale(s) {}

        float scale = 2.0f;
    };

    using BillboardUniformValue = vsg::Value<BillboardUniform>;

    class CustomLayout : public vsg::Inherit<vsg::StandardLayout, CustomLayout>
    {
    public:
        vsg::ref_ptr<BillboardUniformValue> billboard;
    };

    vsg::ref_ptr<vsg::ShaderSet> createMyTextShaderSet(const vsg::Path& vertesShaderFilename,  const vsg::Path& fragmentShaderFilename, vsg::ref_ptr<const vsg::Options> options)
    {
        // based on VulkanSceneGraph/src/text/Text.cpp's vsg::createTextShaderSet(ref_ptr<const Options> options) implementation

        // load custom vertex shaders
        auto vertexShader = vsg::read_cast<vsg::ShaderStage>(vertesShaderFilename, options);
        // if (!vertexShader) vertexShader = text_vert(); // fallback to shaders/text_vert.cppp

        auto fragmentShader = vsg::read_cast<vsg::ShaderStage>(fragmentShaderFilename, options);
        // if (!fragmentShader) fragmentShader = text_frag(); // fallback to shaders/text_frag.cppp

        uint32_t numTextIndices = 256;
        vertexShader->specializationConstants = vsg::ShaderStage::SpecializationConstants{
            {0, vsg::uintValue::create(numTextIndices)} // numTextIndices
        };

        auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

        shaderSet->addAttributeBinding("inPosition", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
        shaderSet->addUniformBinding("textureAtlas", "", 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1));

        // use only for the billboard code path.
        shaderSet->addUniformBinding("billboardUniform", "BILLBOARD_AND_SCALE", 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, BillboardUniformValue::create());

        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        shaderSet->addAttributeBinding("inColor", "CPU_LAYOUT", 1, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));
        shaderSet->addAttributeBinding("inOutlineColor", "CPU_LAYOUT", 2, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));
        shaderSet->addAttributeBinding("inOutlineWidth", "CPU_LAYOUT", 3, VK_FORMAT_R32_SFLOAT, vsg::floatArray::create(1));
        shaderSet->addAttributeBinding("inTexCoord", "CPU_LAYOUT", 4, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

        // assign the custom uniform binding

        return shaderSet;
    }


    class CustomTechnique : public vsg::Inherit<vsg::CpuLayoutTechnique, CustomTechnique>
    {
    public:
        void setup(vsg::Text* text, uint32_t minimumAllocation = 0) override;

    };

}

EVSG_type_name(CustomText::BillboardUniformValue);
EVSG_type_name(CustomText::CustomLayout);
EVSG_type_name(CustomText::CustomTechnique);

using namespace CustomText;

vsg::RegisterWithObjectFactoryProxy<CustomText::BillboardUniformValue> s_Register_BillboardUniformValue;
vsg::RegisterWithObjectFactoryProxy<CustomText::CustomLayout> s_Register_CustomLayout;
vsg::RegisterWithObjectFactoryProxy<CustomText::CustomTechnique> s_Register_CustomTechnique;

void CustomTechnique::setup(vsg::Text* text, uint32_t minimumAllocation)
{
    std::cout<<"CustomTechnique::setup()"<<std::endl;

    auto layout = text->layout;

    vsg::TextQuads quads;
    layout->layout(text->text, *(text->font), quads);


    vsg::vec4 color = quads.front().colors[0];
    vsg::vec4 outlineColor = quads.front().outlineColors[0];
    float outlineWidth = quads.front().outlineWidths[0];
    bool singleColor = true;
    bool singleOutlineColor = true;
    bool singleOutlineWidth = true;
    for (auto& quad : quads)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (quad.colors[i] != color) singleColor = false;
            if (quad.outlineColors[i] != outlineColor) singleOutlineColor = false;
            if (quad.outlineWidths[i] != outlineWidth) singleOutlineWidth = false;
        }
    }

    uint32_t num_quads = std::max(static_cast<uint32_t>(quads.size()), minimumAllocation);
    uint32_t num_vertices = num_quads * 4;
    uint32_t num_colors = singleColor ? 1 : num_vertices;
    uint32_t num_outlineColors = singleOutlineColor ? 1 : num_vertices;
    uint32_t num_outlineWidths = singleOutlineWidth ? 1 : num_vertices;

    if (!vertices || num_vertices > vertices->size()) vertices = vsg::vec3Array::create(num_vertices);
    if (!colors || num_colors > colors->size()) colors = vsg::vec4Array::create(num_colors);
    if (!outlineColors || num_outlineColors > outlineColors->size()) outlineColors = vsg::vec4Array::create(num_outlineColors);
    if (!outlineWidths || num_outlineWidths > outlineWidths->size()) outlineWidths = vsg::floatArray::create(num_outlineWidths);
    if (!texcoords || num_vertices > texcoords->size()) texcoords = vsg::vec3Array::create(num_vertices);

    uint32_t vi = 0;

    float leadingEdgeGradient = 0.1f;

    if (singleColor) colors->set(0, color);
    if (singleOutlineColor) outlineColors->set(0, outlineColor);
    if (singleOutlineWidth) outlineWidths->set(0, outlineWidth);

    for (auto& quad : quads)
    {
        float leadingEdgeTilt = length(quad.vertices[0] - quad.vertices[1]) * leadingEdgeGradient;
        float topEdgeTilt = leadingEdgeTilt;

        vertices->set(vi, quad.vertices[0]);
        vertices->set(vi + 1, quad.vertices[1]);
        vertices->set(vi + 2, quad.vertices[2]);
        vertices->set(vi + 3, quad.vertices[3]);

        if (!singleColor)
        {
            colors->set(vi, quad.colors[0]);
            colors->set(vi + 1, quad.colors[1]);
            colors->set(vi + 2, quad.colors[2]);
            colors->set(vi + 3, quad.colors[3]);
        }

        if (!singleOutlineColor)
        {
            outlineColors->set(vi, quad.outlineColors[0]);
            outlineColors->set(vi + 1, quad.outlineColors[1]);
            outlineColors->set(vi + 2, quad.outlineColors[2]);
            outlineColors->set(vi + 3, quad.outlineColors[3]);
        }

        if (!singleOutlineWidth)
        {
            outlineWidths->set(vi, quad.outlineWidths[0]);
            outlineWidths->set(vi + 1, quad.outlineWidths[1]);
            outlineWidths->set(vi + 2, quad.outlineWidths[2]);
            outlineWidths->set(vi + 3, quad.outlineWidths[3]);
        }

        texcoords->set(vi, vsg::vec3(quad.texcoords[0].x, quad.texcoords[0].y, leadingEdgeTilt + topEdgeTilt));
        texcoords->set(vi + 1, vsg::vec3(quad.texcoords[1].x, quad.texcoords[1].y, topEdgeTilt));
        texcoords->set(vi + 2, vsg::vec3(quad.texcoords[2].x, quad.texcoords[2].y, 0.0f));
        texcoords->set(vi + 3, vsg::vec3(quad.texcoords[3].x, quad.texcoords[3].y, leadingEdgeTilt));

        vi += 4;
    }

    uint32_t num_indices = num_quads * 6;
    if (!indices || num_indices > indices->valueCount())
    {
        if (num_vertices > 65536) // check if requires uint or ushort indices
        {
            auto ui_indices = vsg::uintArray::create(num_indices);
            indices = ui_indices;

            auto itr = ui_indices->begin();
            vi = 0;
            for (uint32_t i = 0; i < num_quads; ++i)
            {
                (*itr++) = vi;
                (*itr++) = vi + 1;
                (*itr++) = vi + 2;
                (*itr++) = vi + 2;
                (*itr++) = vi + 3;
                (*itr++) = vi;

                vi += 4;
            }
        }
        else
        {
            auto us_indices = vsg::ushortArray::create(num_indices);
            indices = us_indices;

            auto itr = us_indices->begin();
            vi = 0;
            for (uint32_t i = 0; i < num_quads; ++i)
            {
                (*itr++) = vi;
                (*itr++) = vi + 1;
                (*itr++) = vi + 2;
                (*itr++) = vi + 2;
                (*itr++) = vi + 3;
                (*itr++) = vi;

                vi += 4;
            }
        }
    }

    if (!drawIndexed)
        drawIndexed = vsg::DrawIndexed::create(static_cast<uint32_t>(quads.size() * 6), 1, 0, 0, 0);
    else
        drawIndexed->indexCount = static_cast<uint32_t>(quads.size() * 6);

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    if (!scenegraph)
    {
        scenegraph = vsg::StateGroup::create();

        auto shaderSet = createTextShaderSet(text->font->options);
        auto config = vsg::GraphicsPipelineConfig::create(shaderSet);

        auto& sharedObjects = text->font->sharedObjects;
        if (!sharedObjects && text->font->options) sharedObjects = text->font->options->sharedObjects;
        if (!sharedObjects) sharedObjects = vsg::SharedObjects::create();

        vsg::DataList arrays;
        config->assignArray(arrays, "inPosition", VK_VERTEX_INPUT_RATE_VERTEX, vertices);
        config->assignArray(arrays, "inColor", singleColor ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX, colors);
        config->assignArray(arrays, "inOutlineColor", singleOutlineColor ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX, outlineColors);
        config->assignArray(arrays, "inOutlineWidth", singleOutlineWidth ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX, outlineWidths);
        config->assignArray(arrays, "inTexCoord", VK_VERTEX_INPUT_RATE_VERTEX, texcoords);

        // set up sampler for atlas.
        auto sampler = vsg::Sampler::create();
        sampler->magFilter = VK_FILTER_LINEAR;
        sampler->minFilter = VK_FILTER_LINEAR;
        sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler->borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        sampler->anisotropyEnable = VK_TRUE;
        sampler->maxAnisotropy = 16.0f;
        sampler->maxLod = 12.0;

        if (sharedObjects) sharedObjects->share(sampler);

        vsg::Descriptors descriptors;
        config->assignTexture(descriptors, "textureAtlas", text->font->atlas, sampler);

        // custom part - assigning our billboardUnfirom
        auto customLayout = layout.cast<CustomLayout>();
        if (customLayout && customLayout->billboard)
        {
            std::cout<<"Assigning billboard"<<std::endl;
            config->assignUniform(descriptors, "billboardUniform", customLayout->billboard);
        }

        if (sharedObjects) sharedObjects->share(descriptors);

        // disable face culling so text can be seen from both sides
        config->rasterizationState->cullMode = VK_CULL_MODE_NONE;

        // set alpha blending
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT;

        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        config->colorBlendState->attachments = {colorBlendAttachment};

        if (sharedObjects)
            sharedObjects->share(config, [](auto gpc) { gpc->init(); });
        else
            config->init();

        scenegraph->add(config->bindGraphicsPipeline);

        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(config->descriptorBindings);
        if (sharedObjects) sharedObjects->share(descriptorSetLayout);
        auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, descriptors);

        auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, config->layout, 0, descriptorSet);
        if (sharedObjects) sharedObjects->share(bindDescriptorSet);

        scenegraph->add(bindDescriptorSet);


        bindVertexBuffers = vsg::BindVertexBuffers::create(0, arrays);
        bindIndexBuffer = vsg::BindIndexBuffer::create(indices);

        // setup geometry
        auto drawCommands = vsg::Commands::create();
        drawCommands->addChild(bindVertexBuffers);
        drawCommands->addChild(bindIndexBuffer);
        drawCommands->addChild(drawIndexed);

        scenegraph->addChild(drawCommands);
    }
    else
    {
        std::cout << "TODO : CpuLayoutTechnique::setup(), does not yet support updates. Consider using GpuLayoutTechnique instead." << std::endl;
        // bindVertexBuffers->copyDataToBuffers();
        // bindIndexBuffer->copyDataToBuffers();
    }
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create("vsg::Text example");
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);
    auto font_filename = arguments.value(std::string("fonts/times.vsgb"), "-f");
    auto output_filename = arguments.value(std::string(), "-o");
    auto numFrames = arguments.value(-1, "--nf");
    auto clearColor = arguments.value(vsg::vec4(0.2f, 0.2f, 0.4f, 1.0f), "--clear");

    auto vertesShaderFilename = arguments.value<vsg::Path>("shaders/custom_text.vert", "--vert");
    auto fragmentShaderFilename = arguments.value<vsg::Path>("shaders/custom_text.frag", "--frag");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    auto sharedObjects = options->sharedObjects = vsg::SharedObjects::create();
#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    vsg::ref_ptr<vsg::Node> scenegraph;

    if (argc>1)
    {
        vsg::Path filename = arguments[1];
        scenegraph = vsg::read_cast<vsg::Node>(filename, options);
        if (!scenegraph)
        {
            std::cout<<"Error reading file "<<filename<<std::endl;
            return 1;
        }
    }
    else
    {
        auto group = vsg::Group::create();
        scenegraph = group;

        auto font = vsg::read_cast<vsg::Font>(font_filename, options);

        if (!font)
        {
            std::cout << "Failing to read font : " << font_filename << std::endl;
            return 1;
        }

        font->options = options;
        {
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
            layout->position = vsg::vec3(0.0, 0.0, 0.0);
            layout->horizontal = vsg::vec3(1.0, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 1.0);
            layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);
            layout->outlineWidth = 0.1;

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("Standard vsg::Text shaders.");
            text->font = font;
            text->layout = layout;
            text->setup();
            group->addChild(text);
        }

        {
            font->options->shaderSets["text"] = createMyTextShaderSet(vertesShaderFilename, fragmentShaderFilename, options);

            auto layout = CustomLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
            layout->position = vsg::vec3(0.0, -2.0, 0.0);
            layout->horizontal = vsg::vec3(1.0, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 1.0, 0.0);
            layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);
            layout->outlineWidth = 0.1;

            // assign BillboardUnfirom to enable the billboard effect and pass settings.
            layout->billboard = BillboardUniformValue::create(1.5f);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("Custom vsg::Text shaders.");
            text->font = font;
            text->layout = layout;
            text->technique = CustomTechnique::create();
            text->setup();

            group->addChild(text);
        }
    }

    if (!output_filename.empty())
    {
        vsg::write(scenegraph, output_filename);
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

    window->clearColor() = VkClearColorValue{{clearColor.r, clearColor.g, clearColor.b, clearColor.a}};

    viewer->addWindow(window);

    // camera related details
    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto viewport = vsg::ViewportState::create(window->extent2D());
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 1000.0);
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scenegraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // compile the Vulkan objects
    viewer->compile();

    // assign Trackball
    viewer->addEventHandler(vsg::Trackball::create(camera));

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

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
