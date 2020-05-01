#pragma once

#include <vsg/all.h>


class Builder : public vsg::Inherit<vsg::Object, Builder>
{
public:
    enum GeometryType
    {
        DRAW_COMMANDS,
        DRAW_INDEXED_COMMANDS,
        GEOMETRY,
        VERTEX_INDEX_DRAW
    };

    GeometryType geometryType = VERTEX_INDEX_DRAW;

    /// set up the compile traversal to compile for specified window
    void setup(vsg::ref_ptr<vsg::Window> window, vsg::ViewportState* viewport, uint32_t maxNumTextures = 32);

    void compile(vsg::ref_ptr<vsg::Node>  subgraph);

    vsg::ref_ptr<vsg::Node> createQuad(const vsg::dvec3& dimensions = {1.0, 1.0, 1.0});
    vsg::ref_ptr<vsg::Node> createCube(const vsg::dvec3& dimensions = {1.0, 1.0, 1.0});

private:
    uint32_t _allocatedTextureCount = 0;
    uint32_t _maxNumTextures = 0;
    vsg::ref_ptr<vsg::CompileTraversal> _compile;

};

