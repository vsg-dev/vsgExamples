#pragma once

#include "Resources.h"

#include <vsg/app/RenderGraph.h>

#include <vsg/app/Camera.h>
#include <vsg/app/Window.h>
#include <vsg/app/WindowResizeHandler.h>
#include <vsg/state/ResourceHints.h>

namespace vsg::oit::depthpeeling {

    class RenderGraph final : public Inherit<vsg::RenderGraph, RenderGraph>
    {
        mutable ref_ptr<Resources> _resources;

        template<typename V>
        void t_accept(V& visitor);

    public:
        RenderGraph() = default;
        explicit RenderGraph(ref_ptr<Window> window, ref_ptr<vsg::View> view);

        uint32_t numPeelLayers = 0;

        const ref_ptr<Resources>& resources() const
        {
            return _resources;
        }

        void accept(Visitor& visitor) override;
        void accept(ConstVisitor& visitor) const override;
        void accept(RecordTraversal& recordTraversal) const override;
    };

}
EVSG_type_name(vsg::oit::depthpeeling::RenderGraph);
