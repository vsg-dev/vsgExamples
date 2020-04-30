#pragma once

#include <vsg/all.h>


struct Builder : public vsg::Inherit<vsg::Object, Builder>
{
    enum GeometryType
    {
        DRAW_COMMANDS,
        DRAW_INDEXED_COMMANDS,
        GEOMETRY,
        VERTEX_INDEX_DRAW
    };

    GeometryType geometryType = VERTEX_INDEX_DRAW;

    vsg::ref_ptr<vsg::Node> createQuad(const vsg::dvec3& dimensions = {1.0, 1.0, 1.0});
    vsg::ref_ptr<vsg::Node> createCube(const vsg::dvec3& dimensions = {1.0, 1.0, 1.0});

};

