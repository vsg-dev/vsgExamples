
#include <initializer_list>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <jni.h>
#include <errno.h>
#include <cassert>

#include <android/log.h>
#include <android_native_app_glue.h>

#include <vsg/all.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__))

//
// App state
//
struct AppData
{
    struct android_app* app;

    vsg::ref_ptr<vsg::Viewer> viewer;

    vsg::BufferDataList uniformBufferData;
    vsg::ref_ptr<vsg::mat4Value> projMatrix;
    vsg::ref_ptr<vsg::mat4Value> viewMatrix;
    vsg::ref_ptr<vsg::mat4Value> modelMatrix;

    int32_t width;
    int32_t height;

    // touch coord
    int32_t x;
    int32_t y;

    float time;
};

//
// Init the vsg viewer and load assets
//
static int vsg_init(struct AppData* appData)
{
    appData->viewer = vsg::Viewer::create();

    appData->time = 0.0f;

    vsg::Window::Traits traits = {};
    traits.width = appData->width = ANativeWindow_getWidth(appData->app->window);
    traits.height = appData->height = ANativeWindow_getHeight(appData->app->window);

    //traits.nativeHandle = engine->app->window;
    traits.nativeWindow = appData->app->window;

    /*ANativeWindow** nativeWindow = std::any_cast<ANativeWindow*>(&traits.nativeHandle);

    if(nativeWindow != nullptr)
    {

    }*/

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(traits));
    if (!window)
    {
        LOGW("Could not create window.");
        return 1;
    }

    appData->viewer->addWindow(window);


    // read shaders
    vsg::Paths searchPaths = {"/storage/emulated/0/Download/data"};

    vsg::ref_ptr<vsg::Shader> vertexShader = vsg::Shader::read( VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert.spv", searchPaths));
    vsg::ref_ptr<vsg::Shader> fragmentShader = vsg::Shader::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout<<"Could not create shaders."<<std::endl;
        return 1;
    }

    // read texture
    vsg::vsgReaderWriter vsgReader;
    auto textureData = vsgReader.read<vsg::Data>(vsg::findFile("textures/lz.vsgb", searchPaths));
    if (!textureData)
    {
        std::cout<<"Could not read texture file : textures/lz.vsgb" <<std::endl;
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

    // set up uniforms
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
        std::cout<<"Texture not created"<<std::endl;
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
            vsg::ViewportState::create(VkExtent2D{traits.width, traits.height}), // device independent
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
        // add a GraphicsStage to the Window to do dispatch of the command graph to the commnad buffer(s)
        win->addStage(vsg::GraphicsStage::create(commandGraph));
        win->populateCommandBuffers();
    }
    return 0;

    return 0;
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

    appData->viewer->pollEvents();

    appData->time = appData->time + 0.033f;
    //float previousTime = engine->time;
    //engine->time = std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::steady_clock::now()-startTime).count();
    //if (printFrameRate) std::cout<<"time = "<<time<<" fps="<<1.0/(time-previousTime)<<std::endl;

    // update
    (*appData->projMatrix) = vsg::perspective(vsg::radians(45.0f), float(appData->width)/float(appData->height), 0.1f, 10.f);
    (*appData->viewMatrix) = vsg::lookAt(vsg::vec3(2.0f, 2.0f, 2.0f), vsg::vec3(0.0f, 0.0f, 0.0f), vsg::vec3(0.0f, 0.0f, 1.0f));
    (*appData->modelMatrix) = vsg::rotate(appData->time * vsg::radians(90.0f), vsg::vec3(0.0f, 0.0, 1.0f));

    vsg::copyDataListToBuffers(appData->uniformBufferData);

    appData->viewer->submitFrame();
}

//
// Handle input event
//
static int32_t android_handleinput(struct android_app* app, AInputEvent* event)
{
    struct AppData* appData = (struct AppData*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
    {
        appData->x = AMotionEvent_getX(event, 0);
        appData->y = AMotionEvent_getY(event, 0);
        return 1;
    }
    return 0;
}

//
// Handle main commands
//
static void android_handlecmd(struct android_app* app, int32_t cmd)
{
    struct AppData* appData = (struct AppData*)app->userData;
    switch (cmd)
    {
        case APP_CMD_SAVE_STATE:
            break;
        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
            if (appData->app->window != NULL)
            {
                vsg_init(appData);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
            break;
        case APP_CMD_GAINED_FOCUS:
            break;
        case APP_CMD_LOST_FOCUS:
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

        // render if vulkan if ready
        if (appData.viewer.valid()) {
            vsg_frame(&appData);
        }
    } while (app->destroyRequested == 0);
}