#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

//
vsg::ref_ptr<vsg::Node> createTexturedQuad(const vsg::vec3& position, const vsg::vec2& extents, vsg::ref_ptr<vsg::Data> textureData, vsg::ref_ptr<vsg::Sampler> sampler, vsg::ref_ptr<vsg::Options> options)
{
    auto sharedObjects = options->sharedObjects;
    auto shaderSet = vsg::createFlatShadedShaderSet(options);
    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(shaderSet);

    // enable texturing
    graphicsPipelineConfig->assignTexture("diffuseMap", textureData, sampler);

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
        {position,
         position + vsg::vec3(extents.x, 0.0f, 0.0f),
         position + vsg::vec3(extents.x, 0.0f, extents.y),
         position + vsg::vec3(0.0f, 0.0f, extents.y)
        });

    auto normals = vsg::vec3Array::create(
        {{0.0f, -1.0f, 0.0f}
        });

    auto texcoords = vsg::vec2Array::create(
        {{0.0f, 1.0f},
         {1.0f, 1.0f},
         {1.0f, 0.0f},
         {0.0f, 0.0f}
        });

    auto colors = vsg::vec4Value::create(vsg::vec4{1.0f, 1.0f, 1.0f, 1.0f});

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
         2, 3, 0});

    vsg::DataList vertexArrays;

    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_INSTANCE, normals);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, texcoords);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, colors);

    if (sharedObjects) sharedObjects->share(vertexArrays);
    if (sharedObjects) sharedObjects->share(indices);

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(6, 1, 0, 0, 0));

    if (sharedObjects)
    {
        sharedObjects->share(drawCommands->children);
        sharedObjects->share(drawCommands);
    }

    // share the pipeline config and initialize it if it's unique
    if (sharedObjects)
        sharedObjects->share(graphicsPipelineConfig, [](auto gpc) { gpc->init(); });
    else
        graphicsPipelineConfig->init();

    // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
    auto stateGroup = vsg::StateGroup::create();

    graphicsPipelineConfig->copyTo(stateGroup, sharedObjects);

    stateGroup->addChild(drawCommands);

    return stateGroup;
}

