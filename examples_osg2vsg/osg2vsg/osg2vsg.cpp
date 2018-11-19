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

vsg::ref_ptr<vsg::Data> convertToVsg(osg::Image* image)
{
    // TODO
    std::cout<<"convertToVsg("<<image->getCompoundClassName()<<") image version"<<std::endl;

    return vsg::ref_ptr<vsg::Data>(new vsg::vec4Array2D(image->s(), image->t()));

//    return vsg::ref_ptr<vsg::Data>();
}

vsg::ref_ptr<vsg::Object> convertToVsg(osg::Object* osgObject)
{
    if (osg::Image* image = osgObject->asImage()) return convertToVsg(image);

    // TODO
    std::cout<<"convertToVsg("<<osgObject->getCompoundClassName()<<") object version."<<std::endl;

    return vsg::ref_ptr<vsg::Object>();
}

vsg::ref_ptr<vsg::Object> convertToVsg(VsgObjects& vsgObjects, OsgObjects& osgObjects)
{
    if (vsgObjects.size()==1 && osgObjects.empty()) return vsgObjects.front();

    using VsgNodes = std::vector<vsg::ref_ptr<vsg::Node>>;
    using VsgData = std::vector<vsg::ref_ptr<vsg::Data>>;

    VsgNodes vsgNodes;
    VsgData vsgData;
    VsgObjects vsgRemainingObjects;
    for(auto& object : vsgObjects)
    {
        if (auto node = dynamic_cast<vsg::Node*>(object.get()); node!=nullptr) vsgNodes.emplace_back(node);
        else if (auto data = dynamic_cast<vsg::Data*>(object.get()); data!=nullptr) vsgData.emplace_back(data);
        else vsgRemainingObjects.emplace_back(object);
    }

    for(auto& object : osgObjects)
    {
        if (object->asNode())
        {
            if (auto node = convertToVsg(object->asNode()); node.valid()) vsgNodes.emplace_back(node);
        }
        else if (object->asImage())
        {
            if (auto data = convertToVsg(object->asImage()); data.valid()) vsgData.emplace_back(data);
        }
        else
        {
            if (auto vsgObject = convertToVsg(object.get()); vsgObject.valid()) vsgRemainingObjects.emplace_back(vsgObject);
        }
    }

    vsg::ref_ptr<vsg::Object> root;
    if (vsgNodes.size()>1)
    {
        // input contains nodes
        auto group = vsg::Group::create();

        for (auto& node : vsgNodes)
        {
            group->addChild(node);
        }

        root = group;
    }
    else if (vsgNodes.size()==1)
    {
        root = vsgNodes.front();
    }
    else if ((vsgData.size()+vsgRemainingObjects.size())>1)
    {
        root = new vsg::Object;
    }
    else if (vsgData.size()==1)
    {
        return vsgData.front();
    }
    else if (vsgRemainingObjects.size()==1)
    {
        return vsgRemainingObjects.front();
    }

    unsigned int i=0;
    for (auto& data : vsgData)
    {
        root->setObject(vsg::make_string("data_",i++), data);
    }

    for (auto& object: vsgRemainingObjects)
    {
        root->setObject(vsg::make_string("object_",i++), object);
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
                std::cout<<"Read osg file "<<filename<<" root object type = "<<object->getCompoundClassName()<<std::endl;
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
