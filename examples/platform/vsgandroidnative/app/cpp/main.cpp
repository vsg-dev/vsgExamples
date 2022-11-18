
#include <initializer_list>
//#include <memory>
#include <cstdlib>
#include <cstring>
#include <jni.h>

#include <android/log.h>
#include <android_native_app_glue.h>

#include <vsg/all.h>
#include <vsg/platform/Android/Android_Window.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "vsgnative", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "vsgnative", __VA_ARGS__))

//
// App state
//
struct AppData
{
    struct android_app* app;

    vsg::ref_ptr<vsg::WindowTraits> traits;
    vsg::ref_ptr<vsg::Viewer> viewer;
    vsg::ref_ptr<vsgAndroid::Android_Window> window;

    vsg::BufferDataList uniformBufferData;
    vsg::ref_ptr<vsg::mat4Value> projMatrix;
    vsg::ref_ptr<vsg::mat4Value> viewMatrix;
    vsg::ref_ptr<vsg::mat4Value> modelMatrix;

    bool animate;

    float time;
};

//
// Init the vsg viewer and load assets
//
static int vsg_init(struct AppData* appData)
{
    appData->animate = true;
    appData->time = 0.0f;

    appData->viewer = vsg::Viewer::create();

    // setup traits
    appData->traits = new vsg::Window::Traits();
    appData->traits->width = ANativeWindow_getWidth(appData->app->window);
    appData->traits->height = ANativeWindow_getHeight(appData->app->window);

    // attach the apps native window to the traits, vsg will create a surface inside this.
    //traits.nativeHandle = engine->app->window;
    appData->traits->nativeWindow = appData->app->window;

    // create a window using the ANativeWindow passed via traits
    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(appData->traits));
    if (!window)
    {
        LOGW("Error: Could not create window a VSG window.");
        return 1;
    }

    // get the width/height
    uint32_t width = appData->window->extent2D().width;
    uint32_t height = appData->window->extent2D().height;

    // cast the window to an android window so we can pass it events
    appData->window = static_cast<vsgAndroid::Android_Window*>(window.get());

    // attach the window to the viewer
    appData->viewer->addWindow(window);


    // set search paths to try find data folder in the devices Download folder
    vsg::Paths searchPaths = { "/storage/emulated/0/Download/data", "/sdcard/Download/data" };

    // load vertex and fragments shaders located in the Download/data/shaders folder
    vsg::ref_ptr<vsg::Shader> vertexShader = vsg::Shader::read( VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert.spv", searchPaths));
    vsg::ref_ptr<vsg::Shader> fragmentShader = vsg::Shader::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        LOGW("Error: Failed to load shaders, ensure the shaders are copied to the devices Download/data/shaders folder.");
        return 1;
    }

    // read texture
    vsg::vsgReaderWriter vsgReader;
    auto textureData = vsgReader.read<vsg::Data>(vsg::findFile("textures/lz.vsgb", searchPaths));
    if (!textureData)
    {
        LOGW("Error: Failed to load Texture data for 'textures/lz.vsgb'. Ensure lz.vsgb has been copied to the devices Download/data/textures folder");
        return 1;
    }

    //
    // create high level Vulkan objects associated the main window

    vsg::ref_ptr<vsg::PhysicalDevice> physicalDevice(window->physicalDevice());
    vsg::ref_ptr<vsg::Device> device(window->device());
    vsg::ref_ptr<vsg::Surface> surface(window->surface());
    vsg::ref_ptr<vsg::RenderPass> renderPass(window->renderPass());

    VkQueue graphicsQueue = device->getQueue(physicalDevice->getGraphicsFamily());
    VkQueue presentQueue = device->getQueue(physicalDevice->getPresentFamily());
    if (!graphicsQueue || !presentQueue)
    {
        LOGW("Required graphics/present queue not available!");
        return 1;
    }

    // create command pool
    vsg::ref_ptr<vsg::CommandPool> commandPool = vsg::CommandPool::create(device, physicalDevice->getGraphicsFamily());

    // set up vertex and index arrays
    vsg::ref_ptr<vsg::vec3Array> vertices(new vsg::vec3Array
            {
                    {-0.5f, -0.5f, 0.0f},
                    {0.5f,  -0.5f, 0.05f},
                    {0.5f , 0.5f, 0.0f},
                    {-0.5f, 0.5f, 0.0f},
                    {-0.5f, -0.5f, -0.5f},
                    {0.5f,  -0.5f, -0.5f},
                    {0.5f , 0.5f, -0.5},
                    {-0.5f, 0.5f, -0.5}
            });

    vsg::ref_ptr<vsg::vec3Array> colors(new vsg::vec3Array
            {
                    {1.0f, 0.0f, 0.0f},
                    {0.0f, 1.0f, 0.0f},
                    {0.0f, 0.0f, 1.0f},
                    {1.0f, 1.0f, 1.0f},
                    {1.0f, 0.0f, 0.0f},
                    {0.0f, 1.0f, 0.0f},
                    {0.0f, 0.0f, 1.0f},
                    {1.0f, 1.0f, 1.0f},
            });

    vsg::ref_ptr<vsg::vec2Array> texcoords(new vsg::vec2Array
            {
                    {0.0f, 0.0f},
                    {1.0f, 0.0f},
                    {1.0f, 1.0f},
                    {0.0f, 1.0f},
                    {0.0f, 0.0f},
                    {1.0f, 0.0f},
                    {1.0f, 1.0f},
                    {0.0f, 1.0f}
            });

    vsg::ref_ptr<vsg::ushortArray> indices(new vsg::ushortArray
            {
                    0, 1, 2,
                    2, 3, 0,
                    4, 5, 6,
                    6, 7, 4
            });

    auto vertexBufferData = vsg::createBufferAndTransferData(device, commandPool, graphicsQueue, vsg::DataList{vertices, colors, texcoords}, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
    auto indexBufferData = vsg::createBufferAndTransferData(device, commandPool, graphicsQueue, {indices}, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);

    // set up uniforms, keep these is our appDats structure so we can update them
    appData->projMatrix = new vsg::mat4Value;
    appData->viewMatrix = new vsg::mat4Value;
    appData->modelMatrix = new vsg::mat4Value;

    appData->uniformBufferData = vsg::createHostVisibleBuffer(device, {appData->projMatrix, appData->viewMatrix, appData->modelMatrix}, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);

    //
    // set up texture image
    //
    vsg::ImageData imageData = vsg::transferImageData(device, commandPool, graphicsQueue, textureData);
    if (!imageData.valid())
    {
        LOGW("Error: Failed to create texture.");
        return 1;
    }

    //
    // set up descriptor layout and descriptor set and pipeline layout for uniforms
    //
    vsg::ref_ptr<vsg::DescriptorPool> descriptorPool = vsg::DescriptorPool::create(device, 1,
    {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    });

    vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout = vsg::DescriptorSetLayout::create(device,
    {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    });

    vsg::ref_ptr<vsg::DescriptorSet> descriptorSet = vsg::DescriptorSet::create(device, descriptorPool, descriptorSetLayout,
    {
            vsg::DescriptorBuffer::create(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, appData->uniformBufferData),
            vsg::DescriptorImage::create(3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vsg::ImageDataList{imageData})
    });

    vsg::ref_ptr<vsg::PipelineLayout> pipelineLayout = vsg::PipelineLayout::create(device, {descriptorSetLayout}, {});

    // setup binding of descriptors
    vsg::ref_ptr<vsg::BindDescriptorSets> bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, vsg::DescriptorSets{descriptorSet}); // device dependent


    // set up graphics pipeline
    vsg::VertexInputState::Bindings vertexBindingsDescriptions
    {
            VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX},
            VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX},
            VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions
    {
            VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
            VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0},
            VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},
    };

    vsg::ref_ptr<vsg::ShaderStages> shaderStages = vsg::ShaderStages::create(vsg::ShaderModules
    {
            vsg::ShaderModule::create(device, vertexShader),
            vsg::ShaderModule::create(device, fragmentShader)
    });

    vsg::ref_ptr<vsg::GraphicsPipeline> pipeline = vsg::GraphicsPipeline::create(device, renderPass, pipelineLayout, // device dependent
    {
            shaderStages,  // device dependent
            vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),// device independent
            vsg::InputAssemblyState::create(), // device independent
            vsg::ViewportState::create(VkExtent2D{width, height}), // device independent
            vsg::RasterizationState::create(),// device independent
            vsg::MultisampleState::create(),// device independent
            vsg::ColorBlendState::create(),// device independent
            vsg::DepthStencilState::create()// device independent
    });

    vsg::ref_ptr<vsg::BindPipeline> bindPipeline = vsg::BindPipeline::create(pipeline);

    // set up vertex buffer binding
    vsg::ref_ptr<vsg::BindVertexBuffers> bindVertexBuffers = vsg::BindVertexBuffers::create(0, vertexBufferData);  // device dependent

    // set up index buffer binding
    vsg::ref_ptr<vsg::BindIndexBuffer> bindIndexBuffer = vsg::BindIndexBuffer::create(indexBufferData.front(), VK_INDEX_TYPE_UINT16); // device dependent

    // set up drawing of the triangles
    vsg::ref_ptr<vsg::DrawIndexed> drawIndexed = vsg::DrawIndexed::create(12, 1, 0, 0, 0); // device independent

    // set up what we want to render in a command graph
    // create command graph to contain all the Vulkan calls for specifically rendering the model
    vsg::ref_ptr<vsg::Group> commandGraph = vsg::Group::create();

    vsg::ref_ptr<vsg::StateGroup> stateGroup = vsg::StateGroup::create();
    commandGraph->addChild(stateGroup);

    // set up the state configuration
    stateGroup->add(bindPipeline);  // device dependent
    stateGroup->add(bindDescriptorSets);  // device dependent

    // add subgraph that represents the model to render
    vsg::ref_ptr<vsg::StateGroup> model = vsg::StateGroup::create();
    stateGroup->addChild(model);

    // add the vertex and index buffer data
    model->add(bindVertexBuffers); // device dependent
    model->add(bindIndexBuffer); // device dependent

    // add the draw primitive command
    model->addChild(drawIndexed); // device independent

    for (auto& win : appData->viewer->windows())
    {
        // add a GraphicsStage to the Window to do dispatch of the command graph to the command buffer(s)
        win->addStage(vsg::GraphicsStage::create(commandGraph));
        win->populateCommandBuffers();
    }
    return 0;
}

