#include "Resources.h"

#include "Bindings.h"

using namespace vsg::oit::depthpeeling;
using namespace vsg;

ref_ptr<RenderPass> Resources::createTransparencyPass(const CreateRenderPassInfo& createInfo)
{
    if (!createInfo.window.valid())
    {
        return {};
    }

    // ========================================================================
    // Attachment descriptions
    // ========================================================================

    auto opaqueColorDescription = defaultColorAttachment(createInfo.window->surfaceFormat().format);
    opaqueColorDescription.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    opaqueColorDescription.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

    auto opaqueDepthDescription = defaultDepthAttachment(createInfo.window->depthFormat());
    opaqueDepthDescription.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    opaqueDepthDescription.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    opaqueDepthDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    auto accumDescription = defaultColorAttachment(ColorAccumImageType);
    accumDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    accumDescription.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    accumDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

    auto peelColorDescription = defaultColorAttachment(ColorPeelImageType);
    peelColorDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    peelColorDescription.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    peelColorDescription.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    auto depthDescription = defaultDepthAttachment(DepthPeelImageType);
    depthDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthDescription.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    // ========================================================================
    // Attachments
    // ========================================================================

    RenderPass::Attachments attachments{ 
        opaqueColorDescription, accumDescription, peelColorDescription, 
        opaqueDepthDescription, depthDescription, depthDescription };

    // Attachment-References

    std::vector<AttachmentReference> attachmentRefs{
        {ColorOpaque, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT},
        {ColorAccum, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT},
        {ColorPeel, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT},
        {DepthOpaque, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT},
        {DepthPeel0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT},
        {DepthPeel1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT}};

    RenderPass::Subpasses subpasses;
    RenderPass::Dependencies dependencies;

    // ========================================================================
    // Subpasses
    // ========================================================================

    // Subpass 0 => Peels - 1 => Gather and compute transparencies in correct order

    for (auto pass = 0u; pass < createInfo.numPeelLayers; ++pass)
    {
        SubpassDescription renderSubpass;
        renderSubpass.colorAttachments.emplace_back(attachmentRefs.at(ColorPeel));
        renderSubpass.depthStencilAttachments.emplace_back(
            attachmentRefs.at(DepthPeel0 + (pass % 2)));

        renderSubpass.inputAttachments.emplace_back(attachmentRefs.at(DepthOpaque));
        renderSubpass.inputAttachments.back().layout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        renderSubpass.inputAttachments.emplace_back(
            attachmentRefs.at(DepthPeel0 + ((pass + 1) % 2)));
        renderSubpass.inputAttachments.back().layout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        renderSubpass.preserveAttachments.push_back(ColorOpaque);
        renderSubpass.preserveAttachments.push_back(ColorAccum);

        subpasses.emplace_back(renderSubpass);

        SubpassDescription combineSubpass;
        combineSubpass.colorAttachments.emplace_back(attachmentRefs.at(ColorAccum));

        combineSubpass.inputAttachments.emplace_back(attachmentRefs.at(ColorPeel));
        combineSubpass.inputAttachments.back().layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        combineSubpass.preserveAttachments.push_back(ColorOpaque);
        combineSubpass.preserveAttachments.push_back(DepthOpaque);
        combineSubpass.preserveAttachments.push_back(DepthPeel0);
        combineSubpass.preserveAttachments.push_back(DepthPeel1);

        subpasses.emplace_back(combineSubpass);
    }

    // Combine Subpass => Combine opaque and transparent fragments

    SubpassDescription combineSubpass;
    combineSubpass.colorAttachments.emplace_back(attachmentRefs.at(ColorOpaque));

    combineSubpass.inputAttachments.emplace_back(attachmentRefs.at(ColorAccum));
    combineSubpass.inputAttachments.back().layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    subpasses.emplace_back(combineSubpass);

    // ========================================================================
    // Dependencies between subpasses
    // ========================================================================

    constexpr auto ColorDepthAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    constexpr auto ColorAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

    // EXTERNAL => First peeling subpass

    SubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    dependency.srcAccessMask = ColorDepthAccessMask;
    dependency.dstAccessMask = ColorDepthAccessMask;
    dependency.dependencyFlags = 0;

    // Subpass 0 => (Peels * 2) - 1 => Gather transparencies and combine 1 - Peels * 2

    for (auto pass = 0u; pass < createInfo.numPeelLayers; ++pass)
    {
        dependency.srcAccessMask = ColorDepthAccessMask;
        dependencies.emplace_back(dependency);
        dependency.srcSubpass = dependency.dstSubpass++;

        dependency.srcAccessMask = ColorAccessMask;
        dependencies.emplace_back(dependency);
        dependency.srcSubpass = dependency.dstSubpass++;
    }

    // Last gather transparencies subpass => Combine subpass

    dependency.srcAccessMask = ColorAccessMask;
    dependencies.emplace_back(dependency);

    // Combine subpass => EXTERNAL

    dependency.srcSubpass = dependency.dstSubpass;
    dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies.emplace_back(dependency);

    // ========================================================================
    // Create renderpass
    // ========================================================================

    return RenderPass::create(
        createInfo.window->getOrCreateDevice(), attachments, subpasses, dependencies);
}

