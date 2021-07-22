#pragma once

#include <vsg/all.h>

struct GeometryInfo
{
    vsg::vec3 position = {0.0, 0.0, 0.0};
    vsg::vec3 dimensions = {1.0, 1.0, 1.0};
    vsg::vec4 color = {1.0, 1.0, 1.0, 1.0};
    vsg::ref_ptr<vsg::Data> image;

    bool operator < (const GeometryInfo& rhs) const
    {
        if (position < rhs.position) return true;
        if (rhs.position < position) return false;

        if (dimensions < rhs.dimensions) return true;
        if (rhs.dimensions < dimensions) return false;

        if (color < rhs.color) return true;
        if (rhs.color < color) return false;

        return image < rhs.image;
    }
};

class Builder : public vsg::Inherit<vsg::Object, Builder>
{
public:
    bool verbose = false;

    /// set up the compile traversal to compile for specified window
    void setup(vsg::ref_ptr<vsg::Window> window, vsg::ViewportState* viewport, uint32_t maxNumTextures = 32);

    void compile(vsg::ref_ptr<vsg::Node> subgraph);

    vsg::ref_ptr<vsg::Node> createBox(const GeometryInfo& info = {});
    vsg::ref_ptr<vsg::Node> createCapsule(const GeometryInfo& info = {});
    vsg::ref_ptr<vsg::Node> createCone(const GeometryInfo& info = {});
    vsg::ref_ptr<vsg::Node> createCylinder(const GeometryInfo& info = {});
    vsg::ref_ptr<vsg::Node> createQuad(const GeometryInfo& info = {});
    vsg::ref_ptr<vsg::Node> createSphere(const GeometryInfo& info = {});

private:
    uint32_t _allocatedTextureCount = 0;
    uint32_t _maxNumTextures = 0;
    vsg::ref_ptr<vsg::CompileTraversal> _compile;

    vsg::ref_ptr<vsg::DescriptorSetLayout>_descriptorSetLayout;
    vsg::ref_ptr<vsg::PipelineLayout> _pipelineLayout;
    vsg::ref_ptr<vsg::BindGraphicsPipeline> _bindGraphicsPipeline;

    std::map<vsg::vec4, vsg::ref_ptr<vsg::vec4Array2D>> _colorData;
    std::map<vsg::ref_ptr<vsg::Data>, vsg::ref_ptr<vsg::BindDescriptorSets>> _textureDescriptorSets;

    vsg::ref_ptr<vsg::BindGraphicsPipeline> _createGraphicsPipeline();
    vsg::ref_ptr<vsg::BindDescriptorSets> _createTexture(const GeometryInfo& info);

    using GeometryMap = std::map<GeometryInfo, vsg::ref_ptr<vsg::Node>>;
    GeometryMap _boxes;
    GeometryMap _capsules;
    GeometryMap _cones;
    GeometryMap _cylinder;
    GeometryMap _quads;
    GeometryMap _spheres;
};
