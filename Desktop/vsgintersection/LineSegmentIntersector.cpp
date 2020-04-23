/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "LineSegmentIntersector.h"

#include <iostream>

using namespace vsg;

LineSegmentIntersector::LineSegmentIntersector(const dvec3& s, const dvec3& e) :
    start(s),
    end(e) {}

ref_ptr<Intersector> LineSegmentIntersector::transform(const dmat4& m)
{
    std::cout<<"LineSegmentIntersector::transform() TODO"<<std::endl;
    auto transformed = LineSegmentIntersector::create(m * start, m * end);
    return transformed;
}

bool LineSegmentIntersector::intersects(const dsphere& bs)
{
    std::cout<<"intersects( center = "<<bs.center<<", radius = "<<bs.radius<<")"<<std::endl;

    // if bs not valid then return true based on the assumption that an invalid sphere is yet to be defined.
    if (!bs.valid()) return true;

    dvec3 sm = start - bs.center;
    double c = length2(sm) - bs.radius * bs.radius;
    if (c<0.0) return true;

    dvec3 se = end-start;
    double a = length2(se);
    double b = dot(sm, se)*2.0;
    double d = b*b-4.0*a*c;

    if (d<0.0) return false;

    d = sqrt(d);

    double div = 1.0/(2.0*a);

    double r1 = (-b-d)*div;
    double r2 = (-b+d)*div;

    if (r1<=0.0 && r2<=0.0) return false;
    if (r1>=1.0 && r2>=1.0) return false;

    // passed all the rejection tests so line must intersect bounding sphere, return true.
    return true;
}

bool LineSegmentIntersector::intersects()
{
    std::cout<<"LineSegmentIntersector::intersects() mesh TODO"<<std::endl;
    return false;
}
