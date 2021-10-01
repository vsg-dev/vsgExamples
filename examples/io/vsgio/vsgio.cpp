#include <vsg/all.h>

#include <fstream>
#include <iostream>
#include <unordered_map>

vsg::ref_ptr<vsg::Node> createQuadTree(unsigned int numLevels, vsg::Node* sharedLeaf)
{
    if (numLevels == 0) return sharedLeaf ? vsg::ref_ptr<vsg::Node>(sharedLeaf) : vsg::Node::create();

    vsg::ref_ptr<vsg::Group> t = vsg::Group::create();

    --numLevels;

    t->children = {{createQuadTree(numLevels, sharedLeaf),
                    createQuadTree(numLevels, sharedLeaf),
                    createQuadTree(numLevels, sharedLeaf),
                    createQuadTree(numLevels, sharedLeaf)}};

    return t;
}

vsg::ref_ptr<vsg::Node> createQuadGroupTree(unsigned int numLevels, vsg::Node* sharedLeaf)
{
    if (numLevels == 0) return sharedLeaf ? vsg::ref_ptr<vsg::Node>(sharedLeaf) : vsg::Node::create();

    vsg::ref_ptr<vsg::QuadGroup> t = vsg::QuadGroup::create();

    --numLevels;

    t->children = {{createQuadGroupTree(numLevels, sharedLeaf),
                    createQuadGroupTree(numLevels, sharedLeaf),
                    createQuadGroupTree(numLevels, sharedLeaf),
                    createQuadGroupTree(numLevels, sharedLeaf)}};

    return t;
}

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);
    auto numLevels = arguments.value(4u, {"--levels", "-l"});
    auto useQuadGroup = arguments.read("-q");
    auto inputFilename = arguments.value(std::string(), "-i");
    auto outputFilename = arguments.value(std::string(), "-o");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    vsg::ref_ptr<vsg::Object> object;
    if (inputFilename.empty())
    {
        auto leaf = vsg::Node::create();

        if (useQuadGroup)
        {
            object = createQuadGroupTree(numLevels, leaf);
        }
        else
        {
            object = createQuadTree(numLevels, leaf);
        }

        object->setValue("double_value", 10.0);
        object->setValue("string_value", "All the Kings men.");
        object->setValue("my vector", vsg::vec4{1.1f, 2.2f, 3.3f, 4.4f});

        object->setObject("my array", vsg::floatArray::create({10.1f, 21.2f, 31.4f, 55.0f}));
        object->setObject("my vec3Array", vsg::vec3Array::create({{10.1f, 21.2f, 31.4f},
                                                                  {55.0f, 45.0f, -20.0f}}));

        auto image = vsg::vec4Array2D::create(3, 3);

        for (uint32_t i = 0; i < image->width(); ++i)
        {
            for (uint32_t j = 0; j < image->height(); ++j)
            {
                image->at(i, j) = vsg::vec4(static_cast<float>(i), static_cast<float>(j), static_cast<float>(i * j), 1.0f);
            }
        }

        for (auto& c : *image)
        {
            std::cout << "image c=" << c << std::endl;
        }

        object->setObject("image", image);
    }
    else
    {
        if (vsg::fileExists(inputFilename))
        {
            vsg::VSG io;
            object = io.read(inputFilename);
            if (!object)
            {
                std::cout << "Warning: file not read : " << inputFilename << std::endl;
                return 1;
            }
        }
        else
        {
            std::cout << "Warning: could not find file : " << inputFilename << std::endl;
            return 1;
        }
    }

    if (object)
    {
        if (outputFilename.empty())
        {
            // write graph to console
            vsg::VSG io;
            io.write(object, std::cout);
        }
        else
        {
            vsg::write(object, outputFilename);
        }
    }

    return 0;
}