void Resources::ensureRenderPassUpToDate(const CreateRenderPassInfo& createInfo)
{
    if (_renderPass.valid()
        && _state.renderPass.window == createInfo.window
        && _state.renderPass.numPeelLayers == createInfo.numPeelLayers)
    {
        return;
    }

    if (!createInfo.window.valid() || createInfo.numPeelLayers == 0)
    {
        _renderPass.reset();
    }
    else
    {
        _renderPass = createTransparencyPass(createInfo);
    }

    _state.renderPass = createInfo;
}

ref_ptr<RenderPass> Resources::getOrCreateRenderPass(const CreateRenderPassInfo& createInfo)
{
    ensureRenderPassUpToDate(createInfo);
    return _renderPass;
}

void Resources::ensureAttachmentsUpToDate(const CreateAttachmentInfo& createInfo)
{
    if (_attachments.valid()
        && _state.attachment.window == createInfo.window
        && _state.attachment.extent.width == createInfo.extent.width
        && _state.attachment.extent.height == createInfo.extent.height)
    {
        return;
    }

    createImages(createInfo);

    // update all registered descriptor set bindings as attachments have changed and 
    // they might be used as input attachments in the shader, so need to be re-bound
    for (auto& dsb : _descriptorSetBindings)
    {
        dsb->dirty();
    }
}

const Resources::Attachments& Resources::getOrCreateAttachments(const CreateAttachmentInfo& createInfo)
{
    ensureAttachmentsUpToDate(createInfo);
    return _attachments;
}

ref_ptr<Framebuffer> Resources::getOrCreateFramebuffer(const CreateFramebufferInfo& createInfo)
{
    auto valid = createInfo.window.valid() 
        && _state.framebuffer.window == createInfo.window 
        && _state.framebuffer.extent.width == createInfo.extent.width 
        && _state.framebuffer.extent.height == createInfo.extent.height;
    if (valid)
    {
        for (auto& framebuffer : _framebuffers)
        {
            if (!framebuffer.valid() || framebuffer->extent2D() != createInfo.extent)
            {
                valid = false;
                break;
            }
        }
    }

    if (valid)
    {
        return framebuffer();
    }

    createFramebuffer(createInfo);
    return framebuffer();
}

ref_ptr<Framebuffer> Resources::framebuffer() const
{
    const auto idx = _state.framebuffer.window.valid() ? _state.framebuffer.window->imageIndex() : 0;
    if (!_state.framebuffer.window.valid() || idx >= _framebuffers.size())
    {
        return {};
    }

    return _framebuffers.at(idx);
}

const vsg::RenderGraph::ClearValues& Resources::clearValues()
{
    static const vsg::RenderGraph::ClearValues clearValues{
        {{{ 0.0f, 0.0f, 0.0f, 0.0f }}}, // ColorOpaque
        {{{ 0.0f, 0.0f, 0.0f, 0.0f }}}, // ColorAccum
        {{{ 0.0f, 0.0f, 0.0f, 0.0f }}}, // ColorPeel
        {{{ 0.0f, 0 }}},                // DepthOpaque
        {{{ 0.0f, 0 }}},                // DepthPeel0
        {{{ 0.0f, 0 }}}                 // DepthPeel1
    };

    return clearValues;
}