vsg::ref_ptr<vsg::Node> createText(const vsg::vec3& position, const vsg::vec2& extents, const std::string& str, vsg::ref_ptr<vsg::Options> options)
{
    auto font = vsg::read_cast<vsg::Font>("fonts/times.vsgb", options);

    auto layout = vsg::StandardLayout::create();
    layout->glyphLayout = vsg::StandardLayout::LEFT_TO_RIGHT_LAYOUT;
    layout->position = position;
    layout->horizontal = vsg::vec3(extents.x, 0.0, 0.0);
    layout->vertical = vsg::vec3(0.0, 0.0, extents.y);
    layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);

    auto text = vsg::Text::create();
    text->text = vsg::stringValue::create(str);
    text->font = font;
    text->layout = layout;
    text->setup(0, options);
    return text;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create(arguments);

    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    options->readOptions(arguments);

    float maxLodDefault = VK_LOD_CLAMP_NONE;
    arguments.read("--maxLod", maxLodDefault);

    auto annotate = !arguments.read("--no-text");
    auto numFrames = arguments.value(-1, "-f");
    auto outputFile = arguments.value<vsg::Path>("", "-o");

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create window." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    auto instance = window->getOrCreateInstance();
    auto physicalDevice = window->getOrCreatePhysicalDevice();
    if (!physicalDevice)
    {
        std::cout<<"No vkPhyscialDevice available."<<std::endl;
        return 1;
    }


    uint32_t format_low = VK_FORMAT_UNDEFINED, format_high = VK_FORMAT_UNDEFINED;
    if (arguments.read("--formats", format_low, format_high))
    {
        VkImageType type = VK_IMAGE_TYPE_2D;
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageCreateFlags flags = 0;

        for(uint32_t format = format_low; format <= format_high; format += 1)
        {
            VkImageFormatProperties imageFormatProperties;
            VkResult result = vkGetPhysicalDeviceImageFormatProperties(*physicalDevice, VkFormat(format), type, tiling, usage, flags, &imageFormatProperties);
            if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                std::cout<<"image format "<<format<<" not supported."<<std::endl;
            }
            else
            {
                std::cout<<"image format "<<format<<" supported."<<std::endl;
            }
        }
    }

    std::map<vsg::Path, vsg::ref_ptr<vsg::Data>> images;
    for(auto i = 1; i < argc; ++i)
    {
        vsg::Path filename = arguments[i];
        if (auto image = vsg::read_cast<vsg::Data>(filename, options)) images[filename] = image;
        else std::cout<<"Unable to load "<<filename<<std::endl;
    }

    // if nothing loaded
    if (images.empty()) return 1;


    std::cout<<"Loading "<<images.size()<<" images."<<std::endl;

    auto scenegraph = vsg::Group::create();
    vsg::vec3 position(0.0f, 0.0f, 0.0f);
    vsg::vec2 delta(16.0f, 16.0f);
    vsg::vec2 textExtents(delta.x*0.5f, delta.y*0.5f);


    auto formatSupported = [&](VkFormat format) -> bool
    {
        if (VK_FORMAT_R8G8B8_UNORM <= format && format <= VK_FORMAT_B8G8R8_SRGB)
        {
            // RGB formats are converted to RGBA automatically by the VSG when transferring images
            return true;
        }
        else
        {
            VkImageType type = VK_IMAGE_TYPE_2D;
            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
            VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
            VkImageCreateFlags flags = 0;
            VkImageFormatProperties imageFormatProperties;
            return vkGetPhysicalDeviceImageFormatProperties(*physicalDevice, format, type, tiling, usage, flags, &imageFormatProperties) != VK_ERROR_FORMAT_NOT_SUPPORTED;
        }
    };

    for(auto& [filename, image] : images)
    {
        if (!formatSupported(image->properties.format))
        {
            std::cout<<filename<<" : Image format "<<image->properties.format<<" not supported by Vulkan driver/hardware."<<std::endl;
            continue;
        }

        if (image->depth() == 1 && image->properties.imageViewType != VK_IMAGE_VIEW_TYPE_2D)
        {
            image->properties.imageViewType = VK_IMAGE_VIEW_TYPE_2D;
        }

        if (image->depth()>1 /*image->properties.imageViewType != VK_IMAGE_VIEW_TYPE_2D*/)
        {
            std::cout<<"image "<<filename<<" is a texture array, cubemap or 3d texture, can't render for now, dimensions = {"<<image->width()<<", "<<image->height()<<", "<<image->depth()<<"}"<<std::endl;
        }
        else
        {
            auto mipmapLayout = image->getMipmapLayout();
            auto [width, height, depth] = image->pixelExtents();
            vsg::vec2 extents(width, height);

            // default mipmap settings
            {
                auto sampler = vsg::Sampler::create();
                sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler->minLod = 0.0f;
                sampler->maxLod = maxLodDefault;

                if (auto model = createTexturedQuad(position, extents, image, sampler, options))
                {
                    scenegraph->addChild(model);
                    if (annotate)
                    {
                        scenegraph->addChild(createText(position - vsg::vec3(0.0f, 0.0f, textExtents[1]), textExtents, vsg::make_string(filename, "\n", image->width(), " x ", image->height(), " format = ", image->properties.format), options));
                    }

                    position.x += extents[0] + delta[0] * 2.0f;
                }
            }

            // create an image for each mipmap level.
            if (image->properties.mipLevels > 1)
            {
                for(uint8_t level = 0; level < image->properties.mipLevels; ++level)
                {
                    auto sampler = vsg::Sampler::create();
                    sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                    sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                    sampler->minLod = static_cast<float>(level);
                    sampler->maxLod = static_cast<float>(level);

                    if (auto model = createTexturedQuad(position, extents, image, sampler, options))
                    {

                        scenegraph->addChild(model);
                        if (annotate)
                        {
                            scenegraph->addChild(createText(position - vsg::vec3(0.0f, 0.0f, textExtents[1]), textExtents, vsg::make_string("level = ", level), options));

                            if (mipmapLayout)
                            {
                                auto& mipmapExtents = mipmapLayout->at(level);
                                scenegraph->addChild(createText(position - vsg::vec3(0.0f, 0.0f, textExtents[1]*2.0f), textExtents, vsg::make_string(mipmapExtents), options));
                            }
                        }

                        position.x += extents[0] + delta[0];
                    }
                }
            }

            position.x = 0.0f;
            position.z += extents[1] + delta[1];
        }
    }

    if (scenegraph->children.empty())
    {
        std::cout<<"No images loaded to render, please specify an image filename on command line."<<std::endl;
        return 0;
    }

    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6 * 3.0;

    // camera related details
    double nearFarRatio = 0.001;
    auto viewport = vsg::ViewportState::create(0, 0, window->extent2D().width, window->extent2D().height);
    auto perspective = vsg::Perspective::create(60.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.0);
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scenegraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // compile the Vulkan objects
    viewer->compile();

    if (outputFile)
    {
        vsg::write(scenegraph, outputFile, options);
        return 0;
    }

    // assign a CloseHandler to the Viewer to respond to pressing Escape or the window close button
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    viewer->addEventHandler(vsg::Trackball::create(camera));

    // main frame loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
