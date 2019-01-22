#include <vsg/core/ref_ptr.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/Object.h>
#include <vsg/core/Auxiliary.h>
#include <vsg/core/Array.h>
#include <vsg/core/Array2D.h>
#include <vsg/core/Version.h>

#include <vsg/nodes/Group.h>
#include <vsg/nodes/QuadGroup.h>
#include <vsg/nodes/StateGroup.h>

#include <vsg/utils/CommandLine.h>

#include <vsg/io/FileSystem.h>
#include <vsg/io/AsciiInput.h>
#include <vsg/io/AsciiOutput.h>
#include <vsg/io/BinaryInput.h>
#include <vsg/io/BinaryOutput.h>
#include <vsg/io/ObjectFactory.h>

#include <iostream>
#include <fstream>
#include <unordered_map>

vsg::ref_ptr<vsg::Node> createQuadTree(unsigned int numLevels, vsg::Node* sharedLeaf)
{
    if (numLevels==0) return sharedLeaf ? vsg::ref_ptr<vsg::Node>(sharedLeaf) : vsg::Node::create();

    vsg::ref_ptr<vsg::Group> t = vsg::Group::create();

    --numLevels;

    t->getChildren().reserve(4);

    t->addChild(createQuadTree(numLevels, sharedLeaf));
    t->addChild(createQuadTree(numLevels, sharedLeaf));
    t->addChild(createQuadTree(numLevels, sharedLeaf));
    t->addChild(createQuadTree(numLevels, sharedLeaf));

    return t;
}


vsg::ref_ptr<vsg::Node> createQuadGroupTree(unsigned int numLevels, vsg::Node* sharedLeaf)
{
    if (numLevels==0) return sharedLeaf ? vsg::ref_ptr<vsg::Node>(sharedLeaf) : vsg::Node::create();

    vsg::ref_ptr<vsg::QuadGroup> t = vsg::QuadGroup::create();

    --numLevels;

    t->setChild(0, createQuadGroupTree(numLevels, sharedLeaf));
    t->setChild(1, createQuadGroupTree(numLevels, sharedLeaf));
    t->setChild(2, createQuadGroupTree(numLevels, sharedLeaf));
    t->setChild(3, createQuadGroupTree(numLevels, sharedLeaf));

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

        object->setObject("my array", new vsg::floatArray{10.1f, 21.2f, 31.4f, 55.0f});
        object->setObject("my vec3Array", new vsg::vec3Array{
            {10.1f, 21.2f, 31.4f},
            {55.0f, 45.0f, -20.0f}
        });

        vsg::ref_ptr<vsg::vec4Array2D> image( new vsg::vec4Array2D(3, 3) );

        for(size_t i=0; i<image->width(); ++i)
        {
            for(size_t j=0; j<image->height(); ++j)
            {
                image->at(i, j) = vsg::vec4(i, j, i*j, 1.0f);
            }
        }

        for(auto& c : *image)
        {
            std::cout<<"image c="<<c<<std::endl;
        }

        object->setObject("image", image);

    }
    else
    {
        if (vsg::fileExists(inputFilename))
        {
            auto ext = vsg::fileExtension(inputFilename);
            std::cout<<"Input File extension "<<ext<<std::endl;

            if (ext=="vsga")
            {
                std::ifstream fin(inputFilename);
                vsg::AsciiInput input(fin);
                object = input.readObject("Root");
            }
            else if (ext=="vsgb")
            {
                std::ifstream fin(inputFilename, std::ios::in | std::ios::binary);
                vsg::BinaryInput input(fin);
                object = input.readObject("Root");
            }
            else
            {
                std::cout<<"Warning: file format not supported : "<<inputFilename<<std::endl;
                return 1;
            }

        }
        else
        {
            std::cout<<"Warning: could not find file : "<<inputFilename<<std::endl;
            return 1;
        }
    }

    if (object)
    {
        if (outputFilename.empty())
        {
            // write graph to console
            vsg::AsciiOutput output(std::cout);
            output.writeObject("Root", object);
        }
        else
        {
            auto ext = vsg::fileExtension(outputFilename);
            std::cout<<"Output File extension "<<ext<<std::endl;

            // write to specified file
            if (ext=="vsga")
            {
                std::ofstream fout(outputFilename);
                vsg::AsciiOutput output(fout);
                output.writeObject("Root", object);
            }
            else if (ext=="vsgb")
            {
                std::ofstream fout(outputFilename, std::ios::out | std::ios::binary);
                vsg::BinaryOutput output(fout);
                output.writeObject("Root", object);
            }
        }
    }

    return 0;
}
