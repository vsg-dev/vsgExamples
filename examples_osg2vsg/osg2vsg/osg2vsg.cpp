#include <vsg/io/AsciiInput.h>
#include <vsg/io/AsciiOutput.h>
#include <vsg/io/BinaryInput.h>
#include <vsg/io/BinaryOutput.h>
#include <vsg/io/FileSystem.h>

#include <vsg/nodes/Group.h>

#include <vsg/utils/CommandLine.h>

#include <osgDB/FileUtils>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>

#include <chrono>

using OsgObjects = std::vector<osg::ref_ptr<osg::Object>>;
using VsgObjects = std::vector<vsg::ref_ptr<vsg::Object>>;

vsg::ref_ptr<vsg::Node> convertToVsg(osg::Node* osgNode)
{
    // TODO
    std::cout<<"convertToVsg("<<osgNode->getCompoundClassName()<<") node version"<<std::endl;

    return vsg::ref_ptr<vsg::Node>();
}

vsg::ref_ptr<vsg::Object> convertToVsg(osg::Object* osgObject)
{
    // TODO
    std::cout<<"convertToVsg("<<osgObject->getCompoundClassName()<<") object version."<<std::endl;

    return vsg::ref_ptr<vsg::Object>();
}

vsg::ref_ptr<vsg::Object> convertToVsg(VsgObjects& vsgObjects, OsgObjects& osgObjects)
{
    if (vsgObjects.size()==1 && osgObjects.empty()) return vsgObjects.front();

    using VsgNodes = std::vector<vsg::ref_ptr<vsg::Node>>;
    using OsgNodes = std::vector<osg::ref_ptr<osg::Node>>;

    VsgNodes vsgNodes;
    VsgObjects vsgRemainingObjects;
    for(auto& object : vsgObjects)
    {
        auto node = dynamic_cast<vsg::Node*>(object.get());
        if (node) vsgNodes.push_back(vsg::ref_ptr<vsg::Node>(node));
        else vsgRemainingObjects.push_back(object);
    }

    OsgNodes osgNodes;
    OsgObjects osgRemainingObjects;
    for(auto& object : osgObjects)
    {
        auto node = object->asNode();
        if (node) osgNodes.push_back(node);
        else osgRemainingObjects.push_back(object.get());
    }

    vsg::ref_ptr<vsg::Object> root;

    if ((vsgNodes.size()+osgNodes.size())>1)
    {
        // input contains nodes
        auto group = vsg::Group::create();

        for (auto& node : vsgNodes)
        {
            group->addChild(node);
        }

        for (auto& osgNode : osgNodes)
        {
            auto vsgNode = convertToVsg(osgNode.get());
            if (vsgNode) group->addChild(vsgNode);
        }

        root = group;
    }
    else if (vsgNodes.size()==1)
    {
        root = vsgNodes.front();
    }
    else if (osgNodes.size()==1)
    {
        root = convertToVsg(osgNodes.front().get());
    }

    if ((vsgRemainingObjects.size()+osgRemainingObjects.size())>=1)
    {
        if (!root) root = new vsg::Object;

        unsigned int i=0;
        for(auto& object : vsgRemainingObjects)
        {
            root->setObject(vsg::make_string("vsg_object_",i), object);
        }

        for(auto& osgObject : osgRemainingObjects)
        {

            auto object = convertToVsg(osgObject.get());
            root->setObject(vsg::make_string("osg_object_",i), object);
        }
    }

    return root;
}

osg::ref_ptr<osg::Object> convertToOsg(VsgObjects& vsgObjects, OsgObjects& osgObjects)
{
    return nullptr;
}

