#include "Builder.h"

#include "PipelineConfigurator.h"
#include "Bindings.h"

#include <vsg/app/View.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/state/material.h>

#include <vsg/commands/ClearAttachments.h>
#include <vsg/commands/NextSubPass.h>
#include <vsg/commands/Draw.h>

#include <vsg/lighting/Light.h>

using namespace vsg::oit::depthpeeling;
using namespace vsg;

Builder::Builder(ref_ptr<Settings> settings)
    : _settings(settings)
{
}

Builder::RenderGraphs Builder::createRenderGraphs()
{
    ref_ptr<Node> headlight = _settings->headlight ? createHeadlight() : nullptr;
    return {
        createOpaqueRenderGraph(headlight),
        createTransparencyRenderGraph(headlight)
    };
}

void Builder::setCamera(ref_ptr<Camera> camera)
{
    _camera = camera;
}

ref_ptr<Camera> Builder::getCamera() const
{
    return _camera;
}

void Builder::setScene(Pass pass, ref_ptr<Node> scene)
{
    switch (pass)
    {
    case Pass::Opaque:
        _opaqueScene = scene;
        break;
    case Pass::Transparency:
        _transparencyScene = scene;
        break;
    }
}

ref_ptr<Node> Builder::getScene(Pass pass) const
{
    switch (pass)
    {
    case Pass::Opaque:
        return _opaqueScene;
    case Pass::Transparency:
        return _transparencyScene;
    default:
        return nullptr;
    }
}

ref_ptr<BindDescriptorSet> Builder::getOrCreateMaterialBinding(MaterialConfigurator configurator, bool share) const
{
    if (!configurator || !_settings.valid())
    {
        return {};
    }

    auto graphicsPipelineConfigurator = OpaquePipelineConfigurator::getOrCreate(_settings->shadingModel, _settings->options);
    DescriptorConfigurator descriptorConfigurator{ graphicsPipelineConfigurator->shaderSet };

    configurator(descriptorConfigurator, _settings->options);

    auto& materialDescriptorBinding = graphicsPipelineConfigurator->shaderSet->getDescriptorBinding("material");
    auto binding = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS,
        graphicsPipelineConfigurator->layout, materialDescriptorBinding.set,
        descriptorConfigurator.descriptorSets.at(materialDescriptorBinding.set));

    if (share && _settings->options.valid() && _settings->options->sharedObjects.valid())
    {
        _settings->options->sharedObjects->share(binding);
    }

    return binding;
}

ref_ptr<View> Builder::createView(const ref_ptr<Node>& headlight) const
{
    auto proj = Perspective::create(*_camera->projectionMatrix.cast<Perspective>());
    auto cam = Camera::create(proj, _camera->viewMatrix, _camera->viewportState);

    auto view = View::create(cam);
    if (headlight.valid())
    {
        view->addChild(headlight);
    }

    return view;
}

ref_ptr<ClearAttachments> Builder::createClearAttachments() const
{
    ClearAttachments::Attachments attachments;
    attachments.push_back({ VK_IMAGE_ASPECT_COLOR_BIT, 0, {{{0.0f, 0.0f, 0.0f, 0.0f}}} });
    attachments.push_back({ VK_IMAGE_ASPECT_DEPTH_BIT, 0, {{{0.0f, 0}}} });

    VkClearRect clearRect;
    clearRect.rect.offset = { 0, 0 };
    clearRect.rect.extent = _settings->window->extent2D();
    clearRect.baseArrayLayer = 0;
    clearRect.layerCount = 1;

    ClearAttachments::Rects clearRects;
    clearRects.emplace_back(clearRect);

    return ClearAttachments::create(attachments, clearRects);
}

ref_ptr<Node> Builder::createPeelScene(Resources& resources, int32_t peelIndex) const
{
    auto scene = StateGroup::create();

    auto configurator = PeelPipelineConfigurator::getOrCreate(_settings->shadingModel, peelIndex, _settings->options);
    configurator->copyTo(scene);

    auto bindDescriptorSet = BindPeelDescriptorSet::create(
        resources, configurator->shaderSet, configurator->layout, peelIndex);
    scene->add(bindDescriptorSet);
    resources.registerDescriptorSetBinding(*bindDescriptorSet);

    scene->addChild(createClearAttachments());
    scene->addChild(_transparencyScene);

    return scene;
}

ref_ptr<Node> Builder::createPeelCombineScene(Resources& resources, int32_t peelIndex) const
{
    auto scene = StateGroup::create();

    auto configurator = PeelCombinePipelineConfigurator::getOrCreate(peelIndex, _settings->options);
    configurator->copyTo(scene);

    auto bindDescriptorSet = BindPeelCombineDescriptorSet::create(
        resources, configurator->shaderSet, configurator->layout);
    scene->add(bindDescriptorSet);
    resources.registerDescriptorSetBinding(*bindDescriptorSet);

    scene->addChild(Draw::create(3, 1, 0, 0));

    return scene;
}

ref_ptr<Node> Builder::createFinalCombineScene(Resources& resources) const
{
    auto scene = StateGroup::create();

    auto configurator = FinalCombinePipelineConfigurator::getOrCreate(_settings->numPeelLayers, _settings->options);
    configurator->copyTo(scene);

    auto bindDescriptorSet = BindFinalCombineDescriptorSet::create(
        resources, configurator->shaderSet, configurator->layout);
    scene->add(bindDescriptorSet);
    resources.registerDescriptorSetBinding(*bindDescriptorSet);

    scene->addChild(Draw::create(3, 1, 0, 0));

    return scene;
}

ref_ptr<vsg::RenderGraph> Builder::createOpaqueRenderGraph(const ref_ptr<Node>& headlight) const
{
    if (_settings == nullptr || _opaqueScene == nullptr)
    {
        return {};
    }

    auto configurator = OpaquePipelineConfigurator::getOrCreate(_settings->shadingModel, _settings->options);

    auto scene = StateGroup::create();
    configurator->copyTo(scene);
    scene->addChild(_opaqueScene);

    auto view = createView(headlight);
    view->addChild(scene);

    auto renderGraph = vsg::RenderGraph::create(_settings->window, view);
    renderGraph->contents = VK_SUBPASS_CONTENTS_INLINE;

    return renderGraph;
}

ref_ptr<vsg::RenderGraph> Builder::createTransparencyRenderGraph(const ref_ptr<Node>& headlight) const
{
    if (_settings == nullptr || _transparencyScene == nullptr || _settings->numPeelLayers == 0)
    {
        return {};
    }

    auto view = createView(headlight);
    auto renderGraph = RenderGraph::create(_settings->window, view);
    renderGraph->contents = VK_SUBPASS_CONTENTS_INLINE;
    renderGraph->numPeelLayers = _settings->numPeelLayers;

    auto scene = Group::create();
    for (auto peelIndex = 0; peelIndex < _settings->numPeelLayers; ++peelIndex)
    {
        scene->addChild(createPeelScene(*renderGraph->resources(), peelIndex));
        scene->addChild(NextSubPass::create());
        scene->addChild(createPeelCombineScene(*renderGraph->resources(), peelIndex));
        scene->addChild(NextSubPass::create());
    }
    scene->addChild(createFinalCombineScene(*renderGraph->resources()));

    view->addChild(scene);

    return renderGraph;
}
