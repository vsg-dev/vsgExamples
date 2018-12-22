#include <vsg/all.h>

#include <vsg/viewer/Camera.h>
#include <vsg/ui/PrintEvents.h>

#include <vsg/viewer/CloseHandler.h>

#include <iostream>
#include <chrono>

namespace vsg
{
    template<typename T>
    constexpr t_mat4<T> lookAtInverse(t_vec3<T> const& eye, t_vec3<T> const& center, t_vec3<T> const& up)
    {
        using vec_type = t_vec3<T>;

        vec_type forward = normalize(center - eye);
        vec_type up_normal = normalize(up);
        vec_type side = normalize(cross(forward, up_normal));
        vec_type u = normalize(cross(side, forward));

        return translate(eye.x, eye.y, eye.z) *
            t_mat4<T>(side[0], u[0], -forward[0], 0,
                            side[1], u[1], -forward[1], 0,
                            side[2], u[2], -forward[2], 0,
                            0, 0, 0, 1);
    }

    class Trackball : public Inherit<Visitor, Trackball>
    {
    public:

        Trackball(ref_ptr<Camera> camera) :
            _camera(camera)
        {
            _lookAt = dynamic_cast<LookAt*>(_camera->getViewMatrix());

            if (!_lookAt)
            {
                // TODO: need to work out how to map the original ViewMatrix to a LookAt and back, for now just fallback to assigning our own LookAt
                _lookAt = new LookAt;
            }

            _homeLookAt = new LookAt(_lookAt->eye, _lookAt->center, _lookAt->up);
        }

        /// compute non dimensional window coordinate (-1,1) from event coords
        dvec2 ndc(PointerEvent& event)
        {
            dvec2 v = dvec2((static_cast<double>(event.x)/window_width)*2.0-1.0, (static_cast<double>(event.y)/window_height)*2.0-1.0);
            //std::cout<<"ndc = "<<v<<std::endl;
            return v;
        }

        /// compute trackball coordinate from event coords
        dvec3 tbc(PointerEvent& event)
        {
            dvec2 v = ndc(event);

            double l = length(v);
            if (l<1.0f)
            {
                double h = 0.5 + cos(l*PI)*0.5;
                return dvec3(v.x, -v.y, h);
            }
            else
            {
                return dvec3(v.x, -v.y, 0.0);
            }
        }

        void apply(KeyPressEvent& keyPress) override
        {
            if (keyPress.keyBase==_homeKey)
            {
                LookAt* lookAt = dynamic_cast<LookAt*>(_camera->getViewMatrix());
                if (lookAt && _homeLookAt)
                {
                    lookAt->eye = _homeLookAt->eye;
                    lookAt->center = _homeLookAt->center;
                    lookAt->up = _homeLookAt->up;
                }
            }
        }

        void apply(ExposeWindowEvent& exposeWindow) override
        {
            window_width = static_cast<double>(exposeWindow.width);
            window_height = static_cast<double>(exposeWindow.height);
        }

        void apply(ButtonPressEvent& buttonPress) override
        {
            prev_ndc = ndc(buttonPress);
            prev_tbc = tbc(buttonPress);

            if (buttonPress.button == 4)
            {
                zoom(-0.1);
            }
            else if (buttonPress.button == 5)
            {
                zoom(0.1);
            }
        };

        void apply(ButtonReleaseEvent& buttonRelease) override
        {
            prev_ndc = ndc(buttonRelease);
            prev_tbc = tbc(buttonRelease);
        };

        void apply(MoveEvent& moveEvent) override
        {
            LookAt* lookAt = dynamic_cast<LookAt*>(_camera->getViewMatrix());

            dvec2 new_ndc = ndc(moveEvent);
            dvec3 new_tbc = tbc(moveEvent);

            if (moveEvent.mask & BUTTON_MASK_1)
            {
                dvec3 xp = cross(normalize(new_tbc), normalize(prev_tbc));
                double xp_len = length(xp);
                if (xp_len>0.0)
                {
                    dvec3 axis = xp/xp_len;
                    double angle = asin(xp_len);
                    rotate(angle, axis);
                }
            }
            else if (moveEvent.mask & BUTTON_MASK_2)
            {
                dvec2 delta = new_ndc - prev_ndc;
                pan(delta);
            }
            else if (moveEvent.mask & BUTTON_MASK_3)
            {
                dvec2 delta = new_ndc - prev_ndc;
                zoom(delta.y);
            }

            prev_ndc = new_ndc;
            prev_tbc = new_tbc;
        };