void Resources::registerDescriptorSetBinding(DescriptorSetBinding& dsb)
{
    _descriptorSetBindings.emplace_back(&dsb);
}

void Resources::createImages(const CreateAttachmentInfo& createInfo)
{
    if (!createInfo.window.valid())
    {
        _attachments = {};
        _state.attachment = createInfo;

        return;
    }

    auto device = createInfo.window->getOrCreateDevice();

    // determine depth attachment from opaque pass of window
    _attachments.opaqueDepth = {
        createInfo.window->getOrCreateDepthImage(),
        createInfo.window->getOrCreateDepthImageView()
    };

    // create accum color image
    _attachments.accum.image = Image::create();
    _attachments.accum.image->imageType = VK_IMAGE_TYPE_2D;
    _attachments.accum.image->extent = {createInfo.extent.width, createInfo.extent.height, 1};
    _attachments.accum.image->mipLevels = 1;
    _attachments.accum.image->arrayLayers = 1;
    _attachments.accum.image->format = ColorAccumImageType;
    _attachments.accum.image->tiling = VK_IMAGE_TILING_OPTIMAL;
    _attachments.accum.image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    _attachments.accum.image->samples = VK_SAMPLE_COUNT_1_BIT;
    _attachments.accum.image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    _attachments.accum.image->usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    _attachments.accum.image->compile(device);
    _attachments.accum.image->allocateAndBindMemory(device);
    _attachments.accum.view = ImageView::create(_attachments.accum.image, VK_IMAGE_ASPECT_COLOR_BIT);
    _attachments.accum.view->compile(device);
    // create peel target color image
    _attachments.color.image = Image::create();
    _attachments.color.image->imageType = VK_IMAGE_TYPE_2D;
    _attachments.color.image->extent = _attachments.accum.image->extent;
    _attachments.color.image->mipLevels = 1;
    _attachments.color.image->arrayLayers = 1;
    _attachments.color.image->format = ColorPeelImageType;
    _attachments.color.image->tiling = VK_IMAGE_TILING_OPTIMAL;
    _attachments.color.image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    _attachments.color.image->samples = VK_SAMPLE_COUNT_1_BIT;
    _attachments.color.image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    _attachments.color.image->usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    _attachments.color.image->compile(device);
    _attachments.color.image->allocateAndBindMemory(device);
    _attachments.color.view = ImageView::create(_attachments.color.image, VK_IMAGE_ASPECT_COLOR_BIT);
    _attachments.color.view->compile(device);
    // Gather passes images
    for (auto& depth : _attachments.depth)
    {
        // create depth image
        depth.image = Image::create();
        depth.image->imageType = VK_IMAGE_TYPE_2D;
        depth.image->extent = _attachments.accum.image->extent;
        depth.image->mipLevels = 1;
        depth.image->arrayLayers = 1;
        depth.image->format = DepthPeelImageType;
        depth.image->tiling = VK_IMAGE_TILING_OPTIMAL;
        depth.image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.image->samples = VK_SAMPLE_COUNT_1_BIT;
        depth.image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        depth.image->usage =
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

        depth.image->compile(device);
        depth.image->allocateAndBindMemory(device);

        depth.view = ImageView::create(depth.image, VK_IMAGE_ASPECT_DEPTH_BIT);
        depth.view->compile(device);
    }

    _state.attachment = createInfo;
}

void Resources::createFramebuffer(const CreateFramebufferInfo& createInfo)
{
    if (!createInfo.window.valid())
    {
        _framebuffers.clear();
        _state.framebuffer = createInfo;

        return;
    }

    auto& attachments = getOrCreateAttachments({ createInfo.window, createInfo.extent });

    _framebuffers.clear();
    for (auto index = 0u; index < createInfo.window->numFrames(); ++index)
    {
        auto colorImageView = createInfo.window->imageView(index);

        ImageViews imageViews{
            colorImageView, 
            attachments.accum.view, 
            attachments.color.view,
            attachments.opaqueDepth.view, 
            attachments.depth[0].view, 
            attachments.depth[1].view };

        _framebuffers.push_back(Framebuffer::create(
            getOrCreateRenderPass(_state.renderPass), imageViews, 
            createInfo.extent.width, createInfo.extent.height, 1));
    }

    _state.framebuffer = createInfo;
}
