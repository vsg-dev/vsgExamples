#include <iostream>
#include <vsg/all.h>

class UpdateImage : public vsg::Visitor
{
public:
    double value = 0.0;

    template<class A>
    void update(A& image)
    {
        using value_type = typename A::value_type;
        float r_mult = 1.0f / static_cast<float>(image.height() - 1);
        float r_offset = 0.5f + sin(value) * 0.25f;

        float c_mult = 1.0f / static_cast<float>(image.width() - 1);
        float c_offset = 0.5f + cos(value) * 0.25f;

        for (size_t r = 0; r < image.height(); ++r)
        {
            float r_ratio = static_cast<float>(r) * r_mult;
            value_type* ptr = &image.at(0, r);
            for (size_t c = 0; c < image.width(); ++c)
            {
                float c_ratio = static_cast<float>(c) * c_mult;

                float intensity = 0.5f - ((r_ratio - r_offset) * (r_ratio - r_offset)) + ((c_ratio - c_offset) * (c_ratio - c_offset));

                if constexpr (std::is_same_v<value_type, float>)
                {
                    (*ptr) = intensity * intensity;
                }
                else
                {
                    ptr->r = intensity * intensity;
                    ptr->g = intensity;
                    ptr->b = intensity;

                    if constexpr (std::is_same_v<value_type, vsg::vec4>) ptr->a = 1.0f;
                }

                ++ptr;
            }
        }
        image.dirty();
    }

    // use the vsg::Visitor to safely cast to types handled by the UpdateImage class
    void apply(vsg::floatArray2D& image) override { update(image); }
    void apply(vsg::vec3Array2D& image) override { update(image); }
    void apply(vsg::vec4Array2D& image) override { update(image); }

