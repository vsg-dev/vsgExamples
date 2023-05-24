#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <vsgImGui/RenderImGui.h>
#include <vsgImGui/SendEventsToImGui.h>
#include <vsgImGui/imgui.h>

class MyGui : public vsg::Inherit<vsg::Command, MyGui>
{
public:
    vsg::ref_ptr<vsg::SetLineWidth> setLineWidth;

    MyGui(vsg::ref_ptr<vsg::SetLineWidth> in_setLineWidth) :
        setLineWidth(in_setLineWidth)
    {
    }

    // Example here taken from the Dear imgui comments (mostly)
    void record(vsg::CommandBuffer&) const override
    {
        ImGui::Begin("vsg dynamic state");
        ImGui::SliderFloat("Line Width", &(setLineWidth->lineWidth), 1, 5);
        ImGui::End();
    }
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);

    vsg::ref_ptr<vsg::ShaderSet> shaderSet;
    if (arguments.read("--pbr")) shaderSet = vsg::createPhysicsBasedRenderingShaderSet(options);
    if (arguments.read("--phong")) shaderSet = vsg::createPhongShaderSet(options);
    if (arguments.read("--flat")) shaderSet = vsg::createFlatShadedShaderSet(options);
    if (vsg::Path shaderSetFile; arguments.read("-s", shaderSetFile))
    {
        shaderSet = vsg::read_cast<vsg::ShaderSet>(shaderSetFile, options);
        std::cout << "Read ShaderSet file " << shaderSet << std::endl;
    }

    auto textureFile = arguments.value<vsg::Path>("", "-t");
    auto outputFile = arguments.value<vsg::Path>("", "-o");
    auto outputShaderSetFile = arguments.value<vsg::Path>("", "--os");
    auto share = arguments.read("--share");

    if (!shaderSet) shaderSet = vsg::createPhongShaderSet(options);

    // enable wireframe mode to visualize line width
    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->polygonMode = VK_POLYGON_MODE_LINE;
    shaderSet->defaultGraphicsPipelineStates.push_back(rasterizationState);

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // no ShaderSet loaded so fallback to create function.
    //if (!shaderSet) shaderSet = vsg::createFlatShadedShaderSet(options);

    if (!shaderSet)
    {
        std::cout << "Could not create shaders." << std::endl;
        return 1;
    }

    auto sharedObjects = vsg::SharedObjects::create_if(share);

    vsg::dvec3 position{0.0, 0.0, 0.0};
    vsg::dvec3 delta_column{2.0, 0.0, 0.0};
    vsg::dvec3 delta_row{0.0, 2.0, 0.0};

    auto scenegraph = vsg::Group::create();

    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(shaderSet);

    // instantiate dynamicstate and add the state
    graphicsPipelineConfig->dynamicState = vsg::DynamicState::create();
    graphicsPipelineConfig->dynamicState->dynamicStates.emplace_back(VK_DYNAMIC_STATE_LINE_WIDTH);

    // set up graphics pipeline
    vsg::Descriptors descriptors;

    // read texture image
    if (textureFile)
    {
        auto textureData = vsg::read_cast<vsg::Data>(textureFile, options);
        if (!textureData)
        {
            std::cout << "Could not read texture file : " << textureFile << std::endl;
            return 1;
        }

        // enable texturing
        graphicsPipelineConfig->assignTexture(descriptors, "diffuseMap", textureData);
    }

    // set up pass of material
    auto mat = vsg::PhongMaterialValue::create();
    mat->value().diffuse.set(1.0f, 1.0f, 1.0f, 1.0f);
    mat->value().specular.set(1.0f, 0.0f, 0.0f, 1.0f); // red specular highlight

    graphicsPipelineConfig->assignUniform(descriptors, "material", mat);

    if (sharedObjects) sharedObjects->share(descriptors);

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
        {{-0.5f, -0.5f, 0.0f},
         {0.5f, -0.5f, 0.0f},
         {0.5f, 0.5f, 0.0f},
         {-0.5f, 0.5f, 0.0f},
         {-0.5f, -0.5f, -0.5f},
         {0.5f, -0.5f, -0.5f},
         {0.5f, 0.5f, -0.5f},
         {-0.5f, 0.5f, -0.5f}});

    auto normals = vsg::vec3Array::create(
        {{0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f}});

    auto texcoords = vsg::vec2Array::create(
        {{0.0f, 0.0f},
         {1.0f, 0.0f},
         {1.0f, 1.0f},
         {0.0f, 1.0f},
         {0.0f, 0.0f},
         {1.0f, 0.0f},
         {1.0f, 1.0f},
         {0.0f, 1.0f}});

    auto colors = vsg::vec4Value::create(vsg::vec4{1.0f, 1.0f, 1.0f, 1.0f});

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
         2, 3, 0,
         4, 5, 6,
         6, 7, 4});

    vsg::DataList vertexArrays;

    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, normals);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, texcoords);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, colors);

    if (sharedObjects) sharedObjects->share(vertexArrays);
    if (sharedObjects) sharedObjects->share(indices);

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(12, 1, 0, 0, 0));
    // get the reference and bind it to ImGui slider
    auto setLineWidth = vsg::SetLineWidth::create(1.0f);
    drawCommands->addChild(setLineWidth);

    if (sharedObjects)
    {
        sharedObjects->share(drawCommands->children);
        sharedObjects->share(drawCommands);
    }

    // register the ViewDescriptorSetLayout.
    vsg::ref_ptr<vsg::ViewDescriptorSetLayout> vdsl;
    if (sharedObjects)
        vdsl = sharedObjects->shared_default<vsg::ViewDescriptorSetLayout>();
    else
        vdsl = vsg::ViewDescriptorSetLayout::create();
    graphicsPipelineConfig->additionalDescriptorSetLayout = vdsl;

    // share the pipeline config and initilaize if it's unique
    if (sharedObjects)
        sharedObjects->share(graphicsPipelineConfig, [](auto gpc) { gpc->init(); });
    else
        graphicsPipelineConfig->init();

    auto descriptorSet = vsg::DescriptorSet::create(graphicsPipelineConfig->descriptorSetLayout, descriptors);
    if (sharedObjects) sharedObjects->share(descriptorSet);

    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineConfig->layout, 0, descriptorSet);
    if (sharedObjects) sharedObjects->share(bindDescriptorSet);

    auto bindViewDescriptorSets = vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineConfig->layout, 1);
    if (sharedObjects) sharedObjects->share(bindViewDescriptorSets);

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto stateGroup = vsg::StateGroup::create();
    stateGroup->add(graphicsPipelineConfig->bindGraphicsPipeline);
    stateGroup->add(bindDescriptorSet);
    stateGroup->add(bindViewDescriptorSets);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create();
    transform->subgraphRequiresLocalFrustum = false;
    //auto transform = vsg::MatrixTransform::create(vsg::translate(position));

