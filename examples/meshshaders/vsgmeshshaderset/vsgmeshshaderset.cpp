#include <iostream>
#include <vsg/all.h>
#include <vsgXchange/all.h>
#include <vsgXchange/gltf.h>
#include <vsgXchange/3DTiles.h>

#ifdef meshoptimizer_FOUND
#include <meshoptimizer.h>
#endif

namespace custom
{
    const char* mesh_option = "mesh";

    vsg::ref_ptr<vsg::ShaderSet> pbr_ShaderSet(vsg::ref_ptr<const vsg::Options> options)
    {
        bool meshShader = vsg::value<bool>(false, custom::mesh_option, options);

        vsg::info("Local pbr_ShaderSet(", options, ") meshShader = ", meshShader);

        auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/standard.vert", options);
        auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/wireframe_pbr.frag", options);

        if (!vertexShader || !fragmentShader)
        {
            vsg::error("pbr_ShaderSet(...) could not find shaders.");
            return {};
        }

        auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

        #define VIEW_DESCRIPTOR_SET 0
        #define MATERIAL_DESCRIPTOR_SET 1
        #define ARRAY_DESCRIPTOR_SET 2

        if (meshShader)
        {
            vsg::info("MESH SHADER");

            // array bindings as descriptors
            VkShaderStageFlags meshShaderStage = VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT;

            shaderSet->addDescriptorBinding("vsg_Vertex", "", ARRAY_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage, vsg::vec3Array::create(1));
            shaderSet->addDescriptorBinding("vsg_Normal", "", ARRAY_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage, vsg::vec3Array::create(1));
            shaderSet->addDescriptorBinding("vsg_TexCoord0", "VSG_TEXTURECOORD_0", ARRAY_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage, vsg::vec2Array::create(1));
            shaderSet->addDescriptorBinding("vsg_TexCoord1", "VSG_TEXTURECOORD_1", ARRAY_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage, vsg::vec2Array::create(1));
            shaderSet->addDescriptorBinding("vsg_TexCoord2", "VSG_TEXTURECOORD_2", ARRAY_DESCRIPTOR_SET, 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage, vsg::vec2Array::create(1));
            shaderSet->addDescriptorBinding("vsg_TexCoord3", "VSG_TEXTURECOORD_3", ARRAY_DESCRIPTOR_SET, 5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage, vsg::vec2Array::create(1));
            shaderSet->addDescriptorBinding("vsg_Color", "", ARRAY_DESCRIPTOR_SET, 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage, vsg::vec4Array::create(1), vsg::CoordinateSpace::LINEAR);

            shaderSet->addDescriptorBinding("vsg_Translation_scaleDistance", "VSG_BILLBOARD", ARRAY_DESCRIPTOR_SET, 7, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage, vsg::vec4Array::create(1));

            shaderSet->addDescriptorBinding("vsg_Translation", "VSG_INSTANCE_TRANSLATION", ARRAY_DESCRIPTOR_SET, 7, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage, vsg::vec3Array::create(1));
            shaderSet->addDescriptorBinding("vsg_Rotation", "VSG_INSTANCE_ROTATION", ARRAY_DESCRIPTOR_SET, 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage, vsg::quatArray::create(1));
            shaderSet->addDescriptorBinding("vsg_Scale", "VSG_INSTANCE_SCALE", ARRAY_DESCRIPTOR_SET, 9, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage, vsg::vec3Array::create(1));

            shaderSet->addDescriptorBinding("vsg_JointIndices", "VSG_SKINNING", ARRAY_DESCRIPTOR_SET, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage, vsg::ivec4Array::create(1));
            shaderSet->addDescriptorBinding("vsg_JointWeights", "VSG_SKINNING", ARRAY_DESCRIPTOR_SET, 11, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, meshShaderStage, vsg::vec4Array::create(1));
        }
        else
        {
            vsg::info("VERTEX SHADER");

            // array bindings
            shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
            shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
            shaderSet->addAttributeBinding("vsg_TexCoord0", "VSG_TEXTURECOORD_0", 2, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Array::create(1));
            shaderSet->addAttributeBinding("vsg_TexCoord1", "VSG_TEXTURECOORD_1", 3, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Array::create(1));
            shaderSet->addAttributeBinding("vsg_TexCoord2", "VSG_TEXTURECOORD_2", 4, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Array::create(1));
            shaderSet->addAttributeBinding("vsg_TexCoord3", "VSG_TEXTURECOORD_3", 5, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Array::create(1));
            shaderSet->addAttributeBinding("vsg_Color", "", 6, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1), vsg::CoordinateSpace::LINEAR);

            shaderSet->addAttributeBinding("vsg_Translation_scaleDistance", "VSG_BILLBOARD", 7, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));