int main(int argc, char** argv)
{
    using Filenames = std::vector<std::string>;

    vsg::CommandLine arguments(&argc, argv);

    Filenames inputFilenames;
    for(std::string filename; arguments.read("-i", filename);) inputFilenames.push_back(filename);

    Filenames outputFilenames;
    for(std::string filename; arguments.read("-o", filename);) outputFilenames.push_back(filename);

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    using clock = std::chrono::high_resolution_clock;
    clock::time_point start = clock::now();

    VsgObjects vsgObjects;
    OsgObjects osgObjects;

    // read input data
    for(auto& filename : inputFilenames)
    {
        auto ext = vsg::fileExtension(filename);
        std::cout<<"Input filename : "<<filename<<" ext="<<ext<<std::endl;
        if (ext=="vsga")
        {
            vsg::ref_ptr<vsg::Object> object;
            try
            {
                std::ifstream fin(filename);
                vsg::AsciiInput input(fin);
                object = input.readObject("Root");
            }
            catch(std::string message)
            {
                std::cout<<"Warning : vsg::AsciiInput() on file="<<filename<<" failed. mssage="<<message<<std::endl;
                return 1;
            }

            if (object)
            {
                std::cout<<"Read ascii vsg file "<<filename<<" root object type = "<<object->className()<<std::endl;
                vsgObjects.push_back(object);
            }
        }
        else if (ext=="vsgb")
        {
            vsg::ref_ptr<vsg::Object> object;
            try
            {
                std::ifstream fin(filename, std::ios::in | std::ios::binary);
                vsg::BinaryInput input(fin);
                object = input.readObject("Root");
            }
            catch(std::string message)
            {
                std::cout<<"Warning : vsg::BinaryInput() on file="<<filename<<" failed. mssage="<<message<<std::endl;
                return 1;
            }

            if (object)
            {
                std::cout<<"Read binary vsg file "<<filename<<" root object type = "<<object->className()<<std::endl;
                vsgObjects.push_back(object);
            }
        }
        else
        {
            osg::ref_ptr<osg::Object> object = osgDB::readObjectFile(filename);
            if (object)
            {
                std::cout<<"Read osg file "<<filename<<" root object type = "<<object->className()<<std::endl;
                osgObjects.push_back(object.get());
            }
        }
    }

    for(auto& filename : outputFilenames)
    {
        auto ext = vsg::fileExtension(filename);
        std::cout<<"Output filename : "<<filename<<" ext="<<ext<<std::endl;
        if (ext=="vsga")
        {
            vsg::ref_ptr<vsg::Object> object = convertToVsg(vsgObjects, osgObjects);
            if (!object)
            {
                std::cout<<"Warning: unable to convert loaded data to VSG objects"<<std::endl;
                return 1;
            }

            try
            {
                std::ofstream fout(filename);
                vsg::AsciiOutput output(fout);
                output.writeObject("Root", object);
            }
            catch(std::string message)
            {
                std::cout<<"Warning : vsg::AsciiInput() on file="<<filename<<" failed. mssage="<<message<<std::endl;
                return 1;
            }
        }
        else if (ext=="vsgb")
        {
            vsg::ref_ptr<vsg::Object> object = convertToVsg(vsgObjects, osgObjects);
            if (!object)
            {
                std::cout<<"Warning: unable to convert loaded data to VSG objects"<<std::endl;
                return 1;
            }

            try
            {
                std::ofstream fout(filename, std::ios::out | std::ios::binary);
                vsg::BinaryOutput output(fout);
                output.writeObject("Root", object);
            }
            catch(std::string message)
            {
                std::cout<<"Warning : vsg::BinaryInput() on file="<<filename<<" failed. mssage="<<message<<std::endl;
                return 1;
            }
        }
        else
        {
            osg::ref_ptr<osg::Object> object = convertToOsg(vsgObjects, osgObjects);
            if (object)
            {
                std::cout<<"Writing osg file "<<filename<<" root object type = "<<object->className()<<std::endl;
                osgDB::writeObjectFile(*object, filename);
            }
            else
            {
                std::cout<<"Warning: unable to convert loaded data to OSG objects"<<std::endl;
                return 1;
            }
        }
    }

    clock::time_point after = clock::now();

    std::cout<<"Time to run : "<<std::chrono::duration<double>(after-start).count()<<std::endl;

    return 0;
}
