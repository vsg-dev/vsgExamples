#include "Bindings.h"
#include "Resources.h"

using namespace vsg::oit::depthpeeling;
using namespace vsg;

DescriptorSetBinding::DescriptorSetBinding(Resources& r, ref_ptr<ShaderSet> s, ref_ptr<PipelineLayout> l)
    : Inherit(3)
    , _resources(&r)
    , _shaderSet(s)
    , _layout(l)
{
}

void DescriptorSetBinding::compile(Context& context)
{
    _context = &context;

    if (_layout.valid())
    {
        _layout->compile(context);
    }

    ensureBindingUpToDate();
}

void DescriptorSetBinding::record(CommandBuffer& commandBuffer) const
{
    if (ensureBindingUpToDate())
    {
        _binding->record(commandBuffer);
    }
}

BindPeelDescriptorSet::BindPeelDescriptorSet(
    Resources& r, ref_ptr<ShaderSet> s, ref_ptr<PipelineLayout> l, int32_t i)
    : Inherit(r, s, l)
    , _peelIndex(i)
{
}

ref_ptr<BindDescriptorSet> BindPeelDescriptorSet::createBinding() const
{
    auto resources = _resources.valid() ? _resources.ref_ptr() : nullptr;
    if (!resources.valid())
    {
        return {};
    }

    const auto peelIdx = (_peelIndex + 1) % 2;

    auto opaqueDepthImageInfo = ImageInfo::create(nullptr, 
        resources->attachments().opaqueDepth.view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    auto prevPassDepthImageInfo = ImageInfo::create(nullptr, 
        resources->attachments().depth[peelIdx].view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

    auto& baseBinding = _shaderSet->getDescriptorBinding("opaqueDepth");

    DescriptorConfigurator descriptorConfigurator{ _shaderSet };
    descriptorConfigurator.assignTexture("opaqueDepth", { opaqueDepthImageInfo }, 0);
    descriptorConfigurator.assignTexture("prevPassDepth", { prevPassDepthImageInfo }, 0);

    if (_peelIndex == 0)
    {
        descriptorConfigurator.defines.insert("DEPTHPEELING_FIRSTPASS");
    }

    return BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, _layout,
        baseBinding.set, descriptorConfigurator.descriptorSets.at(baseBinding.set));
}

CombineDescriptorSetBinding::CombineDescriptorSetBinding(
    Resources& r, ref_ptr<ShaderSet> s, ref_ptr<PipelineLayout> l)
    : Inherit(r, s, l)
{
}

ref_ptr<BindDescriptorSet> CombineDescriptorSetBinding::createBinding() const
{
    auto outputImageInfo = ImageInfo::create(nullptr, 
        getInputImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    auto& baseBinding = _shaderSet->getDescriptorBinding("peelOutput");

    DescriptorConfigurator descriptorConfigurator{ _shaderSet };
    descriptorConfigurator.assignTexture("peelOutput", { outputImageInfo }, 0);

    if (auto define = inputImageViewDefine(); !define.empty())
    {
        descriptorConfigurator.defines.insert(define);
    }

    return BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, _layout,
        baseBinding.set, descriptorConfigurator.descriptorSets.at(baseBinding.set));
}

BindPeelCombineDescriptorSet::BindPeelCombineDescriptorSet(
    Resources& r, ref_ptr<ShaderSet> s, ref_ptr<PipelineLayout> l)
    : Inherit(r, s, l)
{
}

ref_ptr<ImageView> BindPeelCombineDescriptorSet::getInputImageView() const
{
    return _resources.valid() ? _resources.ref_ptr()->attachments().color.view : nullptr;
}

std::string BindPeelCombineDescriptorSet::inputImageViewDefine() const
{
    return "DEPTHPEELING_PASS";
}

BindFinalCombineDescriptorSet::BindFinalCombineDescriptorSet(
    Resources& r, ref_ptr<ShaderSet> s, ref_ptr<PipelineLayout> l)
    : Inherit(r, s, l)
{
}

ref_ptr<ImageView> BindFinalCombineDescriptorSet::getInputImageView() const
{
    return _resources.valid() ? _resources.ref_ptr()->attachments().accum.view : nullptr;
}

std::string BindFinalCombineDescriptorSet::inputImageViewDefine() const
{
    return "";
}
