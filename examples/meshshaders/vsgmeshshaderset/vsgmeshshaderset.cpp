#include <iostream>
#include <vsg/all.h>
#include <vsgXchange/all.h>
#include <vsgXchange/gltf.h>
#include <vsgXchange/3DTiles.h>

#ifdef vsgXchange_meshoptimizer
#include <meshoptimizer.h>
#endif

namespace vsg
{
#include "../../../data/shaders/mesh_common.h"
}

// MeshShader blog posts
// https://chaoticbob.github.io/2024/01/24/mesh-shading-part-1.html
// https://interplayoflight.wordpress.com/2025/05/05/meshlets-and-mesh-shaders/
// https://developer.nvidia.com/blog/introduction-turing-mesh-shaders/
// https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/VkPhysicalDevice8BitStorageFeatures.html
// https://docs.vulkan.org/features/latest/features/proposals/VK_EXT_mesh_shader.html
// https://docs.vulkan.org/glsl/latest/chapters/builtins.html
// https://docs.vulkan.org/glslext/latest/glslext/ext/GL_EXT_control_flow_attributes.html
// https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_8bit_storage.html
// https://docs.vulkan.org/glslext/latest/glslext/khr/GL_KHR_shader_subgroup.html
// https://docs.vulkan.org/guide/latest/subgroups.html
// https://www.khronos.org/blog/vulkan-subgroup-tutorial
// https://www.khronos.org/assets/uploads/developers/library/2018-vulkan-devday/06-subgroups.pdf
// https://www.youtube.com/watch?v=3EMdMD1PsgY
// https://www.youtube.com/watch?v=nr23tTzToYk
namespace custom
{
    const char* task_option = "task";
    const char* debug_option = "debug-colors";

    vsg::ref_ptr<vsg::ShaderSet> pbr_ShaderSet(vsg::ref_ptr<const vsg::Options> options)
    {
        bool useTaskShader = vsg::value<bool>(false, custom::task_option, options);
        bool debugColors = vsg::value<bool>(false, custom::debug_option, options);

        vsg::info("Local pbr_ShaderSet(", options, ") useTaskShader = ", useTaskShader, ",  debugColors = ", debugColors);

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


        shaderSet->addDescriptorBinding("vsg_Vertex", "VSG_VERTEX", MESH_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::vec3Array::create(1));
        shaderSet->addDescriptorBinding("vsg_Normal", "VSG_NORMAL", MESH_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::vec3Array::create(1));
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

        shaderSet->addDescriptorBinding("mesh", "", MESH_DESCRIPTOR_SET, 13, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage, vsg::ubyteArray::create(32, 0));
        shaderSet->addDescriptorBinding("meshlets", "", MESH_DESCRIPTOR_SET, 14, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::uivec4Array::create(1));
        shaderSet->addDescriptorBinding("meshlet_Vertices", "", MESH_DESCRIPTOR_SET, 15, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::uintArray::create(1));
        shaderSet->addDescriptorBinding("meshlet_Triangles", "", MESH_DESCRIPTOR_SET, 16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::ubyteArray::create(1));
        shaderSet->addDescriptorBinding("meshlet_Bounds", "", MESH_DESCRIPTOR_SET, 17, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::vec4Array::create(1));

        shaderSet->addDescriptorBinding("vsg_PackedVertex", "PACKED_VERTEX", MESH_DESCRIPTOR_SET, 18, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::floatArray2D::create(8,1));

        // standard descriptors
        shaderSet->addDescriptorBinding("diffuseMap", "VSG_DIFFUSE_MAP", MATERIAL_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
        shaderSet->addDescriptorBinding("detailMap", "VSG_DETAIL_MAP", MATERIAL_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
        shaderSet->addDescriptorBinding("normalMap", "VSG_NORMAL_MAP", MATERIAL_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec3Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32B32_SFLOAT}), vsg::CoordinateSpace::LINEAR);
        shaderSet->addDescriptorBinding("aoMap", "VSG_LIGHTMAP_MAP", MATERIAL_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
        shaderSet->addDescriptorBinding("emissiveMap", "VSG_EMISSIVE_MAP", MATERIAL_DESCRIPTOR_SET, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
        shaderSet->addDescriptorBinding("specularMap", "VSG_SPECULAR_MAP", MATERIAL_DESCRIPTOR_SET, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
        shaderSet->addDescriptorBinding("mrMap", "VSG_METALLROUGHNESS_MAP", MATERIAL_DESCRIPTOR_SET, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec2Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32_SFLOAT}), vsg::CoordinateSpace::LINEAR);

        shaderSet->addDescriptorBinding("displacementMap", "VSG_DISPLACEMENT_MAP", MATERIAL_DESCRIPTOR_SET, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, meshShaderStage, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}), vsg::CoordinateSpace::LINEAR);
        shaderSet->addDescriptorBinding("displacementMapScale", "VSG_DISPLACEMENT_MAP", MATERIAL_DESCRIPTOR_SET, 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage, vsg::vec3Value::create(1.0f, 1.0f, 1.0f));

