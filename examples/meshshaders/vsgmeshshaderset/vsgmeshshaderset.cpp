#include <iostream>
#include <vsg/all.h>
#include <vsgXchange/all.h>
#include <vsgXchange/gltf.h>
#include <vsgXchange/3DTiles.h>

#ifdef vsgXchange_meshoptimizer
#include <meshoptimizer.h>
#endif

// MeshShader blog posts
// https://chaoticbob.github.io/2024/01/24/mesh-shading-part-1.html
// https://interplayoflight.wordpress.com/2025/05/05/meshlets-and-mesh-shaders/
// https://developer.nvidia.com/blog/introduction-turing-mesh-shaders/
// https://docs.vulkan.org/features/latest/features/proposals/VK_EXT_mesh_shader.html
// https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/VkPhysicalDevice8BitStorageFeatures.html

namespace custom
{
    const char* task_option = "task";

    vsg::ref_ptr<vsg::ShaderSet> pbr_ShaderSet(vsg::ref_ptr<const vsg::Options> options)
    {
        bool useTaskShader = vsg::value<bool>(false, custom::task_option, options);

        vsg::info("Local pbr_ShaderSet(", options, ") useTaskShader = ", useTaskShader);

        vsg::ShaderStages shaderStages;

        if (useTaskShader)
        {
            if (auto taskShader = vsg::read_cast<vsg::ShaderStage>("shaders/standard.task", options)) shaderStages.push_back(taskShader);
        }

        if (auto meshShader = vsg::read_cast<vsg::ShaderStage>("shaders/standard.mesh", options)) shaderStages.push_back(meshShader);
        if (auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/standard_pbr.frag", options)) shaderStages.push_back(fragmentShader);

        if (shaderStages.empty())
        {
            vsg::error("mesh pbr_ShaderSet(...) could not find shaders.");
            return {};
        }

        auto shaderSet = vsg::ShaderSet::create(shaderStages);

        #define VIEW_DESCRIPTOR_SET 0
        #define MATERIAL_DESCRIPTOR_SET 1
        #define MESH_DESCRIPTOR_SET 2


        // array bindings as descriptors
        VkShaderStageFlags meshShaderStage = VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT;

        shaderSet->addDescriptorBinding("vsg_Vertex", "", MESH_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::vec3Array::create(1));
        shaderSet->addDescriptorBinding("vsg_Normal", "", MESH_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::vec3Array::create(1));
        shaderSet->addDescriptorBinding("vsg_TexCoord0", "VSG_TEXTURECOORD_0", MESH_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::vec2Array::create(1));
        shaderSet->addDescriptorBinding("vsg_TexCoord1", "VSG_TEXTURECOORD_1", MESH_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::vec2Array::create(1));
        shaderSet->addDescriptorBinding("vsg_TexCoord2", "VSG_TEXTURECOORD_2", MESH_DESCRIPTOR_SET, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::vec2Array::create(1));
        shaderSet->addDescriptorBinding("vsg_TexCoord3", "VSG_TEXTURECOORD_3", MESH_DESCRIPTOR_SET, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::vec2Array::create(1));
        shaderSet->addDescriptorBinding("vsg_Color", "", MESH_DESCRIPTOR_SET, 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::vec4Array::create(1), vsg::CoordinateSpace::LINEAR);

        shaderSet->addDescriptorBinding("vsg_Translation_scaleDistance", "VSG_BILLBOARD", MESH_DESCRIPTOR_SET, 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::vec4Array::create(1));

        shaderSet->addDescriptorBinding("vsg_Translation", "VSG_INSTANCE_TRANSLATION", MESH_DESCRIPTOR_SET, 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::vec3Array::create(1));
        shaderSet->addDescriptorBinding("vsg_Rotation", "VSG_INSTANCE_ROTATION", MESH_DESCRIPTOR_SET, 8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::quatArray::create(1));
        shaderSet->addDescriptorBinding("vsg_Scale", "VSG_INSTANCE_SCALE", MESH_DESCRIPTOR_SET, 9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::vec3Array::create(1));

        shaderSet->addDescriptorBinding("vsg_JointIndices", "VSG_SKINNING", MESH_DESCRIPTOR_SET, 10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::ivec4Array::create(1));
        shaderSet->addDescriptorBinding("vsg_JointWeights", "VSG_SKINNING", MESH_DESCRIPTOR_SET, 11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::vec4Array::create(1));

        shaderSet->addDescriptorBinding("vertexInputRates", "", MESH_DESCRIPTOR_SET, 12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::uintArray::create(1));

        shaderSet->addDescriptorBinding("meshlets", "", MESH_DESCRIPTOR_SET, 13, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::uivec4Array::create(1));
        shaderSet->addDescriptorBinding("meshlet_Vertices", "", MESH_DESCRIPTOR_SET, 14, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::uintArray::create(1));
        shaderSet->addDescriptorBinding("meshlet_Triangles", "", MESH_DESCRIPTOR_SET, 15, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::ubyteArray::create(1));
        shaderSet->addDescriptorBinding("meshlet_Bounds", "", MESH_DESCRIPTOR_SET, 16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::vec4Array::create(1));

        // standard descriptors
        shaderSet->addDescriptorBinding("diffuseMap", "VSG_DIFFUSE_MAP", MATERIAL_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
        shaderSet->addDescriptorBinding("detailMap", "VSG_DETAIL_MAP", MATERIAL_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
        shaderSet->addDescriptorBinding("normalMap", "VSG_NORMAL_MAP", MATERIAL_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec3Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32B32_SFLOAT}), vsg::CoordinateSpace::LINEAR);
        shaderSet->addDescriptorBinding("aoMap", "VSG_LIGHTMAP_MAP", MATERIAL_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
        shaderSet->addDescriptorBinding("emissiveMap", "VSG_EMISSIVE_MAP", MATERIAL_DESCRIPTOR_SET, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
        shaderSet->addDescriptorBinding("specularMap", "VSG_SPECULAR_MAP", MATERIAL_DESCRIPTOR_SET, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
        shaderSet->addDescriptorBinding("mrMap", "VSG_METALLROUGHNESS_MAP", MATERIAL_DESCRIPTOR_SET, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec2Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32_SFLOAT}), vsg::CoordinateSpace::LINEAR);

        shaderSet->addDescriptorBinding("displacementMap", "VSG_DISPLACEMENT_MAP", MATERIAL_DESCRIPTOR_SET, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}), vsg::CoordinateSpace::LINEAR);
        shaderSet->addDescriptorBinding("displacementMapScale", "VSG_DISPLACEMENT_MAP", MATERIAL_DESCRIPTOR_SET, 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::vec3Value::create(1.0f, 1.0f, 1.0f));

        shaderSet->addDescriptorBinding("material", "", MATERIAL_DESCRIPTOR_SET, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PbrMaterialValue::create(), vsg::CoordinateSpace::LINEAR);
        shaderSet->addDescriptorBinding("texCoordIndices", "", MATERIAL_DESCRIPTOR_SET, 11, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::TexCoordIndicesValue::create(), vsg::CoordinateSpace::LINEAR);

        shaderSet->addDescriptorBinding("jointMatrices", "VSG_SKINNING", MATERIAL_DESCRIPTOR_SET, 12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::mat4Value::create());

        shaderSet->addDescriptorBinding("lightData", "", VIEW_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));
        shaderSet->addDescriptorBinding("viewportData", "", VIEW_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Value::create(0, 0, 1280, 1024));
        shaderSet->addDescriptorBinding("shadowMaps", "", VIEW_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
        shaderSet->addDescriptorBinding("shadowMapDirectSampler", "VSG_SHADOWS_PCSS", VIEW_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
        shaderSet->addDescriptorBinding("shadowMapShadowSampler", "", VIEW_DESCRIPTOR_SET, 4, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);

        // additional defines
        shaderSet->optionalDefines = {"VSG_GREYSCALE_DIFFUSE_MAP", "VSG_TWO_SIDED_LIGHTING", "VSG_POINT_SPRITE", "VSG_WORKFLOW_SPECGLOSS", "VSG_SHADOWS_PCSS", "VSG_SHADOWS_SOFT", "VSG_SHADOWS_HARD", "SHADOWMAP_DEBUG", "VSG_ALPHA_TEST"};

        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_ALL, 0, 128);

#if 0
        // TODO need to implement mesh shader alternates for these...
        shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_TRANSLATION"}, vsg::TranslationArrayState::create()});
        shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_TRANSLATION", "VSG_INSTANCE_ROTATION", "VSG_INSTANCE_SCALE"}, vsg::TranslationRotationScaleArrayState::create()});
        shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_TRANSLATION", "VSG_DISPLACEMENT_MAP"}, vsg::TranslationAndDisplacementMapArrayState::create()});
        shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_DISPLACEMENT_MAP"}, vsg::DisplacementMapArrayState::create()});
        shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_BILLBOARD"}, vsg::BillboardArrayState::create()});
#endif

        shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(VIEW_DESCRIPTOR_SET));

