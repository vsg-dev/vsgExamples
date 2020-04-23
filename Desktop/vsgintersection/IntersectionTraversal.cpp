/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "IntersectionTraversal.h"

#include <vsg/nodes/CullNode.h>
#include <vsg/nodes/LOD.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/PagedLOD.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/nodes/VertexIndexDraw.h>

#include <iostream>

using namespace vsg;

IntersectionTraversal::IntersectionTraversal()
{
}

void IntersectionTraversal::apply(const Node& node)
{
    //std::cout<<"apply("<<node.className()<<")"<<std::endl;
    node.traverse(*this);
}

void IntersectionTraversal::apply(const MatrixTransform& transform)
{
    // std::cout<<"MT apply("<<transform.className()<<") "<<transform.getMatrix()<<std::endl;
    // TODO : transform intersectors into local coodinate frame

    intersectorStack.push_back(intersectorStack.back()->transform(transform.getMatrix()));

    std::cout<<"Transforn : "<<intersectorStack.size()<<std::endl;

    transform.traverse(*this);

    intersectorStack.pop_back();
}

void IntersectionTraversal::apply(const LOD& lod)
{
    std::cout<<"LOD apply("<<lod.className()<<") "<<std::endl;
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
    std::cout<<"PLOD apply("<<plod.className()<<") "<<std::endl;
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
    std::cout<<"CullNode apply("<<cn.className()<<") "<<std::endl;
    if (intersects(cn.getBound())) cn.traverse(*this);
}

void IntersectionTraversal::apply(const VertexIndexDraw& vid)
{
    std::cout<<"VertexIndexDraw apply("<<vid.className()<<") "<<std::endl;
    // TODO : Pass vertex array, indices and draw commands on to interesctors
}

void IntersectionTraversal::apply(const Geometry& geometry)
{
    std::cout<<"VertexIndexDraw apply("<<geometry.className()<<") "<<std::endl;
    // TODO : Pass vertex array, indices and draw commands on to interesctors
}

bool IntersectionTraversal::intersects(const dsphere& bs) const
{
    bool result = intersectorStack.back()->intersects(bs);
    std::cout<<"intersects(bs) <intersectorStack.size() = "<<intersectorStack.size()<<" result = "<<result<<std::endl;
    return result;
}