            shaderSet->addAttributeBinding("vsg_Translation", "VSG_INSTANCE_TRANSLATION", 7, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
            shaderSet->addAttributeBinding("vsg_Rotation", "VSG_INSTANCE_ROTATION", 8, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::quatArray::create(1));
            shaderSet->addAttributeBinding("vsg_Scale", "VSG_INSTANCE_SCALE", 9, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

            shaderSet->addAttributeBinding("vsg_JointIndices", "VSG_SKINNING", 10, VK_FORMAT_R32G32B32A32_SINT, vsg::ivec4Array::create(1));
            shaderSet->addAttributeBinding("vsg_JointWeights", "VSG_SKINNING", 11, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));
        }

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

        shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_TRANSLATION"}, vsg::TranslationArrayState::create()});
        shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_TRANSLATION", "VSG_INSTANCE_ROTATION", "VSG_INSTANCE_SCALE"}, vsg::TranslationRotationScaleArrayState::create()});
        shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_TRANSLATION", "VSG_DISPLACEMENT_MAP"}, vsg::TranslationAndDisplacementMapArrayState::create()});
        shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_DISPLACEMENT_MAP"}, vsg::DisplacementMapArrayState::create()});
        shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_BILLBOARD"}, vsg::BillboardArrayState::create()});

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
            vsg::info("MeshGltfBuilder::createSceneGraph(", in_model, ", ", in_options,") ", this,", filename = ", in_model->filename);
            return Inherit::createSceneGraph(in_model, in_options);
        }

        vsg::ref_ptr<vsg::Node> createMesh(vsg::ref_ptr<vsgXchange::gltf::Mesh> gltf_mesh, const vsgXchange::gltf::Builder::MeshExtras& meshExtras) override;

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
            vsg::info("MeshTiles3DBuilder::createTile(tile = ", tile, ", level = ", level, ", ", inherited_refine, ") options = ", options);
            return Inherit::createTile(tile, level, inherited_refine);
        }


        vsg::ref_ptr<vsg::Node> readInstanceChild(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> in_options) override
        {
            vsg::info("Vegetation : MeshTiles3DBuilder::readInstanceChild( ", filename, ", ", options, " )");
            return Inherit::readInstanceChild(filename, in_options);
        }

        vsg::ref_ptr<vsg::Node> readInstanceChild(std::istream& fin, vsg::ref_ptr<const vsg::Options> in_options) override
        {
            vsg::info("Vegetation : MeshTiles3DBuilder::readInstanceChild( ", &fin, ", ", options, " )");
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

                vsg::info("Vegetation : MeshTiles3DBuilder::createSceneGraph(", tileset, ", ", in_options,") ", this, ", filename = ", tileset->filename, ", local_options = ", local_options);

                object = Inherit::createSceneGraph(tileset, local_options);

                vsg::info("after ", tileset->filename, "\n\n");
            }
            else if (name=="tileset_terrain")
            {
                auto local_options = vsg::clone(in_options);
                local_options->setObject(vsgXchange::gltf::prototype_builder, custom::MeshGltfBuilder::create());

                vsg::info("Terrain : MeshTiles3DBuilder::createSceneGraph(", tileset, ", ", in_options,") ", this, ", filename = ", tileset->filename, ", local_options = ", local_options);

                object = Inherit::createSceneGraph(tileset, local_options);
                vsg::info("after ", tileset->filename, "\n\n");
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

vsg::ref_ptr<vsg::Node> custom::MeshGltfBuilder::createMesh(vsg::ref_ptr<vsgXchange::gltf::Mesh> gltf_mesh, const vsgXchange::gltf::Builder::MeshExtras& meshExtras)
{
#if 0
    vsg::info("MeshGltfBuilder::createMesh(", gltf_mesh, ", ..) ", this);
    return Inherit::createMesh(gltf_mesh, meshExtras);
#else

    vsg::info("MeshGltfBuilder::createMesh(", gltf_mesh, ", ..) ", this, " custom mesh, options = ", options);

    /*
    struct Attributes : public vsg::Inherit<vsg::JSONParser::Schema, Attributes>
    {
        std::map<std::string, glTFid> values;
    };

    struct Primitive : public vsg::Inherit<ExtensionsExtras, Primitive>
    {
        Attributes attributes;
        glTFid indices;
        glTFid material;
        uint32_t mode = 0;
        vsg::ObjectsSchema<Attributes> targets;
    };

    struct Mesh : public vsg::Inherit<NameExtensionsExtras, Mesh>
    {
        vsg::ObjectsSchema<Primitive> primitives;
        vsg::ValuesSchema<double> weights;
    };
    */

    const VkPrimitiveTopology topologyLookup[] = {
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST,     // 0, POINTS
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST,      // 1, LINES
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST,      // 2, LINE_LOOP, need special handling
        VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,     // 3, LINE_STRIP
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,  // 4, TRIANGLES
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // 5, TRIANGLE_STRIP
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN    // 6, TRIANGLE_FAN
    };
#if 0
    vsg::info("mesh = {");
    vsg::info("    primitives = ", gltf_mesh->primitives.values.size());
    vsg::info("    weight = ", gltf_mesh->weights.values.size());
#endif

    std::vector<vsg::ref_ptr<vsg::Node>> nodes;

    for (auto& primitive : gltf_mesh->primitives.values)
    {
        vsg::ref_ptr<vsg::DescriptorConfigurator> vsg_material;
        if (primitive->material)
        {
            vsg_material = vsg_materials[primitive->material.value];
        }
        else
        {
            vsg::debug("Material for primitive not assigned, primitive = ", primitive, ", primitive->material = ", primitive->material);
            vsg_material = default_material;
        }

        auto config = vsg::GraphicsPipelineConfigurator::create(vsg_material->shaderSet);
        config->descriptorConfigurator = vsg_material;
        if (options) config->assignInheritedState(options->inheritedState);

        if (meshExtras.jointSampler)
        {
            vsg_material->assignDescriptor("jointMatrices", meshExtras.jointSampler->jointMatrices);
        }

#if 0
        vsg::info("    primitive = {");
        vsg::info("        attributes = {");
        for(auto& [semantic, id] : primitive->attributes.values)
        {
             vsg::info("            ", semantic, ", ", id);
        }
        vsg::info("        }");
        vsg::info("        indices = ", primitive->indices);
        vsg::info("        material = ", primitive->material);
        vsg::info("        mode = ", primitive->mode);
        vsg::info("        targets = ", primitive->targets.values.size()) ;

        vsg::info("        * topology = ", topologyLookup[primitive->mode]) ;
        if (primitive->mode==2) vsg::info("        * LINE_LOOP needs special handling.");

        vsg::info("    }");
#endif

        vsg::DataList vertexArrays;

        auto assignArray = [&](vsgXchange::gltf::Attributes& attrib, VkVertexInputRate vertexInputRate, const std::string& attribute_name) -> bool {

            auto array_itr = attrib.values.find(attribute_name);
            if (array_itr == attrib.values.end()) return false;

            auto name_itr = attributeLookup.find(attribute_name);
            if (name_itr == attributeLookup.end()) return false;

            if (array_itr->second.value >= vsg_accessors.size())
            {
                vsg::warn("gltf::Builder::createMesh() error in assignArray( attrib, vertexIndexRate", attribute_name, "), array index out of range.");
                return false;
            }

            vsg::ref_ptr<vsg::Data> array = vsg_accessors[array_itr->second.value];
            if (!array)
            {
                vsg::warn("gltf::Builder::createMesh() error in assignArray( attrib, vertexIndexRate", attribute_name, "), required array null.");
                return false;
            }

            if (attribute_name == "ROTATION")
            {
                if (auto vec4Rotations = array.cast<vsg::vec4Array>())
                {
                    auto quatArray = vsg::quatArray::create(array, 0, 12, vec4Rotations->size());
                    array = quatArray;
                }
            }
            else if (attribute_name == "TEXCOORD_0" || attribute_name == "TEXCOORD_1" || attribute_name == "TEXCOORD_2" || attribute_name == "TEXCOORD_3")
            {
                if (auto texture_transform = vsg_material->getObject<vsgXchange::gltf::KHR_texture_transform>("KHR_texture_transform"))
                {
                    vsg::vec2 offset(0.0f, 0.0f);
                    vsg::vec2 scale(1.0f, 1.0f);
                    float rotation = texture_transform->rotation;
                    if (texture_transform->offset.values.size() >= 2) offset.set(texture_transform->offset.values[0], texture_transform->offset.values[1]);
                    if (texture_transform->scale.values.size() >= 2) scale.set(texture_transform->scale.values[0], texture_transform->scale.values[1]);

                    if (auto texCoords = array.cast<vsg::vec2Array>())
                    {
                        float sin_rotation = std::sin(rotation);
                        float cos_rotation = std::cos(rotation);
                        auto transformedTexCoords = vsg::vec2Array::create(texCoords->size());
                        auto dest_itr = transformedTexCoords->begin();
                        for (auto& tc : *texCoords)
                        {
                            auto& dest_tc = *(dest_itr++);
                            dest_tc.x = offset.x + (tc.x * scale.x) * cos_rotation + (tc.y * scale.y) * sin_rotation;
                            dest_tc.y = offset.y + (tc.y * scale.y) * cos_rotation - (tc.x * scale.x) * sin_rotation;
                        }
                        array = transformedTexCoords;
                    }
                }
            }
            else if (attribute_name == "JOINTS_0")
            {
                if (auto ushortCoords = array.cast<vsg::usvec4Array>())
                {
                    auto intCoords = vsg::ivec4Array::create(ushortCoords->size());
                    auto dest_itr = intCoords->begin();
                    for (auto& usc : *ushortCoords)
                    {
                        *(dest_itr++) = vsg::ivec4(usc[0], usc[1], usc[2], usc[3]);
                    }

                    array = intCoords;
                }
                else if (auto ubyteCoords = array.cast<vsg::ubvec4Array>())
                {
                    auto intCoords = vsg::ivec4Array::create(ubyteCoords->size());
                    auto dest_itr = intCoords->begin();
                    for (auto& ubc : *ubyteCoords)
                    {
                        *(dest_itr++) = vsg::ivec4(ubc[0], ubc[1], ubc[2], ubc[3]);
                    }

                    array = intCoords;
                }
            }

            config->assignArray(vertexArrays, name_itr->second, vertexInputRate, array);

            vsg::info("assignArray(, vertexInputRate = ", vertexInputRate, ", name = ", attribute_name, ") vertexArrays = { ", vertexArrays, " }, name_itr->second = ", name_itr->second, ", array = ", array);
            return true;
        };

        if (!assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "POSITION"))
        {
            vsg::warn("gltf::Builder::createMesh() error no vertex array assigned.");
            return {};
        }

        if (!assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "NORMAL"))
        {
            auto normal = vsg::vec3Value::create(vsg::vec3(0.0f, 0.0f, 1.0f));
            config->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_INSTANCE, normal);
        }

        if (!assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "TEXCOORD_0"))
        {
            auto texcoord = vsg::vec2Value::create(vsg::vec2(0.0f, 0.0f));
            config->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_INSTANCE, texcoord);
        }

        assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "TEXCOORD_1");
        assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "TEXCOORD_2");
        assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "TEXCOORD_3");

        uint32_t vertexCount = static_cast<uint32_t>(vertexArrays.front()->valueCount());
        uint32_t instanceCount = 1;
        if (meshExtras.instancedAttributes)
        {
            for (auto& [name, id] : meshExtras.instancedAttributes->values)
            {
                instanceCount = static_cast<uint32_t>(vsg_accessors[id.value]->valueCount());
            }
        }

        if (!assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "COLOR_0"))
        {
            if (instanceNodeHint == vsg::Options::INSTANCE_NONE)
            {
                auto defaultColor = vsg::vec4Array::create(instanceCount, vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                config->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, defaultColor);
            }
            else if ((instanceNodeHint & vsg::Options::INSTANCE_COLORS) == 0)
            {
                auto defaultColor = vsg::vec4Array::create(vertexCount, vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                config->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_VERTEX, defaultColor);
            }
        }

        if (meshExtras.jointSampler)
        {
            assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "JOINTS_0");
            assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "WEIGHTS_0");
        }

        if (meshExtras.instancedAttributes)
        {
            assignArray(*meshExtras.instancedAttributes, VK_VERTEX_INPUT_RATE_INSTANCE, "TRANSLATION");
            assignArray(*meshExtras.instancedAttributes, VK_VERTEX_INPUT_RATE_INSTANCE, "ROTATION");
            assignArray(*meshExtras.instancedAttributes, VK_VERTEX_INPUT_RATE_INSTANCE, "SCALE");
        }

        vsg::ref_ptr<vsg::Node> draw;

        if (!meshExtras.instancedAttributes && instanceNodeHint != vsg::Options::INSTANCE_NONE)
        {
            if ((instanceNodeHint & vsg::Options::INSTANCE_COLORS) != 0) config->enableArray("vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, 16, VK_FORMAT_R32G32B32A32_SFLOAT);
            if ((instanceNodeHint & vsg::Options::INSTANCE_TRANSLATIONS) != 0) config->enableArray("vsg_Translation", VK_VERTEX_INPUT_RATE_INSTANCE, 12, VK_FORMAT_R32G32B32_SFLOAT);
            if ((instanceNodeHint & vsg::Options::INSTANCE_ROTATIONS) != 0) config->enableArray("vsg_Rotation", VK_VERTEX_INPUT_RATE_INSTANCE, 16, VK_FORMAT_R32G32B32A32_SFLOAT);
            if ((instanceNodeHint & vsg::Options::INSTANCE_SCALES) != 0) config->enableArray("vsg_Scale", VK_VERTEX_INPUT_RATE_INSTANCE, 12, VK_FORMAT_R32G32B32_SFLOAT);

            if (primitive->indices)
            {
                auto instanceDrawIndexed = vsg::InstanceDrawIndexed::create();
                assign_extras(*primitive, *instanceDrawIndexed);
                instanceDrawIndexed->assignArrays(vertexArrays);

                auto indices = vsg_accessors[primitive->indices.value];
                if (!indices)
                {
                    vsg::warn("gltf::Builder::createMesh() error required indices array null.");
                    return {};
                }

                if (auto ubyte_indices = indices.cast<vsg::ubyteArray>())
                {
                    // need to promote ubyte indices to ushort as Vulkan requires an extension to be enabled for ubyte indices.
                    auto ushort_indices = vsg::ushortArray::create(ubyte_indices->size());
                    auto itr = ushort_indices->begin();
                    for (auto value : *ubyte_indices)
                    {
                        *(itr++) = static_cast<uint16_t>(value);
                    }

                    instanceDrawIndexed->assignIndices(ushort_indices);
                    instanceDrawIndexed->indexCount = static_cast<uint32_t>(ushort_indices->valueCount());
                }
                else
                {
                    instanceDrawIndexed->assignIndices(indices);
                    instanceDrawIndexed->indexCount = static_cast<uint32_t>(indices->valueCount());
                }
                draw = instanceDrawIndexed;
            }
            else
            {
                auto instanceDraw = vsg::InstanceDraw::create();
                assign_extras(*primitive, *instanceDraw);
                instanceDraw->assignArrays(vertexArrays);

                assign_extras(*primitive, *instanceDraw);
                instanceDraw->vertexCount = vertexCount;
                draw = instanceDraw;
            }
        }
        else if (primitive->indices)
        {
            auto vid = vsg::VertexIndexDraw::create();
            assign_extras(*primitive, *vid);
            vid->assignArrays(vertexArrays);
            vid->instanceCount = instanceCount;

            auto indices = vsg_accessors[primitive->indices.value];
            if (!indices)
            {
                vsg::warn("gltf::Builder::createMesh() error required indices array null.");
                return {};
            }

            if (auto ubyte_indices = indices.cast<vsg::ubyteArray>())
            {
                // need to promote ubyte indices to ushort as Vulkan requires an extension to be enabled for ubyte indices.
                auto ushort_indices = vsg::ushortArray::create(ubyte_indices->size());
                auto itr = ushort_indices->begin();
                for (auto value : *ubyte_indices)
                {
                    *(itr++) = static_cast<uint16_t>(value);
                }

                vid->assignIndices(ushort_indices);
                vid->indexCount = static_cast<uint32_t>(ushort_indices->valueCount());
            }
            else
            {
                vid->assignIndices(indices);
                vid->indexCount = static_cast<uint32_t>(indices->valueCount());
            }

            draw = vid;
        }
        else
        {
            auto vd = vsg::VertexDraw::create();
            assign_extras(*primitive, *vd);
            vd->assignArrays(vertexArrays);
            vd->instanceCount = instanceCount;
            vd->vertexCount = vertexCount;
            draw = vd;
        }

        // set the GraphicsPipelineStates to the required values.
        struct SetPipelineStates : public vsg::Visitor
        {
            VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            bool blending = false;
            bool two_sided = false;

            SetPipelineStates(VkPrimitiveTopology in_topology, bool in_blending, bool in_two_sided) :
                topology(in_topology), blending(in_blending), two_sided(in_two_sided) {}

            void apply(vsg::Object& object) { object.traverse(*this); }
            void apply(vsg::RasterizationState& rs)
            {
                if (two_sided) rs.cullMode = VK_CULL_MODE_NONE;
            }
            void apply(vsg::InputAssemblyState& ias) { ias.topology = topology; }
            void apply(vsg::ColorBlendState& cbs) { cbs.configureAttachments(blending); }

        } sps(topologyLookup[primitive->mode], vsg_material->blending, vsg_material->two_sided);

        config->accept(sps);

        vsg::info("  config->pipelineStates = { }", config->pipelineStates, " }");

        vsg::info("config->shaderSet->attributeBindings = {");
        for(auto& attributeBinding : config->shaderSet->attributeBindings)
        {
            vsg::info("    AttributeBinding{ name = ", attributeBinding.name, ", location = ", attributeBinding.location, " }");
        }
        vsg::info("}");

        // https://github.com/zeux/meshoptimizer
        // std::vector<unsigned int> remap(total_indices);
        //
        // size_t total_vertices = meshopt_generateVertexRemap(&remap[0], NULL, total_indices, &vertices[0], total_indices, sizeof(Vertex));
        //
        // result.indices.resize(total_indices);
        // meshopt_remapIndexBuffer(&result.indices[0], NULL, total_indices, &remap[0]);
        //
        // result.vertices.resize(total_vertices);
        // meshopt_remapVertexBuffer(&result.vertices[0], &vertices[0], total_indices, sizeof(Vertex), &remap[0]);

        struct PrintPipelineStates : public vsg::Visitor
        {
            void apply(vsg::Object& object) { object.traverse(*this); }
            void apply(vsg::VertexInputState& vis)
            {
                vsg::info("vis.vertexAttributeDescriptions = {");
                for(auto& attributeDesc : vis.vertexAttributeDescriptions)
                {
                    vsg::info("    { location = ", attributeDesc.location, ", binding = ", attributeDesc.binding, ", format = ", attributeDesc.format, ", offset = ", attributeDesc.offset, " }");
                }
                vsg::info("}");

                vsg::info("vis.vertexBindingDescriptions = {");
                for(auto& bindingDesc : vis.vertexBindingDescriptions)
                {
                    vsg::info("    { binding = ", bindingDesc.binding, ", stride = ", bindingDesc.stride, ", inputRate = ", bindingDesc.inputRate, " }");
                }
                vsg::info("}");
            }
        } print;

        config->accept(print);

        if (sharedObjects)
            sharedObjects->share(config, [](auto gpc) { gpc->init(); });
        else
            config->init();

        // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
        auto stateGroup = vsg::StateGroup::create();

        config->copyTo(stateGroup, sharedObjects);

        stateGroup->addChild(draw);

        if (vsg_material->blending)
        {
            if (meshExtras.instancedAttributes || instanceNodeHint != vsg::Options::INSTANCE_NONE)
            {
#if 0
                auto layer = vsg::Layer::create();
                layer->binNumber = 10;
                layer->child = stateGroup;

                nodes.push_back(layer);
#else
                nodes.push_back(stateGroup);
#endif
            }
            else
            {
                vsg::ComputeBounds computeBounds;
                draw->accept(computeBounds);
                vsg::dvec3 center = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
                double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5;

                auto depthSorted = vsg::DepthSorted::create();
                depthSorted->binNumber = 10;
                depthSorted->bound.set(center[0], center[1], center[2], radius);
                depthSorted->child = stateGroup;

                nodes.push_back(depthSorted);
            }
        }
        else
        {
            nodes.push_back(stateGroup);
        }
    }

    if (nodes.empty())
    {
        vsg::warn("Empty mesh");
        return {};
    }

    vsg::ref_ptr<vsg::Node> vsg_mesh;
    if (nodes.size() == 1)
    {
        // vsg::info("Mesh with single primtiive");
        vsg_mesh = nodes.front();
    }
    else
    {
        // vsg::info("Mesh with multiple primtiives - could possible use vsg::Geomterty.");
        auto group = vsg::Group::create();
        for (auto node : nodes)
        {
            group->addChild(node);
        }

        vsg_mesh = group;
    }

    assign_name_extras(*gltf_mesh, *vsg_mesh);

    return vsg_mesh;

