#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/ConstVisitor.h>
#include <vsg/core/Inherit.h>
#include <vsg/core/Data.h>
#include <vsg/nodes/Node.h>

#include <list>

namespace vsg
{

    using NodePath = std::vector<const Node*>;

    class VSG_DECLSPEC IntersectionTraversal : public Inherit<ConstVisitor, IntersectionTraversal>
    {
    public:

        std::vector<dmat4> matrixStack;

        /// TODO : need to use a topologyStack
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        NodePath nodePath;

        IntersectionTraversal();

        void apply(const Node& node) override;
        void apply(const StateGroup& stategroup) override;
        void apply(const MatrixTransform& transform) override;
        void apply(const LOD& lod) override;
        void apply(const PagedLOD& plod) override;
        void apply(const CullNode& cn) override;
        void apply(const VertexIndexDraw& vid) override;
        void apply(const Geometry& geometry) override;

        /// clone and transform this Intersector to provide a new Intersector in local coordinates
        virtual void pushTransform(const dmat4& m) = 0;

        virtual void popTransform() = 0;

        /// check for intersection instersects with sphere
        virtual bool intersects(const dsphere& sphere) = 0;

        /// check for intersections with primitives associated with VkDrawDraw command
        virtual bool intersect(VkPrimitiveTopology topology, const DataList& arrays, uint32_t firstVertex, uint32_t vertexCount) = 0;

        /// check for intersections with primitives associated with VkDrawDrawIndex command
        virtual bool intersect(VkPrimitiveTopology topology, const DataList& arrays, ref_ptr<Data> indices, uint32_t firstIndex, uint32_t indexCount) = 0;
    };

}
