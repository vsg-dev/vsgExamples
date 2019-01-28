#include <vsg/all.h>


#include <iostream>
#include <chrono>

#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgUtil/Optimizer>
#include <osg/Billboard>
#include <osg/MatrixTransform>

#include "Trackball.h"
#include "GraphicsNodes.h"


vsg::ref_ptr<vsg::GraphicsPipelineGroup> createGraphicsPipeline(vsg::Paths& searchPaths)
{
    //
    // load shaders
    //
    vsg::ref_ptr<vsg::Shader> vertexShader = vsg::Shader::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    vsg::ref_ptr<vsg::Shader> fragmentShader = vsg::Shader::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout<<"Could not create shaders."<<std::endl;
        return vsg::ref_ptr<vsg::GraphicsPipelineGroup>();
    }

    //
    // set up graphics pipeline
    //
    vsg::ref_ptr<vsg::GraphicsPipelineGroup> gp = vsg::GraphicsPipelineGroup::create();

    gp->shaders = vsg::GraphicsPipelineGroup::Shaders{vertexShader, fragmentShader};
    gp->maxSets = 1;
    gp->descriptorPoolSizes = vsg::DescriptorPoolSizes
    {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} // texture
    };

    gp->descriptorSetLayoutBindings = vsg::DescriptorSetLayoutBindings
    {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    gp->pushConstantRanges = vsg::PushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 196} // projection view, and model matrices
    };

    gp->vertexBindingsDescriptions = vsg::VertexInputState::Bindings
    {
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
    };

    gp->vertexAttributeDescriptions = vsg::VertexInputState::Attributes
    {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // tex coord data
    };

    gp->pipelineStates = vsg::GraphicsPipelineStates
    {
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create()
    };

    return gp;
}

vsg::ref_ptr<vsg::Node> createSceneData(vsg::Paths& searchPaths)
{
    //
    // set up graphics pipeline
    //
    vsg::ref_ptr<vsg::GraphicsPipelineGroup> gp = createGraphicsPipeline(searchPaths);


    //
    // set up model transformation node
    //
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT, 128

    // add transform to graphics pipeline group
    gp->addChild(transform);


    //
    // create texture node
    //
    //
    std::string textureFile("textures/lz.vsgb");
    vsg::vsgReaderWriter vsgReader;
    auto textureData = vsgReader.read<vsg::Data>(vsg::findFile(textureFile, searchPaths));
    if (!textureData)
    {
        std::cout<<"Could not read texture file : "<<textureFile<<std::endl;
        return vsg::ref_ptr<vsg::Node>();
    }
    vsg::ref_ptr<vsg::Texture> texture = vsg::Texture::create();
    texture->_textureData = textureData;

    // add texture node to transform node
    transform->addChild(texture);


    //
    // set up vertex and index arrays
    //
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
    }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

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
    }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

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
    }); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    vsg::ref_ptr<vsg::ushortArray> indices(new vsg::ushortArray
    {
        0, 1, 2,
        2, 3, 0,
        4, 5, 6,
        6, 7, 4
    }); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto geometry = vsg::Geometry::create();

    // setup geometry
    geometry->_arrays = vsg::DataList{vertices, colors, texcoords};
    geometry->_indices = indices;

    vsg::ref_ptr<vsg::DrawIndexed> drawIndexed = vsg::DrawIndexed::create(12, 1, 0, 0, 0);
    geometry->_commands = vsg::Geometry::Commands{drawIndexed};

    // add geometry to texture group
    texture->addChild(geometry);

    return gp;
}

class SceneAnalysisVisitor : public osg::NodeVisitor
{
public:
    SceneAnalysisVisitor():
        osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {}


    using Geometries = std::vector<osg::ref_ptr<osg::Geometry>>;
    using StateGeometryMap = std::map<osg::ref_ptr<osg::StateSet>, Geometries>;
    using TransformGeometryMap = std::map<osg::Matrix, Geometries>;

    struct TransformStatePair
    {
        std::map<osg::Matrix, StateGeometryMap> matrixStateGeometryMap;
        std::map<osg::ref_ptr<osg::StateSet>, TransformGeometryMap> stateTransformMap;
    };

    using ProgramTransformStateMap = std::map<osg::ref_ptr<osg::StateSet>, TransformStatePair>;

    using StateStack = std::vector<osg::ref_ptr<osg::StateSet>>;
    using StateSets = std::set<StateStack>;
    using MatrixStack = std::vector<osg::Matrixd>;
    using StatePair = std::pair<osg::ref_ptr<osg::StateSet>, osg::ref_ptr<osg::StateSet>>;
    using StateMap = std::map<StateStack, StatePair>;