    // provide convenient way to invoke the UpdateImage as a functor
    void operator()(vsg::Data* image, double v)
    {
        value = v;
        image->accept(*this);
    }
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--double-buffer")) windowTraits->swapchainPreferences.imageCount = 2;
    if (arguments.read("--triple-buffer")) windowTraits->swapchainPreferences.imageCount = 3; // default
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    arguments.read("--window", windowTraits->width, windowTraits->height);
    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    if (arguments.read("-t"))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->width = 192, windowTraits->height = 108;
        windowTraits->decoration = false;
    }
    auto numFrames = arguments.value(-1, "-f");

    enum ArrayType
    {
        USE_FLOAT,
        USE_RGB,
        USE_RGBA
    };

    bool heightfield = arguments.read("--hf");
    ArrayType arrayType = heightfield ? USE_FLOAT : USE_RGBA;
    if (arguments.read("--float")) arrayType = USE_FLOAT;
    if (arguments.read("--rgb")) arrayType = USE_RGB;
    if (arguments.read("--rgba")) arrayType = USE_RGBA;

    auto image_size = arguments.value<uint32_t>(256, "-s");

    vsg::GeometryInfo geomInfo;
    vsg::StateInfo stateInfo;
    stateInfo.wireframe = arguments.read("--wireframe");
    stateInfo.lighting = !arguments.read("--flat");
    stateInfo.doubleSided = true;

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // load shaders
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return 1;
    }

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);

    // camera related details
    auto viewport = vsg::ViewportState::create(window->extent2D());
    auto perspective = vsg::Perspective::create(60.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), 0.1, 10.0);
    auto lookAt = vsg::LookAt::create(vsg::dvec3(1.0, 1.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    // add event handlers
    viewer->addEventHandler(vsg::Trackball::create(camera));
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    // setup texture image
    vsg::ref_ptr<vsg::Data> textureData;
    switch (arrayType)
    {
    case (USE_FLOAT):
        // use float image - typically for displacementMap
        textureData = vsg::floatArray2D::create(image_size, image_size);
        textureData->getLayout().format = VK_FORMAT_R32_SFLOAT;
        break;
    case (USE_RGB):
        // note, RGB image data has to be converted to RGBA when copying to a vkImage,
        // the VSG will do this automatically do the RGB to RGBA conversion for you each time the data is copied
        // this makes RGB substantially slower than using RGBA data.
        // one approach, illustrated in the vsgdynamictexture_cs example, for avoiding this conversion overhead is to use a compute shader to map the RGB data to RGBA.
        textureData = vsg::vec3Array2D::create(image_size, image_size);
        textureData->getLayout().format = VK_FORMAT_R32G32B32_SFLOAT;
        break;
    case (USE_RGBA):
        // R, RG and RGBA data can be copied to vkImage without any conversion so is efficient, while RGB requires conversion, see below explanation
        textureData = vsg::vec4Array2D::create(image_size, image_size);
        textureData->getLayout().format = VK_FORMAT_R32G32B32A32_SFLOAT;
        break;
    }

    // initialize the image
    UpdateImage updateImage;
    updateImage(textureData, 0.0);

    // setup graphics rendering subgraph
    auto scenegraph = vsg::StateGroup::create();

    vsg::Builder builder;

    if (heightfield)
    {
        // create height field use the texture data as a displacementMap
        stateInfo.displacementMap = textureData;
        scenegraph->addChild(builder.createHeightField(geomInfo, stateInfo));
    }
    else
    {
        // create textured box
        stateInfo.image = textureData;
        scenegraph->addChild(builder.createBox(geomInfo, stateInfo));
    }

    auto memoryBufferPools = vsg::MemoryBufferPools::create("Staging_MemoryBufferPool", vsg::ref_ptr<vsg::Device>(window->getOrCreateDevice()));
    vsg::ref_ptr<vsg::CopyAndReleaseImage> copyImageCmd = vsg::CopyAndReleaseImage::create(memoryBufferPools);

    // setup command graph to copy the image data each frame then rendering the scene graph
    {
        auto grahics_commandGraph = vsg::CommandGraph::create(window);
        grahics_commandGraph->addChild(copyImageCmd);
        grahics_commandGraph->addChild(vsg::createRenderGraphForView(window, camera, scenegraph));

        viewer->assignRecordAndSubmitTaskAndPresentation({grahics_commandGraph});
    }

    // compile the Vulkan objects
    viewer->compile();

    // texture has been filled in so it's now safe to get the ImageInfo that holds the handles to the texture's
    struct FindTexture : public vsg::Visitor
    {
        vsg::ref_ptr<vsg::ImageInfo> imageInfo;

        void apply(vsg::Object& object) override
        {
            object.traverse(*this);
        }
        void apply(vsg::StateGroup& sg) override
        {
            for (auto& sc : sg.stateCommands) { sc->accept(*this); }
            sg.traverse(*this);
        }
        void apply(vsg::DescriptorImage& di) override
        {
            if (!di.imageInfoList.empty()) imageInfo = di.imageInfoList[0]; // contextID=0, and only one imageData
        }

        static vsg::ref_ptr<vsg::ImageInfo> find(vsg::Node* node)
        {
            FindTexture fdi;
            node->accept(fdi);
            return fdi.imageInfo;
        }
    };

    auto textureImageInfo = FindTexture::find(scenegraph);

    if (!textureImageInfo || !textureImageInfo->imageView)
    {
        std::cout << "Can not locate imageInfo to update." << std::endl;
        return 1;
    }

    auto startTime = vsg::clock::now();
    double numFramesCompleted = 0.0;

    // main frame loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        double time = std::chrono::duration<double, std::chrono::seconds::period>(viewer->getFrameStamp()->time - viewer->start_point()).count();

        // update texture data
        updateImage(textureData, time);

        // copy data to staging buffer and issue a copy command to transfer to the GPU texture image
        copyImageCmd->copy(textureData, textureImageInfo);

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();

        numFramesCompleted += 1.0;
    }

    auto duration = std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - startTime).count();
    if (numFramesCompleted > 0.0)
    {
        std::cout << "Average frame rate = " << (numFramesCompleted / duration) << std::endl;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
