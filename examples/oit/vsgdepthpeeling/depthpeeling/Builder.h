#pragma once

#include "RenderGraph.h"
#include "ShaderSet.h"

#include <vsg/app/Window.h>
#include <vsg/io/Options.h>

namespace vsg::oit::depthpeeling {

    class Resources;

    class Builder final : public Inherit<vsg::Object, Builder>
    {
    public:
        struct Settings : Inherit<vsg::Object, Settings>
        {
            Settings() = default;
            explicit Settings(ref_ptr<Window> w, ref_ptr<Options> o, 
                ShadingModel m = ShadingModel::Phong, bool h = true, uint32_t n = 8)
                : window(w)
                , options(o)
                , shadingModel(m)
                , headlight(h)
                , numPeelLayers(n)
            {
            }

            ref_ptr<Window> window;
            ref_ptr<Options> options;

            ShadingModel shadingModel = ShadingModel::Phong;
            bool headlight = true;

            int32_t numPeelLayers = 8;
        };

        enum class Pass
        {
            Opaque,
            Transparency
        };

        Builder(ref_ptr<Settings> settings);

        struct RenderGraphs
        {
            ref_ptr<vsg::RenderGraph> opaque;
            ref_ptr<vsg::RenderGraph> transparency;
        };

        RenderGraphs createRenderGraphs();

        void setCamera(ref_ptr<Camera> camera);
        ref_ptr<Camera> getCamera() const;

        void setScene(Pass pass, ref_ptr<Node> scene);
        ref_ptr<Node> getScene(Pass pass) const;

        using MaterialConfigurator = std::function<void(DescriptorConfigurator&, ref_ptr<Options>&)>;
        ref_ptr<BindDescriptorSet> getOrCreateMaterialBinding(MaterialConfigurator configurator, bool share = true) const;

    private:
        ref_ptr<View> createView(const ref_ptr<Node>& headlight) const;

        ref_ptr<ClearAttachments> createClearAttachments() const;
        ref_ptr<Node> createPeelScene(Resources& resources, int32_t peelIndex) const;
        ref_ptr<Node> createPeelCombineScene(Resources& resources, int32_t peelIndex) const;
        ref_ptr<Node> createFinalCombineScene(Resources& resources) const;

        ref_ptr<vsg::RenderGraph> createOpaqueRenderGraph(const ref_ptr<Node>& headlight) const;
        ref_ptr<vsg::RenderGraph> createTransparencyRenderGraph(const ref_ptr<Node>& headlight) const;

        ref_ptr<Settings> _settings;

        ref_ptr<Camera> _camera;
        ref_ptr<Node> _opaqueScene;
        ref_ptr<Node> _transparencyScene;
    };

}

EVSG_type_name(vsg::oit::depthpeeling::Builder)
EVSG_type_name(vsg::oit::depthpeeling::Builder::Settings)