#if 0
            // add drawCommands to transform
            transform->addChild(drawCommands);

            if (sharedObjects)
            {
                transform = sharedObjects->share(transform);
            }

            // add transform to root of the scene graph
            stateGroup->addChild(transform);
            if (sharedObjects)
            {
                stateGroup = sharedObjects->share(stateGroup);
            }

            scenegraph->addChild(stateGroup);
#else
    // add drawCommands to StateGroup
    stateGroup->addChild(drawCommands);
    if (sharedObjects)
    {
        sharedObjects->share(stateGroup);
    }

    transform->addChild(stateGroup);

    if (sharedObjects)
    {
        sharedObjects->share(transform);
    }

    scenegraph->addChild(transform);
#endif

    if (sharedObjects) sharedObjects->report(std::cout);

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6 * 3.0;

    std::cout << "centre = " << centre << std::endl;
    std::cout << "radius = " << radius << std::endl;

    // camera related details
    double nearFarRatio = 0.001;
    auto viewport = vsg::ViewportState::create(0, 0, window->extent2D().width, window->extent2D().height);
    auto perspective = vsg::Perspective::create(60.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.0);
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    auto commandGraph = vsg::CommandGraph::create(window);
    auto renderGraph = vsg::RenderGraph::create(window);
    commandGraph->addChild(renderGraph);

    auto view = vsg::View::create(camera);
    view->addChild(vsg::createHeadlight());
    view->addChild(scenegraph);

    renderGraph->addChild(view);

    if (outputShaderSetFile)
    {
        vsg::write(shaderSet, outputShaderSetFile, options);
        return 0;
    }

    if (outputFile)
    {
        vsg::write(scenegraph, outputFile, options);
        return 0;
    }

    // add vsgImGui to implement a slider to control line width
    auto renderImGui = vsgImGui::RenderImGui::create(window, MyGui::create(setLineWidth));
    renderGraph->addChild(renderImGui);

    // Add the ImGui event handler first to handle events early
    viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    viewer->addEventHandler(vsg::Trackball::create(camera));

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // compile the Vulkan objects
    viewer->compile();

    // main frame loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        // animate the transform
        //float time = std::chrono::duration<float, std::chrono::seconds::period>(viewer->getFrameStamp()->time - viewer->start_point()).count();
        //transform->matrix = vsg::rotate(time * vsg::radians(90.0f), vsg::vec3(0.0f, 0.0, 1.0f));

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
