#include <vsg/core/Array.h>
#include <vsg/core/Auxiliary.h>
#include <vsg/core/Object.h>
#include <vsg/core/Visitor.h>
#include <vsg/core/ConstVisitor.h>
#include <vsg/io/stream.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <mutex>

struct Unique
{
    using StringIndexMap = std::map<std::string, std::size_t>;
    using IndexStringMap = std::map<std::size_t, std::string>;

    std::mutex _mutex;
    StringIndexMap _stringIndexMap;
    IndexStringMap _indexStringMap;

    std::size_t getIndex(const std::string& name)
    {
        std::lock_guard<std::mutex> guard(_mutex);
        StringIndexMap::iterator itr = _stringIndexMap.find(name);
        if (itr != _stringIndexMap.end()) return itr->second;

        std::size_t s = _stringIndexMap.size();
        _stringIndexMap[name] = s;
        _indexStringMap[s] = name;
        return s;
    }

    std::string getName(std::size_t index)
    {
        std::lock_guard<std::mutex> guard(_mutex);
        IndexStringMap::iterator itr = _indexStringMap.find(index);
        if (itr != _indexStringMap.end()) return itr->second;
        return std::string();
    }
};

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
