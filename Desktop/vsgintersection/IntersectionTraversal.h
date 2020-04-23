#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/ConstVisitor.h>
#include <vsg/core/Inherit.h>
#include <vsg/nodes/Node.h>

#include <list>

namespace vsg
{

using NodePath = std::vector<ref_ptr<Node>>;

class Intersector : public Inherit<Object, Intersector>
{
public:
    /// clone and transform this Intersector to provide a new Intersector in local coordinates
    virtual ref_ptr<Intersector> transform(const dmat4& m) = 0;

    /// check of this intersector instersects with sphere
    virtual bool intersects(const dsphere& sphere) = 0;

    /// check of this intersector instersects with mesh
    /// vertices, indices and draw command
    virtual bool intersects() = 0;
};

class VSG_DECLSPEC IntersectionTraversal : public Inherit<ConstVisitor, IntersectionTraversal>
{
public:

    std::list<ref_ptr<Intersector>> intersectorStack;

    IntersectionTraversal();

    void apply(const Node& node) override;
    void apply(const MatrixTransform& transform) override;
    void apply(const LOD& lod) override;
    void apply(const PagedLOD& plod) override;
    void apply(const CullNode& cn) override;
    void apply(const VertexIndexDraw& vid) override;
    void apply(const Geometry& geometry) override;

    bool intersects(const dsphere& bs) const;

};

}