        void rotate(double angle, const dvec3& axis)
        {
            dmat4 rotation = vsg::rotate(angle, axis);
            dmat4 lv = lookAt(_lookAt->eye, _lookAt->center, _lookAt->up);
            dmat4 lvInverse = lookAtInverse(_lookAt->eye, _lookAt->center, _lookAt->up);
            dvec3 centerEyeSpace = (lv * _lookAt->center);

            dmat4 matrix = lvInverse * translate(centerEyeSpace) * rotation * translate(-centerEyeSpace) * lv;

            _lookAt->up = normalize(matrix * (_lookAt->eye + _lookAt->up) - matrix * _lookAt->eye);
            _lookAt->center = matrix * _lookAt->center;
            _lookAt->eye = matrix * _lookAt->eye;
        }

        void zoom(double ratio)
        {
            dvec3 lookVector = _lookAt->center - _lookAt->eye;
            _lookAt->eye = _lookAt->eye + lookVector * ratio;
        }

        void pan(dvec2& delta)
        {
            dvec3 lookVector = _lookAt->center - _lookAt->eye;
            dvec3 lookNormal = normalize(lookVector);
            dvec3 sideNormal = cross(_lookAt->up, lookNormal);
            double distance = length(lookVector);
            dvec3 translation = sideNormal * (delta.x*distance) + _lookAt->up * (delta.y*distance);

            _lookAt->eye = _lookAt->eye + translation;
            _lookAt->center = _lookAt->center + translation;
        }

    protected:
        ref_ptr<Camera> _camera;
        ref_ptr<LookAt> _lookAt;
        ref_ptr<LookAt> _homeLookAt;

        KeySymbol _homeKey = KEY_Space;
        double _direction;

