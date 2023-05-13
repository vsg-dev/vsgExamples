#include <vsg/core/Array.h>
#include <vsg/core/Visitor.h>
#include <vsg/core/ConstVisitor.h>
#include <vsg/io/stream.h>

#include <iostream>

int main(int /*argc*/, char** /*argv*/)
{

    auto floats = vsg::floatArray::create(10);

    std::cout << "floats.size() = " << floats->size() << std::endl;

    float value = 0.0f;
    std::for_each(floats->begin(), floats->end(), [&value](float& v) {
        v = value++;
    });

    std::for_each(floats->begin(), floats->end(), [](float& v) {
        std::cout << "   v[] = " << v << std::endl;
    });

    auto colours = vsg::vec4Array::create(40);
    vsg::vec4 colour(0.25, 0.5, 0.75, 1.0);
    for (std::size_t i = 0; i < colours->size(); ++i)
    {
        (*colours)[i] = colour;
        colour = vsg::vec4(colour.g, colour.b, colour.a, colour.r);
    }

    std::for_each(colours->begin(), colours->end(), [](vsg::vec4& c) {
        std::cout << "   c[] = " << c << std::endl;
    });

    auto texCoords = vsg::vec2Array::create(
        {{1.0f, 2.0f},
         {3.0f, 4.0f},
         {}});

    std::cout << "texCoords.size() = " << texCoords->size() << std::endl;
    for (auto p : *texCoords)
    {
        std::cout << "    tc " << p.x << ", " << p.y << std::endl;
    }

    auto col = vsg::vec4Array::create({{}});
    std::cout << "col.size() = " << col->size() << std::endl;
    for (auto c : *col)
    {
        std::cout << "    colour " << c.r << ", " << c.g << ", " << c.b << ", " << c.a << std::endl;
    }

    return 0;
}