    struct UniqueStateSet
    {
        bool operator() ( const osg::ref_ptr<osg::StateSet>& lhs, const osg::ref_ptr<osg::StateSet>& rhs) const
        {
            if (!lhs) return lhs;
            if (!rhs) return rhs;
            return lhs->compare(*rhs)<0;
        }
    };

    using UniqueStats = std::set<osg::ref_ptr<osg::StateSet>, UniqueStateSet>;

    StateStack statestack;
    StateMap stateMap;
    MatrixStack matrixstack;
    UniqueStats uniqueStateSets;
    ProgramTransformStateMap programTransformStateMap;

    osg::ref_ptr<osg::StateSet> uniqueState(osg::ref_ptr<osg::StateSet> stateset)
    {
        if (auto itr = uniqueStateSets.find(stateset); itr != uniqueStateSets.end())
        {
            std::cout<<"    uniqueState() found state"<<std::endl;
            return *itr;
        }

        std::cout<<"    uniqueState() inserting state"<<std::endl;

        uniqueStateSets.insert(stateset);
        return stateset;
    }

    StatePair computeStatePair(osg::StateSet* stateset)
    {
        if (!stateset) return StatePair();

        osg::ref_ptr<osg::StateSet> programState = new osg::StateSet;
        osg::ref_ptr<osg::StateSet> dataState = new osg::StateSet;

        programState->setModeList(stateset->getModeList());
        programState->setTextureModeList(stateset->getTextureModeList());
        programState->setDefineList(stateset->getDefineList());

        dataState->setAttributeList(stateset->getAttributeList());
        dataState->setTextureAttributeList(stateset->getTextureAttributeList());
        dataState->setUniformList(stateset->getUniformList());

        for(auto attribute : stateset->getAttributeList())
        {
            osg::Program* program = dynamic_cast<osg::Program*>(attribute.second.first.get());
            if (program)
            {
                std::cout<<"Found program removing from dataState"<<program<<" inserting into programState"<<std::endl;
                dataState->removeAttribute(program);
                programState->setAttribute(program, attribute.second.second);
            }
        }

        return StatePair(uniqueState(programState), uniqueState(dataState));
    };

    void apply(osg::Node& node)
    {
        std::cout<<"Visiting "<<node.className()<<" "<<node.getStateSet()<<std::endl;

        if (node.getStateSet()) pushStateSet(*node.getStateSet());

        traverse(node);

        if (node.getStateSet()) popStateSet();
    }

    void apply(osg::Group& group)
    {
        std::cout<<"Group "<<group.className()<<" "<<group.getStateSet()<<std::endl;

        if (group.getStateSet()) pushStateSet(*group.getStateSet());

        traverse(group);

        if (group.getStateSet()) popStateSet();
    }

    void apply(osg::Transform& transform)
    {
        std::cout<<"Transform "<<transform.className()<<" "<<transform.getStateSet()<<std::endl;

        if (transform.getStateSet()) pushStateSet(*transform.getStateSet());

        osg::Matrix matrix;
        if (!matrixstack.empty()) matrix = matrixstack.back();
        transform.computeLocalToWorldMatrix(matrix, this);

        pushMatrix(matrix);

        traverse(transform);

        popMatrix();

        if (transform.getStateSet()) popStateSet();
    }

    void apply(osg::Billboard& billboard)
    {
        std::cout<<"apply(osg::Billboard& billboard)"<<std::endl;

        for(unsigned int i=0; i<billboard.getNumDrawables(); ++i)
        {
            auto translate = osg::Matrixd::translate(billboard.getPosition(i));

            if (matrixstack.empty()) pushMatrix(translate);
            else pushMatrix(translate * matrixstack.back());

            billboard.getDrawable(i)->accept(*this);

            popMatrix();
        }
    }

