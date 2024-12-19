#include <vsg/all.h>

#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <unordered_map>

class UpdateShaderVisitor : public vsg::Inherit<vsg::Visitor, UpdateShaderVisitor>
{
public:
    UpdateShaderVisitor(vsg::ref_ptr<vsg::ShaderSet> in_shaderSet) :
        shaderSet(std::move(in_shaderSet))
    {}

    vsg::ref_ptr<vsg::ShaderSet> shaderSet;
    std::set<vsg::GraphicsPipeline*> visited = {};

    void apply(vsg::Node& node) override
    {
        node.traverse(*this);
    }

    void apply(vsg::BindGraphicsPipeline& bgp) override
    {
        auto graphicsPipeline = bgp.pipeline;
        if (visited.count(graphicsPipeline.get()) > 0)
        {
            return;
        }

        visited.insert(graphicsPipeline);

        vsg::ref_ptr<vsg::ShaderCompileSettings> hints = nullptr;
        for (const auto& stage : graphicsPipeline->stages)
        {
            if (stage->module && stage->module->hints)
                hints = stage->module->hints;
        }
        if (!hints)
        {
            hints = vsg::ShaderCompileSettings::create();
            for (const auto& stage : graphicsPipeline->stages)
            {
                if (stage->module)
                {
                    for (const auto& define : shaderSet->optionalDefines)
                    {
                        if (stage->module->source.find("#define " + define) != std::string::npos)
                            hints->defines.insert(define);
                    }
                }
            }
        }

        graphicsPipeline->stages = shaderSet->getShaderStages(hints);
    }
};

class ModifyShaderVisitor : public vsg::Inherit<vsg::Visitor, ModifyShaderVisitor>
{
public:
    ModifyShaderVisitor(std::vector<std::pair<std::regex, std::string>> in_replacements) :
        replacements(std::move(in_replacements))
    {
    }

    std::vector<std::pair<std::regex, std::string>> replacements;
    std::set<vsg::GraphicsPipeline*> visited = {};

    void apply(vsg::Node& node) override
    {
        node.traverse(*this);
    }

    void apply(vsg::BindGraphicsPipeline& bgp) override
    {
        auto graphicsPipeline = bgp.pipeline;
        if (visited.count(graphicsPipeline.get()) > 0)
        {
            return;
        }

        visited.insert(graphicsPipeline);

        for (const auto& stage : graphicsPipeline->stages)
        {
            if (stage->module)
            {
                std::string original = stage->module->source;
                for (const auto& [regex, replacement] : replacements)
                    stage->module->source = std::regex_replace(stage->module->source, regex, replacement);
                if (original != stage->module->source)
                {
                    stage->module->code.clear();
                }
            }
        }
    }
};

class ImageFormatSwitcherVistor : public vsg::Inherit<vsg::Visitor, ImageFormatSwitcherVistor>
{
public:
    ImageFormatSwitcherVistor(const std::set<std::uint32_t>& in_slots) :
        slots(in_slots)
    {}

    std::set<std::uint32_t> slots;
    std::set<vsg::DescriptorSet*> visited = {};

    void apply(vsg::Node& node) override
    {
        node.traverse(*this);
    }

    void apply(vsg::DescriptorSet& descriptorSet) override
    {
        vsg::DescriptorSet* address = &descriptorSet;
        if (visited.count(address) > 0)
        {
            return;
        }
        visited.insert(address);

        descriptorSet.traverse(*this);
    }

    void apply(vsg::DescriptorImage& descriptor) override
    {
        if (slots.count(descriptor.dstBinding) != 0)
        {
            for (auto& imageInfo : descriptor.imageInfoList)
            {
                VkFormat& format = imageInfo->imageView->image->data->properties.format;
                format = vsg::uNorm_to_sRGB(format);
            }
        }
    }

    void apply(vsg::ubvec4Array2D& array) override
    {
        VkFormat& format = array.properties.format;
        format = vsg::uNorm_to_sRGB(format);
    }
};

class VertexColorspaceConversionVisitor : public vsg::Inherit<vsg::Visitor, VertexColorspaceConversionVisitor>
{
public:
    VertexColorspaceConversionVisitor(const std::set<std::uint32_t>& in_slots) :
        slots(in_slots)
    {
        attributeStack.push_back({});
        strideStack.push_back({});
    }