        return shaderSet;
    }


    class MeshGltfBuilder : public vsg::Inherit<vsgXchange::gltf::Builder, MeshGltfBuilder>
    {
    public:
        MeshGltfBuilder()
        {
        }

        MeshGltfBuilder(const MeshGltfBuilder&, const vsg::CopyOp& = {}) :
        Inherit()
        {
        }

        vsg::ref_ptr<vsg::Object> clone(const vsg::CopyOp& copyop = {}) const override
        {
            return MeshGltfBuilder::create(*this, copyop);
        }


        vsg::ref_ptr<vsg::Object> createSceneGraph(vsg::ref_ptr<vsgXchange::gltf::glTF> in_model, vsg::ref_ptr<const vsg::Options> in_options) override
        {
            vsg::info("\n\nMeshGltfBuilder::createSceneGraph(", in_model, ", ", in_options,") ", this,", filename = ", in_model->filename);
            return Inherit::createSceneGraph(in_model, in_options);
        }

        vsg::ref_ptr<vsg::Node> createMesh(vsg::ref_ptr<vsgXchange::gltf::Mesh> gltf_mesh, vsgXchange::gltf::Builder::MeshExtras& meshExtras) override;

    };

    class MeshTiles3DBuilder : public vsg::Inherit<vsgXchange::Tiles3D::Builder, MeshTiles3DBuilder>
    {
    public:
        MeshTiles3DBuilder()
        {
        }

        MeshTiles3DBuilder(const MeshTiles3DBuilder&, const vsg::CopyOp& = {}) :
        Inherit()
        {
        }

        vsg::ref_ptr<vsg::Object> clone(const vsg::CopyOp& copyop = {}) const override
        {
            return MeshTiles3DBuilder::create(*this, copyop);
        }


        vsg::ref_ptr<vsg::Node> createTile(vsg::ref_ptr<vsgXchange::Tiles3D::Tile> tile, uint32_t level, const std::string& inherited_refine) override
        {
            vsg::debug("MeshTiles3DBuilder::createTile(tile = ", tile, ", level = ", level, ", ", inherited_refine, ") options = ", options);
            return Inherit::createTile(tile, level, inherited_refine);
        }


        vsg::ref_ptr<vsg::Node> readInstanceChild(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> in_options) override
        {
            vsg::debug("Vegetation : MeshTiles3DBuilder::readInstanceChild( ", filename, ", ", options, " )");
            return Inherit::readInstanceChild(filename, in_options);
        }

        vsg::ref_ptr<vsg::Node> readInstanceChild(std::istream& fin, vsg::ref_ptr<const vsg::Options> in_options) override
        {
            vsg::debug("Vegetation : MeshTiles3DBuilder::readInstanceChild( ", &fin, ", ", options, " )");
            return Inherit::readInstanceChild(fin, in_options);
        }

        vsg::ref_ptr<vsg::Object> createSceneGraph(vsg::ref_ptr<vsgXchange::Tiles3D::Tileset> tileset, vsg::ref_ptr<const vsg::Options> in_options) override
        {
            auto name = vsg::simpleFilename(tileset->filename);
            vsg::ref_ptr<vsg::Object> object;
            if (name=="tileset_vegetation")
            {
                auto local_options = vsg::clone(in_options);
                local_options->setObject(vsgXchange::gltf::prototype_builder, custom::MeshGltfBuilder::create());

                vsg::debug("Vegetation : MeshTiles3DBuilder::createSceneGraph(", tileset, ", ", in_options,") ", this, ", filename = ", tileset->filename, ", local_options = ", local_options);

                object = Inherit::createSceneGraph(tileset, local_options);

                vsg::debug("after ", tileset->filename, "\n\n");
            }
            else if (name=="tileset_terrain")
            {
                auto local_options = vsg::clone(in_options);
                local_options->setObject(vsgXchange::gltf::prototype_builder, custom::MeshGltfBuilder::create());

                vsg::debug("Terrain : MeshTiles3DBuilder::createSceneGraph(", tileset, ", ", in_options,") ", this, ", filename = ", tileset->filename, ", local_options = ", local_options);

                object = Inherit::createSceneGraph(tileset, local_options);
                vsg::debug("after ", tileset->filename, "\n\n");
            }
            else
            {
                object = Inherit::createSceneGraph(tileset, in_options);
            }

            return object;
        }
    };
}