static void vsg_term(struct AppData* appData)
{
    appData->viewer = nullptr;
    appData->window = nullptr;
    appData->modelMatrix = nullptr;
    appData->viewMatrix = nullptr;
    appData->projMatrix = nullptr;
    appData->uniformBufferData.clear();
    appData->animate = false;
}

//
// Render a frame
//
static void vsg_frame(struct AppData* appData)
{
    if (!appData->viewer.valid()) {
        // no viewer.
        return;
    }

    // poll events and advance frame counters
    viewer->advance();

    // pass any events into EventHandlers assigned to the Viewer
    viewer->handleEvents();

    appData->time = appData->time + 0.033f;
    //float previousTime = engine->time;
    //engine->time = std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::steady_clock::now()-startTime).count();
    //if (printFrameRate) std::cout<<"time = "<<time<<" fps="<<1.0/(time-previousTime)<<std::endl;

    // update
    uint32_t width = appData->window->extent2D().width;
    uint32_t height = appData->window->extent2D().height;

    (*appData->projMatrix) = vsg::perspective(vsg::radians(45.0f), float(width)/float(height), 0.1f, 10.f);
    (*appData->viewMatrix) = vsg::lookAt(vsg::vec3(2.0f, 2.0f, 2.0f), vsg::vec3(0.0f, 0.0f, 0.0f), vsg::vec3(0.0f, 0.0f, 1.0f));
    (*appData->modelMatrix) = vsg::rotate(appData->time * vsg::radians(90.0f), vsg::vec3(0.0f, 0.0, 1.0f));

    vsg::copyDataListToBuffers(appData->uniformBufferData);

    // render
    appData->viewer->submitFrame();
}

