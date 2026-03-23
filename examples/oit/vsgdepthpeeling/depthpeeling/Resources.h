#pragma once

#include "ShaderSet.h"

#include <vsg/app/RenderGraph.h>
#include <vsg/app/View.h>

#include <vsg/utils/ShaderSet.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>

#include <vsg/core/type_name.h>

namespace vsg::oit::depthpeeling {

    class DescriptorSetBinding;

    class Resources final : public Inherit<Object, Resources>
    {
    public:
        struct CreateRenderPassInfo
        {
            ref_ptr<Window> window;
            uint32_t numPeelLayers = 8;
        };

        struct CreateAttachmentInfo
        {
            ref_ptr<Window> window;
            VkExtent2D extent{ vsg::RenderGraph::invalid_dimension, vsg::RenderGraph::invalid_dimension };
        };
        using CreateFramebufferInfo = CreateAttachmentInfo;

        struct Attachments
        {
            struct Attachment
            {
                ref_ptr<Image> image;
                ref_ptr<ImageView> view;

                bool valid() const { return image.valid() && view.valid(); }
            };

            // resolved opaque depth attachment from opaque pass
            Attachment opaqueDepth;

            // accumulated transparency attachment over all peel layers
            Attachment accum;
            // current peel layer attachment
            Attachment color;
            // two depth attachments used in ping-pong fashion
            std::array<Attachment, 2u> depth;

            bool valid() const
            {
                return opaqueDepth.valid() && accum.valid() 
                    && color.valid() && depth[0].valid() && depth[1].valid();
            }
        };

        void ensureRenderPassUpToDate(const CreateRenderPassInfo& createInfo);
        ref_ptr<RenderPass> getOrCreateRenderPass(const CreateRenderPassInfo& createInfo);
        ref_ptr<RenderPass> renderPass() const { return _renderPass; }

        void ensureAttachmentsUpToDate(const CreateAttachmentInfo& createInfo);
        const Attachments& getOrCreateAttachments(const CreateAttachmentInfo& createInfo);
        const Attachments& attachments() const { return _attachments; }

        ref_ptr<Framebuffer> getOrCreateFramebuffer(const CreateFramebufferInfo& createInfo);
        ref_ptr<Framebuffer> framebuffer() const;

        static const vsg::RenderGraph::ClearValues& clearValues();

        void registerDescriptorSetBinding(DescriptorSetBinding& dsb);

    protected:
        enum
        {
            ColorOpaque = 0, // resolved (!) color from opaque pass
            ColorAccum = 1,
            ColorPeel = 2,
            DepthOpaque = 3, // resolved (!) depth from opaque pass
            DepthPeel0 = 4,
            DepthPeel1 = 5,
        };

        struct State
        {
            CreateRenderPassInfo renderPass;
            CreateAttachmentInfo attachment;
            CreateFramebufferInfo framebuffer;
        };

        constexpr static auto ColorAccumImageType = VK_FORMAT_R32G32B32A32_SFLOAT;
        constexpr static auto ColorPeelImageType = ColorAccumImageType;
        constexpr static auto DepthPeelImageType = VK_FORMAT_D32_SFLOAT;

        ref_ptr<RenderPass> createTransparencyPass(const CreateRenderPassInfo& createInfo);

        void createImages(const CreateAttachmentInfo& createInfo);
        void createFramebuffer(const CreateFramebufferInfo& createInfo);

        State _state;

        Attachments _attachments;
        ref_ptr<RenderPass> _renderPass;
        std::vector<ref_ptr<Framebuffer>> _framebuffers;
        std::vector<ref_ptr<DescriptorSetBinding>> _descriptorSetBindings;
    };

}
EVSG_type_name(vsg::oit::depthpeeling::Resources)