/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "IntersectionTraversal.h"

#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/CullNode.h>
#include <vsg/nodes/LOD.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/PagedLOD.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/vk/Draw.h>
#include <vsg/maths/transform.h>

#include <iostream>

#include "LineSegmentIntersector.h"

using namespace vsg;

struct PushPopNode
{
    NodePath& nodePath;

    PushPopNode(NodePath& np, const Node* node) : nodePath(np) { nodePath.push_back(node); }
    ~PushPopNode() { nodePath.pop_back(); }
};

IntersectionTraversal::IntersectionTraversal()
{
}

void IntersectionTraversal::apply(const Node& node)
{
    //std::cout<<"apply("<<node.className()<<")"<<std::endl;
    PushPopNode ppn(nodePath, &node);

    node.traverse(*this);
}

void IntersectionTraversal::apply(const StateGroup& stategroup)
{
    // TODO need to check for topology type. GraphicsProgram::InputAssemblyState.topology (VkPrimitiveTopology)

    PushPopNode ppn(nodePath, &stategroup);

    stategroup.traverse(*this);
}

void IntersectionTraversal::apply(const MatrixTransform& transform)
{
    PushPopNode ppn(nodePath, &transform);

    pushTransform(vsg::inverse(transform.getMatrix()));

    transform.traverse(*this);

    popTransform();
}



void IntersectionTraversal::apply(const LOD& lod)
{
    PushPopNode ppn(nodePath, &lod);

    if (intersects(lod.getBound()))
    {
        for(auto& child : lod.getChildren())
        {
            if (child.node)
            {
                child.node->accept(*this);
                break;
            }
        }
    }
}

void IntersectionTraversal::apply(const PagedLOD& plod)
{
    PushPopNode ppn(nodePath, &plod);

    if (intersects(plod.getBound()))
    {
        for(auto& child : plod.getChildren())
        {
            if (child.node)
            {
                child.node->accept(*this);
                break;
            }
        }
    }
}

void IntersectionTraversal::apply(const CullNode& cn)
{
    PushPopNode ppn(nodePath, &cn);

    if (intersects(cn.getBound())) cn.traverse(*this);
}

void IntersectionTraversal::apply(const VertexIndexDraw& vid)
{
    if (vid.arrays.empty()) return;

    PushPopNode ppn(nodePath, &vid);

    sphere bound;
    if (!vid.getValue("bound", bound))
    {
        box bb;
        if (auto vertices = vid.arrays[0].cast<vec3Array>(); vertices)
        {
            for(auto& vertex : *vertices) bb.add(vertex);
        }

        if (bb.valid())
        {
            bound.center = (bb.min + bb.max) * 0.5f;
            bound.radius = length(bb.max - bb.min) * 0.5f;

            // hacky but better to reuse results.  Perhaps use a std::map<> to avoid a breaking const, or make the vistitor non const?
            const_cast<VertexIndexDraw&>(vid).setValue("bound", bound);
        }

        // std::cout<<"Computed bounding sphere : "<<bound.center<<", "<<bound.radius<<std::endl;
    }
    else
    {
        // std::cout<<"Found bounding sphere : "<<bound.center<<", "<<bound.radius<<std::endl;
    }

    if (intersects(bound))
    {
        intersect(topology, vid.arrays, vid.indices, vid.firstIndex, vid.indexCount);
    }
}

void IntersectionTraversal::apply(const Geometry& geometry)
{
    PushPopNode ppn(nodePath, &geometry);

    struct DrawCommandVisitor : public ConstVisitor
    {
        DrawCommandVisitor(IntersectionTraversal& it, VkPrimitiveTopology in_topology, const Geometry& in_geometry) :
            intersectionTraversal(it),
            topology(topology),
            geometry(in_geometry) {}

        IntersectionTraversal& intersectionTraversal;
        VkPrimitiveTopology topology;
        const Geometry& geometry;

        void apply(const Draw& draw) override
        {
            intersectionTraversal.intersect(topology, geometry.arrays, draw.firstVertex, draw.vertexCount);
        }
        void apply(const DrawIndexed& drawIndexed) override
        {
            intersectionTraversal.intersect(topology, geometry.arrays, geometry.indices, drawIndexed.firstIndex, drawIndexed.indexCount);
        }
    };

    DrawCommandVisitor drawCommandVisitor{*this, topology, geometry};

    for(auto& command : geometry.commands)
    {
        PushPopNode ppn(nodePath, command);

        command->accept(drawCommandVisitor);
    }

    // TODO : Pass vertex array, indices and draw commands on to interesctors
}