        shaderSet->addDescriptorBinding("material", "", MATERIAL_DESCRIPTOR_SET, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PbrMaterialValue::create(), vsg::CoordinateSpace::LINEAR);
        shaderSet->addDescriptorBinding("texCoordIndices", "", MATERIAL_DESCRIPTOR_SET, 11, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::TexCoordIndicesValue::create(), vsg::CoordinateSpace::LINEAR);

        shaderSet->addDescriptorBinding("jointMatrices", "VSG_SKINNING", MATERIAL_DESCRIPTOR_SET, 12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage, vsg::mat4Value::create());

        shaderSet->addDescriptorBinding("lightData", "", VIEW_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));
        shaderSet->addDescriptorBinding("viewportData", "", VIEW_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, meshShaderStage | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Value::create(0, 0, 1280, 1024));
        shaderSet->addDescriptorBinding("shadowMaps", "", VIEW_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
        shaderSet->addDescriptorBinding("shadowMapDirectSampler", "VSG_SHADOWS_PCSS", VIEW_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
        shaderSet->addDescriptorBinding("shadowMapShadowSampler", "", VIEW_DESCRIPTOR_SET, 4, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);

        // additional defines
        shaderSet->optionalDefines = {"MESHLET_DEBUG_COLORS", "VSG_GREYSCALE_DIFFUSE_MAP", "VSG_TWO_SIDED_LIGHTING", "VSG_POINT_SPRITE", "VSG_WORKFLOW_SPECGLOSS", "VSG_SHADOWS_PCSS", "VSG_SHADOWS_SOFT", "VSG_SHADOWS_HARD", "SHADOWMAP_DEBUG", "VSG_ALPHA_TEST"};

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

        shaderSet->geometryHints = vsg::ShaderSet::DRAWMESHTASKS | vsg::ShaderSet::MESHLETS;

        if (debugColors)
        {
            if (!shaderSet->defaultShaderHints) shaderSet->defaultShaderHints = vsg::ShaderCompileSettings::create();
            shaderSet->defaultShaderHints->defines.insert("MESHLET_DEBUG_COLORS");
        }

        if (useTaskShader)
        {
            if (!shaderSet->defaultShaderHints) shaderSet->defaultShaderHints = vsg::ShaderCompileSettings::create();
            shaderSet->defaultShaderHints->defines.insert("TASK_SHADER");

        }

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
        vsg::ref_ptr<vsg::ShaderSet> mesh_shaderSet;

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
#if 0
            if (name=="tileset_vegetation")
            {
                auto local_options = vsg::clone(in_options);
                local_options->setObject(vsgXchange::gltf::prototype_builder, custom::MeshGltfBuilder::create());

                vsg::debug("Vegetation : MeshTiles3DBuilder::createSceneGraph(", tileset, ", ", in_options,") ", this, ", filename = ", tileset->filename, ", local_options = ", local_options);

                object = Inherit::createSceneGraph(tileset, local_options);

                vsg::debug("after ", tileset->filename, "\n\n");
            }
            else
#endif
            if (name=="tileset_terrain")
            {
                auto local_options = vsg::clone(in_options);
                // local_options->setObject(vsgXchange::gltf::prototype_builder, custom::MeshGltfBuilder::create());

                if (!mesh_shaderSet) mesh_shaderSet = custom::pbr_ShaderSet(in_options);

                if (mesh_shaderSet)
                {
                    local_options->shaderSets["pbr"] = mesh_shaderSet;
                }

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
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        std::cout<<arguments<<std::endl;

        auto windowTraits = vsg::WindowTraits::create(arguments);

        // use the vsg::Options object to pass the ReaderWriter_all to use when reading files.
        auto options = vsg::Options::create();

        arguments.readAndAssign<bool>(custom::task_option, options);
        arguments.readAndAssign<bool>(custom::debug_option, options);

        options->add(vsgXchange::all::create());

        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
        options->sharedObjects = vsg::SharedObjects::create();

        auto outputFilename = arguments.value<vsg::Path>("", "-o");
        auto outputShaderSetFilename = arguments.value<vsg::Path>("", "--os");

        if (arguments.read({"--custom-bulders", "--cb"}))
        {
            options->setObject(vsgXchange::gltf::prototype_builder, custom::MeshGltfBuilder::create());
            options->setObject(vsgXchange::Tiles3D::prototype_builder, custom::MeshTiles3DBuilder::create());
        }

        if (arguments.read({"--tile"}))
        {
            auto builder = custom::MeshTiles3DBuilder::create();
            builder->mesh_shaderSet = custom::pbr_ShaderSet(options);
            options->setObject(vsgXchange::Tiles3D::prototype_builder, custom::MeshTiles3DBuilder::create());

            if (builder->mesh_shaderSet && outputShaderSetFilename)
            {
                vsg::write(builder->mesh_shaderSet, outputShaderSetFilename, options);
            }
        }
        else if (!arguments.read("--standard"))
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

        // read any command line options that the ReaderWriters support
        options->readOptions(arguments);


        if (int log_level = 0; arguments.read("--log-level", log_level)) vsg::Logger::instance()->level = vsg::Logger::Level(log_level);
        int pd_num = arguments.value<int>(-1, "--select");
        auto numFrames = arguments.value(-1, "-f");
        auto pathFilename = arguments.value<vsg::Path>("", "-p");
        auto autoPlay = !arguments.read({"--no-auto-play", "--nop"});
        auto maxTime = arguments.value(std::numeric_limits<double>::max(), "--max-time");
        bool baricentric = arguments.read({"--baricentric", "-b"});
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

        uint64_t initialFrameCycleCount = arguments.value(reportAverageFrameRate ? 10 : 0, "--ifcc");

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

        //windowTraits->vulkanVersion = VK_API_VERSION_1_1;
        windowTraits->vulkanVersion = VK_API_VERSION_1_2;
        windowTraits->deviceExtensionNames = {VK_EXT_MESH_SHADER_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME};

        // set up features
        auto& features = windowTraits->deviceFeatures;
        features->get().shaderInt64 = VK_TRUE;

        auto& meshFeatures = features->get<VkPhysicalDeviceMeshShaderFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT>();
        meshFeatures.meshShader = VK_TRUE;
        meshFeatures.taskShader = VK_TRUE;

#if 1
        auto& vulkan12Features = features->get<VkPhysicalDeviceVulkan12Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES>();
        vulkan12Features.shaderInt8 = VK_TRUE;
        vulkan12Features.storageBuffer8BitAccess = VK_TRUE;
        vulkan12Features.uniformAndStorageBuffer8BitAccess = VK_TRUE;
#else
        windowTraits->deviceExtensionNames.push_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);
        auto& eightBitStorageFeatures = features->get<VkPhysicalDevice8BitStorageFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES>();
        eightBitStorageFeatures.storageBuffer8BitAccess = VK_TRUE;
        eightBitStorageFeatures.uniformAndStorageBuffer8BitAccess = VK_TRUE;
#endif

        if (baricentric)
        {
            windowTraits->deviceExtensionNames.push_back(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
            auto& barycentricFeatures = features->get<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR>();
            barycentricFeatures.fragmentShaderBarycentric = VK_TRUE;
        }



        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create window." << std::endl;
            return 1;
        }


        if (pd_num >= 0)
        {
            // use the Window implementation to create the Instance and Surface
            auto instance = window->getOrCreateInstance();
            auto surface = window->getOrCreateSurface();

            auto physicalDevices = instance->getPhysicalDevices();
            if (physicalDevices.empty())
            {
                std::cout << "No physical devices reported." << std::endl;
                return 0;
            }

            if (pd_num >= static_cast<int>(physicalDevices.size()))
            {
                std::cout << "--select " << pd_num << ", exceeds physical devices available, maximum permitted value is " << physicalDevices.size() - 1 << std::endl;
                return 0;
            }

            // create a vk/vsg::PhysicalDevice, prefer discrete GPU over integrated GPUs when they are available.
            auto physicalDevice = physicalDevices[pd_num];
            auto properties = physicalDevice->getProperties();

            std::cout << "Selected vsg::PhysicalDevice " << physicalDevice << " " << properties.deviceName << " deviceType = " << properties.deviceType << std::endl;

            window->setPhysicalDevice(physicalDevice);
        }


        auto physicalDevice = window->getOrCreatePhysicalDevice();
        auto subgroupProperties = physicalDevice->getProperties<VkPhysicalDeviceSubgroupProperties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES>();

        vsg::info("subgroupSize = ", subgroupProperties.subgroupSize);
        vsg::info("supportedStages = ", subgroupProperties.supportedStages);
        vsg::info("supportedOperations = ", subgroupProperties.supportedOperations);
        vsg::info("quadOperationsInAllStages = ", subgroupProperties.quadOperationsInAllStages);

        viewer->addWindow(window);

        auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");

        vsg::ref_ptr<vsg::LookAt> lookAt;
        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        if (ellipsoidModel)
        {
            // compute the bounds of the scene graph to help position camera
            vsg::ComputeBounds computeBounds;
            vsg_scene->accept(computeBounds);

            double initialRadius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5;
            double modelToEarthRatio = (initialRadius / ellipsoidModel->radiusEquator());

            // if the model is small compared to the radius of the earth position camera in local coordinate frame of the model rather than ECEF.
            if (modelToEarthRatio < 1.0)
            {
                vsg::dvec3 lla = ellipsoidModel->convertECEFToLatLongAltitude((computeBounds.bounds.min + computeBounds.bounds.max) * 0.5);

                auto worldToLocal = ellipsoidModel->computeWorldToLocalTransform(lla);
                auto localToWorld = ellipsoidModel->computeLocalToWorldTransform(lla);

                // recompute the bounds of the model in the local coordinate frame of the model, rather than ECEF
                // to give a tigher bound around the dataset.
                computeBounds.matrixStack.clear();
                computeBounds.matrixStack.push_back(worldToLocal);
                computeBounds.bounds.reset();
                vsg_scene->accept(computeBounds);

                auto bounds = computeBounds.bounds;
                vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;
                double radius = vsg::length(bounds.max - bounds.min) * 0.5;

                lookAt = vsg::LookAt::create(localToWorld * (centre + vsg::dvec3(0.0, 0.0, radius)), localToWorld * centre, vsg::dvec3(0.0, 1.0, 0.0) * worldToLocal);
            }
            else
            {
                lookAt = vsg::LookAt::create(vsg::dvec3(initialRadius * 2.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0));
            }

            perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
        }
        else
        {
            // compute the bounds of the scene graph to help position camera
            vsg::ComputeBounds computeBounds;
            vsg_scene->accept(computeBounds);

            vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
            double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

            // set up the camera
            lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
            perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.5);
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

        if (initialFrameCycleCount > 0)
        {
            // run an initial set of frames to get past the intiial frame time variability so we get stable frame rate stats
            while ((initialFrameCycleCount-- > 0) && viewer->advanceToNextFrame())
            {
                viewer->getFrameStamp()->simulationTime = 0.0;
                viewer->handleEvents();
                viewer->update();
                viewer->recordAndSubmit();
                viewer->present();
            }
        }

        viewer->start_point() = vsg::clock::now();
        viewer->getFrameStamp()->frameCount = 0;
        viewer->getFrameStamp()->simulationTime = 0.0;

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
    }
    catch (const vsg::Exception& ve)
    {
        for (int i = 0; i < argc; ++i) std::cerr << argv[i] << " ";
        std::cerr << "\n[Exception] - " << ve.message << " result = " << ve.result << std::endl;
        return 1;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