        double window_width = 800.0;
        double window_height = 600.0;
        dvec2 prev_ndc;
        dvec3 prev_tbc;
};

}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    auto debugLayer = arguments.read({"--debug","-d"});
    auto apiDumpLayer = arguments.read({"--api","-a"});
    auto numFrames = arguments.value(-1, "-f");
    auto printFrameRate = arguments.read("--fr");
    auto numWindows = arguments.value(1, "--num-windows");
    auto textureFile = arguments.value(std::string("textures/lz.vsgb"), "-t");
    auto [width, height] = arguments.value(std::pair<uint32_t, uint32_t>(800, 600), {"--window", "-w"});
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    vsg::ref_ptr<vsg::Shader> vertexShader = vsg::Shader::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    vsg::ref_ptr<vsg::Shader> fragmentShader = vsg::Shader::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout<<"Could not create shaders."<<std::endl;
        return 1;
    }

    vsg::vsgReaderWriter vsgReader;
    auto textureData = vsgReader.read<vsg::Data>(vsg::findFile(textureFile, searchPaths));
    if (!textureData)
    {
        std::cout<<"Could not read texture file : "<<textureFile<<std::endl;
        return 1;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(width, height, debugLayer, apiDumpLayer));
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);

    for(int i=1; i<numWindows; ++i)
    {
        vsg::ref_ptr<vsg::Window> new_window(vsg::Window::create(width, height, debugLayer, apiDumpLayer, window));
        viewer->addWindow( new_window );
    }

    // create high level Vulkan objects associated the main window
    vsg::ref_ptr<vsg::PhysicalDevice> physicalDevice(window->physicalDevice());
    vsg::ref_ptr<vsg::Device> device(window->device());
    vsg::ref_ptr<vsg::Surface> surface(window->surface());
    vsg::ref_ptr<vsg::RenderPass> renderPass(window->renderPass());


    VkQueue graphicsQueue = device->getQueue(physicalDevice->getGraphicsFamily());
    VkQueue presentQueue = device->getQueue(physicalDevice->getPresentFamily());
    if (!graphicsQueue || !presentQueue)
    {
        std::cout<<"Required graphics/present queue not available!"<<std::endl;
        return 1;
    }

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


    //
    // set up texture image
    //
    vsg::ImageData imageData = vsg::transferImageData(device, commandPool, graphicsQueue, textureData);
    if (!imageData.valid())
    {
        std::cout<<"Texture not created"<<std::endl;
        return 1;
    }

    vsg::ref_ptr<vsg::mat4Value> projMatrix(new vsg::mat4Value);
    vsg::ref_ptr<vsg::mat4Value> viewMatrix(new vsg::mat4Value);
    vsg::ref_ptr<vsg::mat4Value> modelMatrix(new vsg::mat4Value);

    //
    // set up descriptor layout and descriptor set and pieline layout for uniforms
    //
    vsg::ref_ptr<vsg::DescriptorPool> descriptorPool = vsg::DescriptorPool::create(device, 1,
    {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    });

    vsg::ref_ptr<vsg::PushConstants> pushConstant_proj = vsg::PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 0, projMatrix);
    vsg::ref_ptr<vsg::PushConstants> pushConstant_view = vsg::PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 64, viewMatrix);
    vsg::ref_ptr<vsg::PushConstants> pushConstant_model = vsg::PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 128, modelMatrix);

    vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout = vsg::DescriptorSetLayout::create(device,
    {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    });

    vsg::PushConstantRanges pushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 196}
    };

    vsg::ref_ptr<vsg::DescriptorSet> descriptorSet = vsg::DescriptorSet::create(device, descriptorPool, descriptorSetLayout,
    {
        vsg::DescriptorImage::create(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vsg::ImageDataList{imageData})
    });

    vsg::ref_ptr<vsg::PipelineLayout> pipelineLayout = vsg::PipelineLayout::create(device, {descriptorSetLayout}, pushConstantRanges);


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

    auto viewport = vsg::ViewportState::create(VkExtent2D{width, height});

    vsg::ref_ptr<vsg::GraphicsPipeline> pipeline = vsg::GraphicsPipeline::create(device, renderPass, pipelineLayout, // device dependent
    {
        shaderStages,  // device dependent
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),// device independent
        vsg::InputAssemblyState::create(), // device independent
        viewport, // device independent
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
    vsg::ref_ptr<vsg::StateGroup> commandGraph = vsg::StateGroup::create();

    // set up the state configuration
    commandGraph->add(bindPipeline);  // device dependent
    commandGraph->add(bindDescriptorSets);  // device dependent

    commandGraph->add(pushConstant_proj);
    commandGraph->add(pushConstant_view);
    commandGraph->add(pushConstant_model);

    // add subgraph that represents the model to render
    vsg::ref_ptr<vsg::StateGroup> model = vsg::StateGroup::create();
    commandGraph->addChild(model);

    // add the vertex and index buffer data
    model->add(bindVertexBuffers); // device dependent
    model->add(bindIndexBuffer); // device dependent

    // add the draw primitive command
    model->addChild(drawIndexed); // device independent

    //
    // end of initialize vulkan
    //
    /////////////////////////////////////////////////////////////////////

    auto startTime =std::chrono::steady_clock::now();

    for (auto& win : viewer->windows())
    {
        // add a GraphicsStage tp the Window to do dispatch of the command graph to the commnad buffer(s)
        win->addStage(vsg::GraphicsStage::create(commandGraph));
        win->populateCommandBuffers();
    }


    vsg::ref_ptr<vsg::Perspective> perspective(new vsg::Perspective(60.0, static_cast<double>(width) / static_cast<double>(height), 0.1, 10.0));
    vsg::ref_ptr<vsg::LookAt> lookAt(new vsg::LookAt(vsg::dvec3(1.0, 1.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0)));
    vsg::ref_ptr<vsg::Camera> camera(new vsg::Camera(perspective, lookAt));

    // assign a Trackball and CloseHandler to the Viewer to respond to events
    auto trackball = vsg::Trackball::create(camera);
    viewer->addEventHandlers({trackball, vsg::CloseHandler::create(viewer)});

    bool windowResized = false;
    float time = 0.0f;

    while (viewer->active() && (numFrames<0 || (numFrames--)>0))
    {
        // poll events and advance frame counters
        viewer->advance();

        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        float previousTime = time;
        time = std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::steady_clock::now()-viewer->start_point()).count();
        if (printFrameRate) std::cout<<"time = "<<time<<" fps="<<1.0/(time-previousTime)<<std::endl;

        camera->getProjectionMatrix()->get((*projMatrix));
        camera->getViewMatrix()->get((*viewMatrix));

        //(*projMatrix) = vsg::perspective(vsg::radians(45.0f), float(width)/float(height), 0.1f, 10.f);
        //(*viewMatrix) = vsg::lookAt(vsg::vec3(2.0f, 2.0f, 2.0f), vsg::vec3(0.0f, 0.0f, 0.0f), vsg::vec3(0.0f, 0.0f, 1.0f));

        //(*modelMatrix) = vsg::rotate(time * vsg::radians(90.0f), vsg::vec3(0.0f, 0.0, 1.0f));

        for (auto& win : viewer->windows())
        {
            // we need to regenerate the CommandBuffer so that the PushConstants get called with the new values.
            win->populateCommandBuffers();
        }

        if (window->resized()) windowResized = true;

        viewer->submitFrame();

        if (windowResized)
        {
            windowResized = false;

            auto windowExtent = window->extent2D();
            viewport->getViewport().width = static_cast<float>(windowExtent.width);
            viewport->getViewport().height = static_cast<float>(windowExtent.height);
            viewport->getScissor().extent = windowExtent;

            vsg::ref_ptr<vsg::GraphicsPipeline> new_pipeline = vsg::GraphicsPipeline::create(device, renderPass, pipelineLayout, pipeline->getPipelineStates());

            bindPipeline->setPipeline(new_pipeline);

            pipeline = new_pipeline;

            perspective->aspectRatio = static_cast<double>(windowExtent.width) / static_cast<double>(windowExtent.height);

            std::cout<<"window aspect ratio = "<<perspective->aspectRatio<<std::endl;
        }
    }


    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