    vsg::sRGB_to_linearColorSpaceConvertor convertor = {};

    std::set<std::uint32_t> slots;
    std::set<vsg::GraphicsPipeline*> visited = {};
    std::vector<std::optional<VkVertexInputAttributeDescription>> attributeStack;
    std::vector<std::uint32_t> strideStack;

    void apply(vsg::Node& node) override
    {
        node.traverse(*this);
    }

    void apply(vsg::StateGroup& stateGroup) override
    {
        attributeStack.push_back(attributeStack.back());
        strideStack.push_back(strideStack.back());

        stateGroup.traverse(*this);

        attributeStack.pop_back();
        strideStack.pop_back();
    }

    void apply(vsg::BindGraphicsPipeline& bgp) override
    {
        auto graphicsPipeline = bgp.pipeline;

        for (const auto& state : graphicsPipeline->pipelineStates)
        {
            if (state && state->is_compatible(typeid(vsg::VertexInputState)))
            {
                const auto& vas = static_cast<const vsg::VertexInputState&>(*state);
                attributeStack.back() = std::nullopt;
                for (const auto& attribute : vas.vertexAttributeDescriptions)
                {
                    if (slots.count(attribute.location))
                    {
                        attributeStack.back() = attribute;
                    }
                }
                if (attributeStack.back())
                {
                    for (const auto& binding : vas.vertexBindingDescriptions)
                    {
                        if (binding.binding == attributeStack.back()->binding)
                        {
                            strideStack.back() = binding.stride;
                        }
                    }
                }
            }
        }
    }

    void apply(vsg::VertexIndexDraw& vertexIndexDraw) override
    {
        if (!attributeStack.back())
            return;

        std::uint32_t index = attributeStack.back()->binding - vertexIndexDraw.firstBinding;
        if (vertexIndexDraw.arrays.size() > index)
        {
            auto bufferInfo = vertexIndexDraw.arrays[index];
            if (bufferInfo->data->stride() == strideStack.back() && attributeStack.back()->offset == 0)
            {
                if (bufferInfo->data->cast<vsg::vec3Value>())
                    convertor.convertVertexColor(bufferInfo->data->cast<vsg::vec3Value>()->value());
                else if (bufferInfo->data->cast<vsg::vec4Value>())
                    convertor.convertVertexColor(bufferInfo->data->cast<vsg::vec4Value>()->value());
                else if (bufferInfo->data->cast<vsg::vec3Array>())
                    convertor.convertVertexColors(*bufferInfo->data->cast<vsg::vec3Array>());
                else if (bufferInfo->data->cast<vsg::vec4Array>())
                    convertor.convertVertexColors(*bufferInfo->data->cast<vsg::vec4Array>());
            }
            // other scenegraphs, e.g. ones using interleaved attributes, may require more complicated handling
            // none of the VSG example models do, though
        }

    }
};

class DescriptorBufferColorSpaceConversionVisitor : public vsg::Inherit<vsg::Visitor, DescriptorBufferColorSpaceConversionVisitor>
{
public:
    DescriptorBufferColorSpaceConversionVisitor(const std::set<std::uint32_t>& in_slots) :
        slots(in_slots)
    {
    }

    vsg::sRGB_to_linearColorSpaceConvertor convertor = {};

    std::set<std::uint32_t> slots;
    std::set<vsg::DescriptorSet*> visited = {};

    void apply(vsg::Node& node) override
    {
        node.traverse(*this);
    }

    void apply(vsg::DescriptorSet& descriptorSet) override
    {
        vsg::DescriptorSet* address = &descriptorSet;
        if (visited.count(address) > 0)
        {
            return;
        }
        visited.insert(address);

        descriptorSet.traverse(*this);
    }