    void apply(osg::Geometry& geometry)
    {
        if (geometry.getStateSet()) pushStateSet(*geometry.getStateSet());

        auto itr = stateMap.find(statestack);

        if (itr==stateMap.end())
        {
            if (statestack.empty())
            {
                std::cout<<"New Empty StateSet's"<<std::endl;
                stateMap[statestack] = computeStatePair(0);
            }
            else if (statestack.size()==1)
            {
                std::cout<<"New Single  StateSet's"<<std::endl;
                stateMap[statestack] = computeStatePair(statestack.back());
            }
            else // multiple stateset's need to merge
            {
                std::cout<<"New Merging StateSet's "<<statestack.size()<<std::endl;
                osg::ref_ptr<osg::StateSet> new_stateset = new osg::StateSet;
                for(auto& stateset : statestack)
                {
                    new_stateset->merge(*stateset);
                }
                stateMap[statestack] = computeStatePair(new_stateset);
            }

            itr = stateMap.find(statestack);

            if (itr->second.first.valid()) osgDB::writeObjectFile(*(itr->second.first), vsg::make_string("programState_", stateMap.size(),".osgt"));
            if (itr->second.second.valid()) osgDB::writeObjectFile(*(itr->second.second), vsg::make_string("dataState_", stateMap.size(),".osgt"));

            std::cout<<"Need to create StateSet"<<std::endl;
        }
        else
        {
            std::cout<<"Already have StateSet"<<std::endl;
        }

        osg::Matrix matrix;
        if (!matrixstack.empty()) matrix = matrixstack.back();

        StatePair& statePair = itr->second;

        TransformStatePair& transformStatePair = programTransformStateMap[statePair.first];
        StateGeometryMap& stateGeometryMap = transformStatePair.matrixStateGeometryMap[matrix];
        stateGeometryMap[statePair.second].push_back(&geometry);

        TransformGeometryMap& transformGeometryMap = transformStatePair.stateTransformMap[statePair.second];
        transformGeometryMap[matrix].push_back(&geometry);

        std::cout<<"   Geometry "<<geometry.className()<<" ss="<<statestack.size()<<" ms="<<matrixstack.size()<<std::endl;

        if (geometry.getStateSet()) popStateSet();
    }

    void pushStateSet(osg::StateSet& stateset)
    {
        statestack.push_back(&stateset);
    }

    void popStateSet()
    {
        statestack.pop_back();
    }

    void pushMatrix(const osg::Matrix& matrix)
    {
        matrixstack.push_back(matrix);
    }

    void popMatrix()
    {
        matrixstack.pop_back();
    }

    void print()
    {
        std::cout<<"\nprint()\n";
        std::cout<<"   programTransformStateMap.size() = "<<programTransformStateMap.size()<<std::endl;
        for(auto [programStateSet, transformStatePair] : programTransformStateMap)
        {
            std::cout<<"       programStateSet = "<<programStateSet.get()<<std::endl;
            std::cout<<"           transformStatePair.matrixStateGeometryMap.size() = "<<transformStatePair.matrixStateGeometryMap.size()<<std::endl;
            std::cout<<"           transformStatePair.stateTransformMap.size() = "<<transformStatePair.stateTransformMap.size()<<std::endl;
        }
    }

    osg::ref_ptr<osg::Node> createStateGeometryGraph(StateGeometryMap& stateGeometryMap)
    {
        std::cout<<"createStateGeometryGraph()"<<stateGeometryMap.size()<<std::endl;

        if (stateGeometryMap.empty()) return nullptr;

        osg::ref_ptr<osg::Group> group = new osg::Group;
        for(auto [stateset, geometries] : stateGeometryMap)
        {
            osg::ref_ptr<osg::Group> stateGroup = new osg::Group;
            stateGroup->setStateSet(stateset);
            group->addChild(stateGroup);

            for(auto& geometry : geometries)
            {
                osg::ref_ptr<osg::Geometry> new_geometry = geometry; // new osg::Geometry(*geometry);
                new_geometry->setStateSet(nullptr);
                stateGroup->addChild(new_geometry);
            }
        }

        if (group->getNumChildren()==1) return group->getChild(0);

        return group;
    }

    osg::ref_ptr<osg::Node> createTransformGeometryGraph(TransformGeometryMap& transformGeometryMap)
    {
        std::cout<<"createStateGeometryGraph()"<<transformGeometryMap.size()<<std::endl;

        if (transformGeometryMap.empty()) return nullptr;

        osg::ref_ptr<osg::Group> group = new osg::Group;
        for(auto [matrix, geometries] : transformGeometryMap)
        {
            osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform;
            transform->setMatrix(matrix);
            group->addChild(transform);

            for(auto& geometry : geometries)
            {
                osg::ref_ptr<osg::Geometry> new_geometry = geometry; // new osg::Geometry(*geometry);
                new_geometry->setStateSet(nullptr);
                transform->addChild(new_geometry);
            }
        }

        if (group->getNumChildren()==1) return group->getChild(0);

        return group;
    }