#endif
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

    arguments.readAndAssign<bool>(custom::mesh_option, options);

    auto numFrames = arguments.value(-1, "-f");
    auto pathFilename = arguments.value<vsg::Path>("", "-p");
    auto autoPlay = !arguments.read({"--no-auto-play", "--nop"});
    auto maxTime = arguments.value(std::numeric_limits<double>::max(), "--max-time");
    auto outputFilename = arguments.value<vsg::Path>("", "-o");
    auto outputShaderSetFilename = arguments.value<vsg::Path>("", "--os");
    auto horizonMountainHeight = arguments.value(0.0, "--hmh");
    auto nearFarRatio = arguments.value<double>(0.001, "--nfr");
    bool reportAverageFrameRate = arguments.read("--fps");

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

    auto vsg_scene = vsg::Group::create();

    vsg::Path path;

    // read any vsg files
    for (int i = 1; i < argc; ++i)
    {
        vsg::Path filename = arguments[i];
        if (auto node = vsg::read_cast<vsg::Node>(filename, options))
        {
            vsg_scene->addChild(node);
        }
        else
        {
            std::cout << "Unable to load file " << filename << std::endl;
        }
    }

    if (vsg_scene->children.empty())
    {
        return 1;
    }

    if (outputFilename)
    {
        vsg::write(vsg_scene, outputFilename, options);
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    windowTraits->vulkanVersion = VK_API_VERSION_1_1;
    windowTraits->deviceExtensionNames = {VK_EXT_MESH_SHADER_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME};
    windowTraits->deviceExtensionNames.push_back(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);

    // set up features
    auto& features = windowTraits->deviceFeatures;

    auto& meshFeatures = features->get<VkPhysicalDeviceMeshShaderFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT>();
    meshFeatures.meshShader = VK_TRUE;
    meshFeatures.taskShader = VK_TRUE;

    auto& barycentricFeatures = features->get<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR>();
    barycentricFeatures.fragmentShaderBarycentric = VK_TRUE;

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

    vsg::info("scene bounds ", bounds);

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
        perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    auto cameraAnimation = vsg::CameraAnimationHandler::create(camera, pathFilename, options);
    viewer->addEventHandler(cameraAnimation);
    if (autoPlay && cameraAnimation->animation)
    {
        cameraAnimation->play();

        if (reportAverageFrameRate && maxTime == std::numeric_limits<double>::max())
        {
            maxTime = cameraAnimation->animation->maxTime();
        }
    }

    viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

    auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile();

    viewer->start_point() = vsg::clock::now();

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
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