EVSG_type_name(custom::MeshGltfBuilder)
EVSG_type_name(custom::MeshTiles3DBuilder)

vsg::ref_ptr<vsg::Node> custom::MeshGltfBuilder::createMesh(vsg::ref_ptr<vsgXchange::gltf::Mesh> gltf_mesh, vsgXchange::gltf::Builder::MeshExtras& meshExtras)
{
    vsg::info("MeshGltfBuilder::createMesh(", gltf_mesh, ", ..) ", this);
    return Inherit::createMesh(gltf_mesh, meshExtras);
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create(arguments);

    // use the vsg::Options object to pass the ReaderWriter_all to use when reading files.
    auto options = vsg::Options::create();
    options->add(vsgXchange::all::create());
    options->setObject(vsgXchange::gltf::prototype_builder, custom::MeshGltfBuilder::create());
    options->setObject(vsgXchange::Tiles3D::prototype_builder, custom::MeshTiles3DBuilder::create());

    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

    // read any command line options that the ReaderWriters support
    options->readOptions(arguments);

    arguments.readAndAssign<bool>(custom::task_option, options);

    if (int log_level = 0; arguments.read("--log-level", log_level)) vsg::Logger::instance()->level = vsg::Logger::Level(log_level);
    auto numFrames = arguments.value(-1, "-f");
    auto pathFilename = arguments.value<vsg::Path>("", "-p");
    auto autoPlay = !arguments.read({"--no-auto-play", "--nop"});
    auto maxTime = arguments.value(std::numeric_limits<double>::max(), "--max-time");
    auto outputFilename = arguments.value<vsg::Path>("", "-o");
    auto outputShaderSetFilename = arguments.value<vsg::Path>("", "--os");
    auto horizonMountainHeight = arguments.value(0.0, "--hmh");
    auto nearFarRatio = arguments.value<double>(0.001, "--nfr");
    bool reportAverageFrameRate = arguments.read("--fps");
    if (arguments.read({"-t", "--test"}))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->fullscreen = true;
        reportAverageFrameRate = true;
    }
    if (arguments.read({"--st", "--small-test"}))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->width = 192, windowTraits->height = 108;
        windowTraits->decoration = false;
        reportAverageFrameRate = true;
    }

    if (!arguments.read("--standard"))
    {
        vsg::ref_ptr<vsg::ShaderSet> shaderSet = custom::pbr_ShaderSet(options);
        options->shaderSets["pbr"] = shaderSet;
        if (!shaderSet)
        {
            std::cout << "No vsg::ShaderSet to process." << std::endl;
            return 1;
        }

        if (outputShaderSetFilename)
        {
            vsg::write(shaderSet, outputShaderSetFilename, options);
        }
    }


    auto group = vsg::Group::create();

    vsg::Path path;

    // read any vsg files
    for (int i = 1; i < argc; ++i)
    {
        vsg::Path filename = arguments[i];
        if (auto node = vsg::read_cast<vsg::Node>(filename, options))
        {
            group->addChild(node);
        }
        else
        {
            std::cout << "Unable to load file " << filename << std::endl;
        }
    }

    if (group->children.empty())
    {
        return 1;
    }

    vsg::ref_ptr<vsg::Node> vsg_scene;
    if (group->children.size() == 1)
        vsg_scene = group->children[0];
    else
        vsg_scene = group;

    if (outputFilename)
    {
        vsg::write(vsg_scene, outputFilename, options);
        return 1;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    windowTraits->vulkanVersion = VK_API_VERSION_1_1;
    windowTraits->deviceExtensionNames = {VK_EXT_MESH_SHADER_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME};
    windowTraits->deviceExtensionNames.push_back(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
    windowTraits->deviceExtensionNames.push_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);

    // set up features
    auto& features = windowTraits->deviceFeatures;

    auto& meshFeatures = features->get<VkPhysicalDeviceMeshShaderFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT>();
    meshFeatures.meshShader = VK_TRUE;
    meshFeatures.taskShader = VK_TRUE;

    auto& barycentricFeatures = features->get<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR>();
    barycentricFeatures.fragmentShaderBarycentric = VK_TRUE;

    auto& eightBitStorageFeatures = features->get<VkPhysicalDevice8BitStorageFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES>();
    eightBitStorageFeatures.storageBuffer8BitAccess = VK_TRUE;
    eightBitStorageFeatures.uniformAndStorageBuffer8BitAccess = VK_TRUE;
    // storagePushConstant8;

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create window." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // compute the bounds of the scene graph to help position camera
    auto bounds = vsg::visit<vsg::ComputeBounds>(vsg_scene).bounds;
    vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;
    double radius = vsg::length(bounds.max - bounds.min) * 0.6;

    // vsg::info("scene bounds ", bounds);

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
    if (ellipsoidModel)
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 1000.0);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    auto cameraAnimation = vsg::CameraAnimationHandler::create(camera, pathFilename, options);
    viewer->addEventHandler(cameraAnimation);
    if (autoPlay && cameraAnimation->animation)
    {
        cameraAnimation->play();

        if (reportAverageFrameRate && (maxTime == std::numeric_limits<double>::max()) && (numFrames < 0))
        {
            maxTime = cameraAnimation->animation->maxTime();
        }
    }

    viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

    auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile();

    if (autoPlay)
    {
        // find any animation groups in the loaded scene graph and play the first animation in each of the animation groups.
        auto animationGroups = vsg::visit<vsg::FindAnimations>(vsg_scene).animationGroups;
        for (auto ag : animationGroups)
        {
            if (!ag->animations.empty()) viewer->animationManager->play(ag->animations.front());
        }
    }

    viewer->start_point() = vsg::clock::now();

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0) && (viewer->getFrameStamp()->simulationTime < maxTime))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    if (reportAverageFrameRate)
    {
        auto fs = viewer->getFrameStamp();
        double fps = static_cast<double>(fs->frameCount) / std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - viewer->start_point()).count();
        std::cout << "Average frame rate = " << fps << " fps" << std::endl;
    }
    return 0;
}