    osg::ref_ptr<osg::Node> createOSG()
    {
        osg::ref_ptr<osg::Group> group = new osg::Group;

        for(auto [programStateSet, transformStatePair] : programTransformStateMap)
        {
            osg::ref_ptr<osg::Group> programGroup = new osg::Group;
            group->addChild(programGroup);
            programGroup->setStateSet(programStateSet.get());

            bool transformAtTop = transformStatePair.matrixStateGeometryMap.size() < transformStatePair.stateTransformMap.size();
            if (transformAtTop)
            {
                for(auto [matrix, stateGeometryMap] : transformStatePair.matrixStateGeometryMap)
                {
                    osg::ref_ptr<osg::Node> stateGeometryGraph = createStateGeometryGraph(stateGeometryMap);
                    if (!stateGeometryGraph) continue;

                    if (!matrix.isIdentity())
                    {
                        osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform;
                        transform->setMatrix(matrix);
                        programGroup->addChild(transform);
                        transform->addChild(stateGeometryGraph);
                    }
                    else
                    {
                        programGroup->addChild(stateGeometryGraph);
                    }
                }
            }
            else
            {
                for(auto [stateset, transformeGeometryMap] : transformStatePair.stateTransformMap)
                {
                    osg::ref_ptr<osg::Node> transformGeometryGraph = createTransformGeometryGraph(transformeGeometryMap);
                    if (!transformGeometryGraph) continue;

                    transformGeometryGraph->setStateSet(stateset);
                    programGroup->addChild(transformGeometryGraph);
                }
            }

            std::cout<<"       programStateSet = "<<programStateSet.get()<<std::endl;
            std::cout<<"           transformStatePair.matrixStateGeometryMap.size() = "<<transformStatePair.matrixStateGeometryMap.size()<<std::endl;
            std::cout<<"           transformStatePair.stateTransformMap.size() = "<<transformStatePair.stateTransformMap.size()<<std::endl;
        }
        return group;
    }
};

vsg::ref_ptr<vsg::Node> convertToVsg(osg::ref_ptr<osg::Node> osg_scene)
{
    SceneAnalysisVisitor print;
    osg_scene->accept(print);

    print.print();

    return vsg::ref_ptr<vsg::Node>();
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    auto debugLayer = arguments.read({"--debug","-d"});
    auto apiDumpLayer = arguments.read({"--api","-a"});
    auto numFrames = arguments.value(-1, "-f");
    auto printFrameRate = arguments.read("--fr");
    auto optimize = !arguments.read("--no-optimize");
    auto outputFilename = arguments.value(std::string(), "-o");
    auto [width, height] = arguments.value(std::pair<uint32_t, uint32_t>(800, 600), {"--window", "-w"});
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");


    // read osg models.
    osg::ArgumentParser osg_arguments(&argc, argv);
    osg::ref_ptr<osg::Node> osg_scene = osgDB::readNodeFiles(osg_arguments);

    if (optimize)
    {
        osgUtil::Optimizer optimizer;
        optimizer.optimize(osg_scene.get());
    }

    if (!outputFilename.empty())
    {
        SceneAnalysisVisitor sceneAnalysis;
        osg_scene->accept(sceneAnalysis);

        auto scene = sceneAnalysis.createOSG();
        if (scene.valid())
        {
            std::cout<<"Writing file to "<<outputFilename<<std::endl;
            osgDB::writeNodeFile(*scene, outputFilename);
        }

        return 1;
    }


    // create the scene/command graph
    vsg::ref_ptr<vsg::Node> commandGraph  = osg_scene.valid() ? convertToVsg(osg_scene) : createSceneData(searchPaths);

    if (!commandGraph)
    {
        std::cout<<"No command graph created."<<std::endl;
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

    // camera related state
    vsg::ref_ptr<vsg::mat4Value> projMatrix(new vsg::mat4Value);
    vsg::ref_ptr<vsg::mat4Value> viewMatrix(new vsg::mat4Value);
    auto viewport = vsg::ViewportState::create(VkExtent2D{width, height});

    vsg::ref_ptr<vsg::Perspective> perspective(new vsg::Perspective(60.0, static_cast<double>(width) / static_cast<double>(height), 0.1, 10.0));
    vsg::ref_ptr<vsg::LookAt> lookAt(new vsg::LookAt(vsg::dvec3(1.0, 1.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0)));
    vsg::ref_ptr<vsg::Camera> camera(new vsg::Camera(perspective, lookAt, viewport));


    // compile the Vulkan objects
    vsg::CompileTraversal compile;
    compile.context.device = device;
    compile.context.commandPool = commandPool;
    compile.context.renderPass = renderPass;
    compile.context.viewport = viewport;
    compile.context.graphicsQueue = graphicsQueue;
    compile.context.projMatrix = projMatrix;
    compile.context.viewMatrix = viewMatrix;

    commandGraph->accept(compile);

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

            vsg::UpdatePipeline updatePipeline(camera->getViewportState());

            viewport->getViewport().width = static_cast<float>(windowExtent.width);
            viewport->getViewport().height = static_cast<float>(windowExtent.height);
            viewport->getScissor().extent = windowExtent;

            commandGraph->accept(updatePipeline);

            perspective->aspectRatio = static_cast<double>(windowExtent.width) / static_cast<double>(windowExtent.height);

            std::cout<<"window aspect ratio = "<<perspective->aspectRatio<<std::endl;
        }
    }


    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
