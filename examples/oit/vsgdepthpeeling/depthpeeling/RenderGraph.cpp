#include "RenderGraph.h"

#include "Bindings.h"

#include <vsg/vk/Context.h>

using namespace vsg::oit::depthpeeling;

namespace vsg::oit::depthpeeling
{

    template<typename T>
    class ScopedOverride {
    public:
        ScopedOverride(T& a, T& b) 
            : _a(a)
            , _b(b) 
        {
            std::swap(_a, _b);
        }

        ~ScopedOverride() 
        {
            std::swap(_a, _b);
        }

    private:
        T& _a;
        T& _b;
    };

    template<typename T>
    ScopedOverride<T> make_scoped_override(T& x, T& y)
    {
        return ScopedOverride<T>(x, y);
    }
}

RenderGraph::RenderGraph(ref_ptr<Window> in_window, ref_ptr<View> in_view)
    : Inherit<vsg::RenderGraph, RenderGraph>(in_window, in_view)
    , _resources(Resources::create())
{
}

template<typename V>
void RenderGraph::t_accept(V& visitor)
{
    if (numPeelLayers == 0)
    {
        Inherit::accept(visitor);
        return;
    }

    // replace with compatible renderpass used in this RenderGraph, 
    // then restore the original renderpass after traversal
    auto select_renderpass = _resources->getOrCreateRenderPass({ window, numPeelLayers });
    auto renderPass_override = make_scoped_override(renderPass, select_renderpass);

    // update the attachments used in the RenderPass
    _resources->ensureAttachmentsUpToDate({ window, getExtent() });

    // traverse the RenderGraph as usual with the base visitor
    Inherit::accept(visitor);
}

void RenderGraph::accept(Visitor& visitor)
{
    t_accept(visitor);
}

void RenderGraph::accept(ConstVisitor& visitor) const
{
    const_cast<RenderGraph*>(this)->t_accept(visitor);
}

void RenderGraph::accept(vsg::RecordTraversal& recordTraversal) const
{
    if (numPeelLayers == 0)
    {
        return;
    }

    // VSG exposes accept(RecordTraversal) as const, but for traversal we need to
    // temporarily replace the framebuffer and restore it afterwards.
    auto& active_framebuffer = const_cast<ref_ptr<Framebuffer>&>(framebuffer);
    auto select_framebuffer = _resources->getOrCreateFramebuffer({ window, getExtent() });
    auto framebuffer_override = make_scoped_override(active_framebuffer, select_framebuffer);

    // VSG exposes accept(RecordTraversal) as const, but for traversal we need to
    // temporarily replace the clear values and restore it afterwards.
    auto& active_clearValues = const_cast<ClearValues&>(clearValues);
    auto select_clearValues = Resources::clearValues();
    auto clearValues_override = make_scoped_override(active_clearValues, select_clearValues);

    // traverse the RenderGraph as usual with the RecordTraversal visitor
    vsg::RenderGraph::accept(recordTraversal);
}
