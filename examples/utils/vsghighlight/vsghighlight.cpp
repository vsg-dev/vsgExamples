#include <iostream>

#include <vsg/all.h>

namespace
{
    vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> buildPipelineConfig(vsg::ref_ptr<const vsg::Options> options)
    {
        auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/highlight.vert", options);
        auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/highlight.frag", options);
        if (!vertexShader || !fragmentShader)
        {
            vsg::error("No highlight shaders found");
            return {};
        }

        //setup highlight/selection colors
        auto slcColors = vsg::vec4Array::create(3);
        slcColors->at(0) = vsg::vec4(0.0, 0.0, 0.0, 1.0); //dummy to sync index to use material.
        slcColors->at(1) = vsg::vec4(1.0, 1.0, 0.0, 1.0); //pre highlight
        slcColors->at(2) = vsg::vec4(1.0, 1.0, 1.0, 1.0); //highlight

        // set up shaders
        auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);
        shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
        shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
        shaderSet->addDescriptorBinding("slcColors", "", 0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, slcColors, vsg::CoordinateSpace::LINEAR);
        shaderSet->addDescriptorBinding("lightData", "", 1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));
        shaderSet->addDescriptorBinding("material", "", 2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PhongMaterialValue::create(), vsg::CoordinateSpace::LINEAR);
        shaderSet->addDescriptorBinding("slcIndex", "", 3, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::uintValue::create(0));
        shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(1));

        // set up pipeline
        auto pipeConfig = vsg::GraphicsPipelineConfigurator::create(shaderSet);
        pipeConfig->enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        pipeConfig->enableArray("vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        pipeConfig->enableDescriptor("slcColors");
        pipeConfig->enableDescriptor("lightData");
        pipeConfig->enableDescriptor("material");
        pipeConfig->enableDescriptor("slcIndex");
        pipeConfig->init();
        return pipeConfig;
    }

    struct FaceBuilder
    {
        const vsg::GraphicsPipelineConfigurator& plConfig;

        FaceBuilder() = delete;
        FaceBuilder(const vsg::GraphicsPipelineConfigurator& plConfigIn) :
            plConfig(plConfigIn) {}
        vsg::ref_ptr<vsg::StateGroup> build(const std::array<vsg::dvec3, 4>& pointsIn, const vsg::dvec3& normalIn, const vsg::vec4& colorIn, const std::string& nameIn)
        {
            auto out = vsg::StateGroup::create();

            auto material = vsg::PhongMaterialValue::create();
            material->value().diffuse = colorIn;
            auto materialDescriptor = vsg::DescriptorBuffer::create(material);
            auto materialSet = vsg::DescriptorSet::create(plConfig.layout->setLayouts.at(2), vsg::Descriptors{materialDescriptor});
            auto materialBind = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, plConfig.layout, 2, materialSet);
            out->addChild(materialBind);

            auto slcIndex = vsg::uintValue::create(0);
            auto slcDescriptor = vsg::DescriptorBuffer::create(slcIndex);
            auto slcSet = vsg::DescriptorSet::create(plConfig.layout->setLayouts.at(3), vsg::Descriptors{slcDescriptor});
            auto slcBind = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, plConfig.layout, 3, slcSet);
            out->addChild(slcBind);

            auto draw = vsg::VertexIndexDraw::create();
            auto points = vsg::vec3Array::create(4);
            for (int index = 0; index < 4; ++index) points->at(index) = pointsIn.at(index);
            auto normals = vsg::vec3Array::create(4, vsg::vec3(normalIn));
            auto indexes = vsg::ushortArray::create({0, 1, 2, 2, 3, 0});
            draw->assignArrays(vsg::DataList{points, normals});
            draw->assignIndices(indexes);
            draw->indexCount = indexes->size();
            draw->instanceCount = 1;
            out->addChild(draw);

            out->setObject("slcIndexBuffer", slcDescriptor);
            out->setValue<std::string>("name", nameIn);

            return out;
        }
    };

    vsg::ref_ptr<vsg::Group> buildBox(const vsg::GraphicsPipelineConfigurator& plConfigIn)
    {
        auto out = vsg::Group::create();
        FaceBuilder builder(plConfigIn);

        double length = 10.0, width = 10.0, height = 10.0;
        std::array<vsg::dvec3, 8> ps =
            {{{0.0, 0.0, 0.0}, {length, 0.0, 0.0}, {length, width, 0.0}, {0.0, width, 0.0}, {0.0, 0.0, height}, {length, 0.0, height}, {length, width, height}, {0.0, width, height}}};
        out->addChild(builder.build(std::array<vsg::dvec3, 4>{{ps.at(0), ps.at(3), ps.at(2), ps.at(1)}}, vsg::dvec3(0.0, 0.0, -1.0), vsg::vec4(0.0, 0.0, 1.0, 1.0), "bottom"));
        out->addChild(builder.build(std::array<vsg::dvec3, 4>{{ps.at(0), ps.at(1), ps.at(5), ps.at(4)}}, vsg::dvec3(0.0, -1.0, 0.0), vsg::vec4(0.0, 1.0, 0.0, 1.0), "front"));
        out->addChild(builder.build(std::array<vsg::dvec3, 4>{{ps.at(1), ps.at(2), ps.at(6), ps.at(5)}}, vsg::dvec3(1.0, 0.0, 0.0), vsg::vec4(1.0, 0.0, 0.0, 1.0), "right"));
        out->addChild(builder.build(std::array<vsg::dvec3, 4>{{ps.at(2), ps.at(3), ps.at(7), ps.at(6)}}, vsg::dvec3(0.0, 1.0, 0.0), vsg::vec4(0.0, 1.0, 0.0, 1.0), "back"));
        out->addChild(builder.build(std::array<vsg::dvec3, 4>{{ps.at(3), ps.at(0), ps.at(4), ps.at(7)}}, vsg::dvec3(-1.0, 0.0, 0.0), vsg::vec4(1.0, 0.0, 0.0, 1.0), "left"));
        out->addChild(builder.build(std::array<vsg::dvec3, 4>{{ps.at(4), ps.at(5), ps.at(6), ps.at(7)}}, vsg::dvec3(0.0, 0.0, 1.0), vsg::vec4(0.0, 0.0, 1.0, 1.0), "top"));

        return out;
    }

    class IntersectionHandler : public vsg::Inherit<vsg::Visitor, IntersectionHandler>
    {
    public:
        vsg::ref_ptr<vsg::Camera> camera;
        vsg::ref_ptr<vsg::Group> scenegraph;
        vsg::ref_ptr<vsg::Viewer> viewer;
        vsg::ref_ptr<vsg::StateGroup> preSelection;
        std::vector<vsg::ref_ptr<vsg::StateGroup>> selections;

        IntersectionHandler(vsg::ref_ptr<vsg::Camera> camIn, vsg::ref_ptr<vsg::Group> sceneIn, vsg::ref_ptr<vsg::Viewer> viewerIn) :
            camera(camIn),
            scenegraph(sceneIn),
            viewer(viewerIn)
        {
        }

        void setSelectionState(vsg::StateGroup& stateGroup, uint32_t freshIndex)
        {
            auto* object = stateGroup.getObject("slcIndexBuffer");
            auto* indexBuffer = object->cast<vsg::DescriptorBuffer>();
            auto* indexBufferInfo = indexBuffer->bufferInfoList[0]->cast<vsg::BufferInfo>();
            auto* localIndex = indexBufferInfo->data->cast<vsg::uintValue>();
            localIndex->set(freshIndex);
            localIndex->dirty();

            //manual transfer operation
            for (auto& task : viewer->recordAndSubmitTasks)
            {
                task->transferTask->assign(indexBuffer->bufferInfoList);
            }
        }

        void clearPreSelection()
        {
            if (!preSelection) return;
            setSelectionState(*preSelection, 0);
            preSelection.reset();
        }

        void setPreSelection(const vsg::Node* nodeIn)
        {
            auto* temp = nodeIn->cast<vsg::StateGroup>();
            preSelection = const_cast<vsg::StateGroup*>(temp);
            setSelectionState(*preSelection, 1);
        }

        bool select()
        {
            if (!preSelection) return false;
            setSelectionState(*preSelection, 2);
            selections.push_back(preSelection);

            std::string name = "no name";
            preSelection->getValue<std::string>("name", name);
            std::cout << name << " selected" << std::endl;

            preSelection.reset();
            return true;
        }

        bool clearSelection()
        {
            bool out = !selections.empty();
            for (const auto& ptr : selections) setSelectionState(*ptr, 0);
            selections.clear();
            std::cout << "selection cleared" << std::endl;
            return out;
        }

        void apply(vsg::MoveEvent& moveEvent) override
        {
            if (moveEvent.handled)
            {
                clearPreSelection();
                return;
            }
            auto intersector = vsg::LineSegmentIntersector::create(*camera, moveEvent.x, moveEvent.y);
            scenegraph->accept(*intersector);
            auto& intersections = intersector->intersections;
            if (intersections.empty())
            {
                clearPreSelection();
                return;
            }
            std::sort(intersections.begin(), intersections.end(), [](auto& lhs, auto& rhs) { return lhs->ratio < rhs->ratio; });

            auto isSelected = [&](const vsg::Node* nodeIn) -> bool //return true if node is in selection vector
            {
                for (const auto& s : selections)
                    if (s == nodeIn) return true;
                return false;
            };
            for (const auto& i : intersections)
            {
                const auto& np = i->nodePath;
                const auto* node = np.at(np.size() - 2);
                if (node == preSelection) return;
                if (isSelected(node)) continue;
                clearPreSelection();
                setPreSelection(node);
                moveEvent.handled = true;

                break;
            }
        }

        void apply(vsg::ButtonReleaseEvent& event) override
        {
            if (event.handled) return;
            if (event.button == 1) event.handled = select();
            if (event.button == 2) event.handled = clearSelection();
        }
    };
} // namespace

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    if (arguments.read("--help") || arguments.read("-h"))
    {
        std::cout << std::endl
                  << "Description: Altering face color on mouse move and button click." << std::endl
                  << "    mouse moves sets pre-highlight color." << std::endl
                  << "    mouse button 1 click, changes current pre-highlight to highlight and stores selection." << std::endl
                  << "    mouse button 2 click, clears selection." << std::endl
                  << std::endl
                  << "-h, --help      " << "Display this message" << std::endl
                  << "-d, --debug     " << "Enable the vulkan validation layer" << std::endl
                  << std::endl;
        return 0;
    }

    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

    // build scene
    auto facePipelineConfig = buildPipelineConfig(options);
    if (!facePipelineConfig) return 1;
    auto scene = vsg::StateGroup::create();
    facePipelineConfig->copyTo(scene);
    scene->addChild(buildBox(*facePipelineConfig));

    // scene calc
    vsg::ComputeBounds computeBounds;
    scene->accept(computeBounds);
    vsg::dvec3 center = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double camDistance = radius * 3.5;
    vsg::dvec3 camProjection(0.0, -camDistance, 0.0);

    //light
    auto absoluteTransform = vsg::AbsoluteTransform::create();
    scene->addChild(absoluteTransform);
    {
        auto ambientLight = vsg::AmbientLight::create();
        ambientLight->name = "ambient";
        ambientLight->color.set(1.0f, 1.0f, 1.0f);
        ambientLight->intensity = 0.0044f;
        absoluteTransform->addChild(ambientLight);
    }
    {
        vsg::dquat xRot(vsg::radians(10.0), vsg::dvec3(-1.0, 0.0, 0.0));
        vsg::dquat yRot(vsg::radians(30.0), vsg::dvec3(0.0, 1.0, 0.0));
        auto dLight = vsg::DirectionalLight::create();
        dLight->name = "directional";
        dLight->color = {1.0, 1.0, 1.0};
        dLight->intensity = 0.8;
        dLight->direction = xRot * yRot * vsg::dvec3(0.0, 0.0, -1.0);
        absoluteTransform->addChild(dLight);
    }

    //build window
    auto windowTraits = vsg::WindowTraits::create(arguments);
    if (arguments.read("--debug") || arguments.read("-d")) windowTraits->debugLayer = true;
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create window." << std::endl;
        return 1;
    }
    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);

    auto viewport = vsg::ViewportState::create(0, 0, window->extent2D().width, window->extent2D().height);
    auto perspective = vsg::Perspective::create(45.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), 0.1, radius * 10.0);
    auto lookAt = vsg::LookAt::create(center + camProjection, center, vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);
    auto commandGraph = vsg::createCommandGraphForView(window, camera, scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
    viewer->compile();
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});
    viewer->addEventHandler(vsg::Trackball::create(camera));
    viewer->addEventHandler(IntersectionHandler::create(camera, scene, viewer));

    while (viewer->advanceToNextFrame())
    {
        viewer->handleEvents();
        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();
    }

    return 0;
}