//
// Handle input event
//
static int32_t android_handleinput(struct android_app* app, AInputEvent* event)
{
    struct AppData* appData = (struct AppData*)app->userData;
    if(appData->window.valid()) return 0;
    // pass the event to the vsg android window
    return appData->window->handleAndroidInputEvent(event) ? 1 : 0;
}

//
// Handle main commands
//
static void android_handlecmd(struct android_app* app, int32_t cmd)
{
    struct AppData* appData = (struct AppData*)app->userData;
    if(appData == nullptr) return;

    switch (cmd)
    {
        case APP_CMD_SAVE_STATE:
            break;
        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
            if (app->window != NULL)
            {
                vsg_init(appData);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
            vsg_term(appData);
            break;
        case APP_CMD_GAINED_FOCUS:
            appData->animate = true;
            break;
        case APP_CMD_LOST_FOCUS:
            appData->animate = false;
            break;
    }
}

//
// Main entry point for the application
//
void android_main(struct android_app* app)
{
    struct AppData appData;

    memset(&appData, 0, sizeof(AppData));
    app->userData = &appData;
    app->onAppCmd = android_handlecmd;
    app->onInputEvent = android_handleinput;
    appData.app = app;

    // Used to poll the events in the main loop
    int events;
    android_poll_source* source;

    // main loop
    do
    {
        // poll events
        if (ALooper_pollAll(0, nullptr, &events, (void**)&source) >= 0)
        {
            if (source != NULL) source->process(app, source);
        }

        // render if vulkan is ready and we are animating
        if (appData.animate && appData.viewer.valid()) {
            vsg_frame(&appData);
        }
    } while (app->destroyRequested == 0);
}