    void apply(vsg::DescriptorBuffer& descriptor) override
    {
        if (slots.count(descriptor.dstBinding) != 0)
        {
            for (auto& bufferInfo : descriptor.bufferInfoList)
            {
                auto& data = bufferInfo->data;
                if (data)
                {
                    if (bufferInfo->data->cast<vsg::vec3Value>())
                        convertor.convertMaterialColor(bufferInfo->data->cast<vsg::vec3Value>()->value());
                    else if (bufferInfo->data->cast<vsg::vec4Value>())
                        convertor.convertMaterialColor(bufferInfo->data->cast<vsg::vec4Value>()->value());
                    else if (bufferInfo->data->cast<vsg::vec3Array>())
                        convertor.convertMaterialColors(*bufferInfo->data->cast<vsg::vec3Array>());
                    else if (bufferInfo->data->cast<vsg::vec4Array>())
                        convertor.convertMaterialColors(*bufferInfo->data->cast<vsg::vec4Array>());
                }
            }
        }
    }
};

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);;
    auto inputDirectory = arguments.value<vsg::Path>("", "-i");
    auto outputDirectory = arguments.value<vsg::Path>("", "-o");
    auto osg2vsgDirectory = arguments.value<vsg::Path>("", "-t");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    if (outputDirectory == "")
    {
        vsg::info("Modifying files in-place...");
        outputDirectory = inputDirectory;
    }

    if (osg2vsgDirectory == "")
    {
        vsg::error("No osg2vsg directory specified.");
        return -1;
    }

    vsg::makeDirectory(outputDirectory / "models");
    vsg::makeDirectory(outputDirectory / "textures");

    vsg::ref_ptr<vsg::Object> object;
    vsg::VSG io;

    auto osg2vsgVertexShader = vsg::read_cast<vsg::ShaderStage>(osg2vsgDirectory / "data/shaders/fbxshader.vert");
    auto osg2vsgFragmentShader = vsg::read_cast<vsg::ShaderStage>(osg2vsgDirectory / "data/shaders/fbxshader.frag");

    auto osg2vsgShaderSet = vsg::ShaderSet::create(vsg::ShaderStages{osg2vsgVertexShader, osg2vsgFragmentShader});

    osg2vsgShaderSet->addAttributeBinding("osg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    osg2vsgShaderSet->addAttributeBinding("osg_Normal", "VSG_NORMAL", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    osg2vsgShaderSet->addAttributeBinding("osg_Tangent", "VSG_TANGENT", 2, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec3Array::create(1));
    osg2vsgShaderSet->addAttributeBinding("osg_Color", "VSG_COLOR", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec3Array::create(1));
    osg2vsgShaderSet->addAttributeBinding("osg_MultiTexCoord0", "VSG_TEXCOORD0", 4, VK_FORMAT_R32G32_SFLOAT, vsg::vec3Array::create(1));
    osg2vsgShaderSet->addAttributeBinding("translate", "VSG_TRANSLATE", 7, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

    osg2vsgShaderSet->optionalDefines = {"VSG_NORMAL", "VSG_TANGENT", "VSG_COLOR", "VSG_TEXCOORD0", "VSG_LIGHTING", "VSG_NORMAL_MAP", "VSG_BILLBOARD", "VSG_TRANSLATE", "VSG_MATERIAL", "VSG_DIFFUSE_MAP", "VSG_OPACITY_MAP", "VSG_AMBIENT_MAP", "VSG_SPECULAR_MAP"};

    std::set<std::uint32_t> colorTextureSlots = {0};

    std::set<std::uint32_t> colorVertexAttributeSlots = {3};

    vsg::info("Updating lz.vsgt");
    if (vsg::fileExists(inputDirectory / "models/lz.vsgt"))
    {
        object = io.read(inputDirectory / "models/lz.vsgt");
        if (object)
        {
            UpdateShaderVisitor shaderVisitor(osg2vsgShaderSet);
            object->accept(shaderVisitor);
            vsg::ShaderCompiler shaderCompiler;
            object->accept(shaderCompiler);
            ImageFormatSwitcherVistor imageFormatVisitor(colorTextureSlots);
            object->accept(imageFormatVisitor);
            VertexColorspaceConversionVisitor vertexColorVisitor(colorVertexAttributeSlots);
            object->accept(vertexColorVisitor);
            vsg::write(object, outputDirectory / "models/lz.vsgt");
        }
        else
            vsg::warn("File not read : ", inputDirectory / "models/lz.vsgt");
    }
    else
        vsg::warn("No such file : ", inputDirectory / "models/lz.vsgt");

    vsg::info("Updating skybox.vsgt");
    if (vsg::fileExists(inputDirectory / "models/skybox.vsgt"))
    {
        object = io.read(inputDirectory / "models/skybox.vsgt");
        if (object)
        {
            vsg::ShaderCompiler shaderCompiler;
            object->accept(shaderCompiler);
            ImageFormatSwitcherVistor imageFormatVisitor(colorTextureSlots);
            object->accept(imageFormatVisitor);
            vsg::write(object, outputDirectory / "models/skybox.vsgt");
        }
        else
            vsg::warn("File not read : ", inputDirectory / "models/skybox.vsgt");
    }
    else
        vsg::warn("No such file : ", inputDirectory / "models/skybox.vsgt");

    vsg::info("Updating teapot.vsgt");
    if (vsg::fileExists(inputDirectory / "models/teapot.vsgt"))
    {
        object = io.read(inputDirectory / "models/teapot.vsgt");
        if (object)
        {
            ModifyShaderVisitor shaderVisitor({{std::regex("vec3 ambientColor = vec3\\(0\\.1,0\\.1,0\\.1\\);"), "vec3 ambientColor = vec3(0.01,0.01,0.01);"},
                                               {std::regex("vec3 specularColor = vec3\\(0\\.3,0\\.3,0\\.3\\);"), "vec3 specularColor = vec3(0.073,0.073,0.073);"},
                                               {std::regex("vec4 color = vec4\\(0\\.01, 0\\.01, 0\\.01, 1\\.0\\);"), "vec4 color = vec4(0.001, 0.001, 0.001, 1.0);"}
                });
            object->accept(shaderVisitor);
            vsg::ShaderCompiler shaderCompiler;
            object->accept(shaderCompiler);
            ImageFormatSwitcherVistor imageFormatVisitor(colorTextureSlots);
            object->accept(imageFormatVisitor);
            DescriptorBufferColorSpaceConversionVisitor materialColorVisitor({10});
            object->accept(materialColorVisitor);
            vsg::write(object, outputDirectory / "models/teapot.vsgt");
        }
        else
            vsg::warn("File not read : ", inputDirectory / "models/teapot.vsgt");
    }
    else
        vsg::warn("No such file : ", inputDirectory / "models/teapot.vsgt");

    vsg::info("Updating raytracing_scene.vsgt");
    if (vsg::fileExists(inputDirectory / "models/raytracing_scene.vsgt"))
    {
        object = io.read(inputDirectory / "models/raytracing_scene.vsgt");
        if (object)
        {
            ModifyShaderVisitor shaderVisitor({{std::regex("vec3 ambientColor = vec3\\(0\\.1,0\\.1,0\\.1\\);"), "vec3 ambientColor = vec3(0.01,0.01,0.01);"},
                                               {std::regex("vec3 specularColor = vec3\\(0\\.3,0\\.3,0\\.3\\);"), "vec3 specularColor = vec3(0.073,0.073,0.073);"},
                                               {std::regex("vec4 color = vec4\\(0\\.01, 0\\.01, 0\\.01, 1\\.0\\);"), "vec4 color = vec4(0.001, 0.001, 0.001, 1.0);"}});
            object->accept(shaderVisitor);
            vsg::ShaderCompiler shaderCompiler;
            object->accept(shaderCompiler);
            ImageFormatSwitcherVistor imageFormatVisitor(colorTextureSlots);
            object->accept(imageFormatVisitor);
            vsg::write(object, outputDirectory / "models/raytracing_scene.vsgt");
        }
        else
            vsg::warn("File not read : ", inputDirectory / "models/raytracing_scene.vsgt");
    }
    else
        vsg::warn("No such file : ", inputDirectory / "models/raytracing_scene.vsgt");

    vsg::info("Updating lz.vsgb");
    if (vsg::fileExists(inputDirectory / "textures/lz.vsgb"))
    {
        object = io.read(inputDirectory / "textures/lz.vsgb");
        if (object)
        {
            ImageFormatSwitcherVistor imageFormatVisitor(colorTextureSlots);
            object->accept(imageFormatVisitor);
            vsg::write(object, outputDirectory / "textures/lz.vsgb");
        }
        else
            vsg::warn("File not read : ", inputDirectory / "textures/lz.vsgb");
    }
    else
        vsg::warn("No such file : ", inputDirectory / "textures/lz.vsgb");

    return 0;
}
