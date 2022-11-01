#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

vsg::ref_ptr<vsg::Node> createQuad(const vsg::vec3& origin, const vsg::vec3& horizontal, const vsg::vec3& vertical, vsg::ref_ptr<vsg::Data> sourceData = {})
{
    struct ConvertToRGBA : public vsg::Visitor
    {
        vsg::ref_ptr<vsg::Data> textureData;

        void apply(vsg::Data& data) override
        {
            textureData = &data;
        }

        void apply(vsg::uintArray2D& fa) override
        {
            // treat as a 24bit depth buffer
            float div = 1.0f / static_cast<float>(1 << 24);

            auto rgba = vsg::vec4Array2D::create(fa.width(), fa.height(), vsg::Data::Properties{VK_FORMAT_R32G32B32A32_SFLOAT});
            auto dest_itr = rgba->begin();
            for (auto& v : fa)
            {
                float m = static_cast<float>(v) * div;
                (*dest_itr++).set(m, m, m, 1.0);
            }
            textureData = rgba;
        }

        void apply(vsg::floatArray2D& fa) override
        {
            auto rgba = vsg::vec4Array2D::create(fa.width(), fa.height(), vsg::Data::Properties{VK_FORMAT_R32G32B32A32_SFLOAT});
            auto dest_itr = rgba->begin();
            for (auto& v : fa)
            {
                (*dest_itr++).set(v, v, v, 1.0);
            }
            textureData = rgba;
        }

        vsg::ref_ptr<vsg::Data> convert(vsg::ref_ptr<vsg::Data> data)
        {
            if (data)
                data->accept(*this);
            else
            {
                auto image = vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32B32A32_SFLOAT});
                image->set(0, 0, vsg::vec4(0.5f, 1.0f, 0.5f, 1.0f));
                textureData = image;
            }

            return textureData;
        }

    } convertToRGBA;

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    auto textureData = convertToRGBA.convert(sourceData);
    if (!textureData) return {};

    // load shaders
    vsg::ref_ptr<vsg::ShaderStage> vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    vsg::ref_ptr<vsg::ShaderStage> fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return {};
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
    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, vsg::DescriptorSets{descriptorSet});

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSets);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT

    // add transform to root of the scene graph
    scenegraph->addChild(transform);

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
        {origin,
         origin + horizontal,
         origin + horizontal + vertical,
         origin + vertical}); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec3Array::create(
        {{1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f}}); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    bool top_left = textureData->properties.origin == vsg::TOP_LEFT; // in Vulkan the origin is by default top left.
    float left = 0.0f;
    float right = 1.0f;
    float top = top_left ? 0.0f : 1.0f;
    float bottom = top_left ? 1.0f : 0.0f;
    auto texcoords = vsg::vec2Array::create(
        {{left, bottom},
         {right, bottom},
         {right, top},
         {left, top}}); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
         2, 3, 0}); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, colors, texcoords}));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(6, 1, 0, 0, 0));

    // add drawCommands to transform
    transform->addChild(drawCommands);

    return scenegraph;
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
    auto render_all_glyphs = arguments.read("--all");
    auto enable_tests = arguments.read("--test");
    auto numFrames = arguments.value(-1, "--nf");
    auto clearColor = arguments.value(vsg::vec4(0.2f, 0.2f, 0.4f, 1.0f), "--clear");
    bool disableDepthTest = arguments.read({"--ddt", "--disable-depth-test"});
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto font = vsg::read_cast<vsg::Font>(font_filename, options);

    if (!font)
    {
        std::cout << "Failing to read font : " << font_filename << std::endl;
        return 1;
    }

    if (disableDepthTest)
    {
        // assign a custom StateSet to options->shaderSets so that subsequent TextGroup::setup(0, options) call will pass in our custom ShaderSet.
        auto shaderSet = options->shaderSets["text"] = vsg::createTextShaderSet(options);

        // create a DepthStencilState, disable depth test and add this to the ShaderSet::defaultGraphicsPipelineStates container so it's used when setting up the TextGroup subgraph
        auto depthStencilState = vsg::DepthStencilState::create();
        depthStencilState->depthTestEnable = VK_FALSE;
        shaderSet->defaultGraphicsPipelineStates.push_back(depthStencilState);
    }

    // set up model transformation node
    auto scenegraph = vsg::Group::create();

    if (render_all_glyphs)
    {
        auto layout = vsg::StandardLayout::create();
        layout->position = vsg::vec3(0.0, 0.0, 0.0);
        layout->horizontal = vsg::vec3(1.0, 0.0, 0.0);
        layout->vertical = vsg::vec3(0.0, 0.0, 1.0);
        layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);

        std::set<uint32_t> characters;
        for (auto c : *(font->charmap))
        {
            if (c != 0) characters.insert(c);
        }

        size_t num_glyphs = characters.size();
        size_t row_lenghth = static_cast<size_t>(ceil(sqrt(float(num_glyphs))));
        size_t num_rows = num_glyphs / row_lenghth;
        if ((num_glyphs % num_rows) != 0) ++num_rows;

        // use an uintArray to store the text string as the full font charcodes can go up to very large values.
        auto text_string = vsg::uintArray::create(num_glyphs + num_rows - 1);
        auto text_itr = text_string->begin();

        size_t i = 0;
        size_t column = 0;

        for (auto c : characters)
        {
            ++i;
            ++column;

            (*text_itr++) = c;

            if (column >= row_lenghth)
            {
                (*text_itr++) = '\n';
                column = 0;
            }
        }

        auto text = vsg::Text::create();
        text->font = font;
        text->layout = layout;
        text->text = text_string;
        text->setup(0, options);

        scenegraph->addChild(text);
    }
    else
    {
        {
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
            layout->position = vsg::vec3(6.0, 0.0, 0.0);
            layout->horizontal = vsg::vec3(1.0, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 1.0, 0.0);
            layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);
            layout->outlineWidth = 0.1;
            layout->billboard = true;

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("VulkanSceneGraph now\nhas SDF text support.");
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            scenegraph->addChild(text);

            if (enable_tests)
            {
                auto quad = createQuad(layout->position, layout->horizontal, layout->vertical);
                scenegraph->addChild(quad);
            }
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->glyphLayout = vsg::StandardLayout::VERTICAL_LAYOUT;
            layout->position = vsg::vec3(-1.0, 0.0, 2.0);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(1.0, 0.0, 0.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("VERTICAL_LAYOUT");
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            scenegraph->addChild(text);

            if (enable_tests)
            {
                auto quad = createQuad(layout->position, layout->horizontal, layout->vertical);
                scenegraph->addChild(quad);
            }
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->glyphLayout = vsg::StandardLayout::LEFT_TO_RIGHT_LAYOUT;
            layout->position = vsg::vec3(-1.0, 0.0, 2.0);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(0.0, 1.0, 0.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("LEFT_TO_RIGHT_LAYOUT");
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            scenegraph->addChild(text);

            if (enable_tests)
            {
                auto quad = createQuad(layout->position, layout->horizontal, layout->vertical);
                scenegraph->addChild(quad);
            }
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->glyphLayout = vsg::StandardLayout::RIGHT_TO_LEFT_LAYOUT;
            layout->position = vsg::vec3(13.0, 0.0, 2.0);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(0.0, 0.0, 1.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("RIGHT_TO_LEFT_LAYOUT");
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            scenegraph->addChild(text);

            if (enable_tests)
            {
                auto quad = createQuad(layout->position, layout->horizontal, layout->vertical);
                scenegraph->addChild(quad);
            }
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
            layout->position = vsg::vec3(2.0, 0.0, -8.0);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(1.0, 0.0, 1.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("horizontalAlignment\nCENTER_ALIGNMENT");
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            scenegraph->addChild(text);

            if (enable_tests)
            {
                auto quad = createQuad(layout->position, layout->horizontal, layout->vertical);
                scenegraph->addChild(quad);
            }
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::LEFT_ALIGNMENT;
            layout->position = vsg::vec3(2.0, 0.0, -9.0);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(1.0, 1.0, 0.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("horizontalAlignment\nLEFT_ALIGNMENT");
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            scenegraph->addChild(text);

            if (enable_tests)
            {
                auto quad = createQuad(layout->position, layout->horizontal, layout->vertical);
                scenegraph->addChild(quad);
            }
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::RIGHT_ALIGNMENT;
            layout->position = vsg::vec3(2.0, 0.0, -10.0);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(0.0, 1.0, 1.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("horizontalAlignment\nRIGHT_ALIGNMENT");
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            scenegraph->addChild(text);

            if (enable_tests)
            {
                auto quad = createQuad(layout->position, layout->horizontal, layout->vertical);
                scenegraph->addChild(quad);
            }
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
            layout->verticalAlignment = vsg::StandardLayout::BOTTOM_ALIGNMENT;
            layout->position = vsg::vec3(10.0, 0.0, -8.5);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(0.0, 1.0, 1.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("verticalAlignment\nBOTTOM_ALIGNMENT");
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            scenegraph->addChild(text);

            if (enable_tests)
            {
                auto quad = createQuad(layout->position, layout->horizontal, layout->vertical);
                scenegraph->addChild(quad);
            }
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
            layout->verticalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
            layout->position = vsg::vec3(10.0, 0.0, -9.0);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(1.0, 0.0, 1.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("verticalAlignment\nCENTER_ALIGNMENT");
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            scenegraph->addChild(text);

            if (enable_tests)
            {
                auto quad = createQuad(layout->position, layout->horizontal, layout->vertical);
                scenegraph->addChild(quad);
            }
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
            layout->verticalAlignment = vsg::StandardLayout::TOP_ALIGNMENT;
            layout->position = vsg::vec3(10.0, 0.0, -9.5);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(1.0, 1.0, 0.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("verticalAlignment\nTOP_ALIGNMENT");
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            scenegraph->addChild(text);

            if (enable_tests)
            {
                auto quad = createQuad(layout->position, layout->horizontal, layout->vertical);
                scenegraph->addChild(quad);
            }
        }

        if (output_filename.empty())
        {
            struct CustomLayout : public vsg::Inherit<vsg::StandardLayout, CustomLayout>
            {
                void layout(const vsg::Data* text, const vsg::Font& font, vsg::TextQuads& quads) override
                {
                    // Let the base StandardLayout class do the basic glyph setup
                    Inherit::layout(text, font, quads);

                    // modify each generated glyph quad's position and colours etc.
                    for (auto& quad : quads)
                    {
                        for (int i = 0; i < 4; ++i)
                        {
                            quad.vertices[i].z += 0.5f * sin(quad.vertices[i].x);
                            quad.colors[i].r = 0.5f + 0.5f * sin(quad.vertices[i].x);
                            quad.outlineColors[i] = vsg::vec4(cos(0.5 * quad.vertices[i].x), 0.1f, 0.0f, 1.0f);
                            quad.outlineWidths[i] = 0.1f + 0.15f * (1.0f + sin(quad.vertices[i].x));
                        }
                    }
                };
            };

            auto layout = CustomLayout::create();
            layout->position = vsg::vec3(0.0, 0.0, -3.0);
            layout->horizontal = vsg::vec3(1.0, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 1.0);
            layout->color = vsg::vec4(1.0, 0.5, 1.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("You can use Outlines\nand your own CustomLayout.");
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            scenegraph->addChild(text);
        }
    }

    auto dynamic_text_label = vsg::stringValue::create("GpuLayoutTechnique");
    auto dynamic_text_layout = vsg::StandardLayout::create();
    auto dynamic_text = vsg::Text::create();
    {
        // currently vsg::GpuLayoutTechnique is the only technique that supports dynamic update of the text parameters
        dynamic_text->technique = vsg::GpuLayoutTechnique::create();

        dynamic_text_layout->billboard = true;
        dynamic_text_layout->position = vsg::vec3(0.0, 0.0, -6.0);
        dynamic_text_layout->horizontal = vsg::vec3(1.0, 0.0, 0.0);
        dynamic_text_layout->vertical = dynamic_text_layout->billboard ? vsg::vec3(0.0, 1.0, 0.0) : vsg::vec3(0.0, 0.0, 1.0) ;
        dynamic_text_layout->color = vsg::vec4(1.0, 0.9, 1.0, 1.0);
        dynamic_text_layout->outlineWidth = 0.1;

        dynamic_text->text = dynamic_text_label;
        dynamic_text->font = font;
        dynamic_text->layout = dynamic_text_layout;
        dynamic_text->setup(32); // allocate enough space for max possible characters
        scenegraph->addChild(dynamic_text);
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
        // update the dynamic_text label string and position
        dynamic_text_label->value() = vsg::make_string("GpuLayoutTechnique: ", viewer->getFrameStamp()->frameCount);
        dynamic_text_layout->position.x += 0.01;
        dynamic_text->setup(0, options);

        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
