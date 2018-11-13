#include <vsg/core/ref_ptr.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/Object.h>
#include <vsg/core/Auxiliary.h>
#include <vsg/core/Version.h>

#include <vsg/nodes/Group.h>
#include <vsg/nodes/FixedGroup.h>

#include <vsg/utils/CommandLine.h>

#include <vsg/io/Input.h>
#include <vsg/io/Output.h>

#include <iostream>
#include <cstring>
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

class AsciiOutput : public vsg::Output
{
public:

        AsciiOutput(std::ostream& output) :
            _output(output)
        {
            _maximumIndentation = std::strlen(_indentationString);

            // write header
            _output<<"#vsga "<<vsgGetVersion()<<"\n";
        }

        std::ostream& indent() { _output.write(_indentationString, std::min(_indentation, _maximumIndentation)); return _output; }

        // write single values
        void write(const char* propertyName, int8_t value) override   { indent()<<propertyName<<" "<<value<<"\n"; }
        void write(const char* propertyName, uint8_t value) override  { indent()<<propertyName<<" "<<value<<"\n"; }
        void write(const char* propertyName, int16_t value) override  { indent()<<propertyName<<" "<<value<<"\n"; }
        void write(const char* propertyName, uint16_t value) override { indent()<<propertyName<<" "<<value<<"\n"; }
        void write(const char* propertyName, int32_t value) override  { indent()<<propertyName<<" "<<value<<"\n"; }
        void write(const char* propertyName, uint32_t value) override { indent()<<propertyName<<" "<<value<<"\n"; }
        void write(const char* propertyName, int64_t value) override  { indent()<<propertyName<<" "<<value<<"\n"; }
        void write(const char* propertyName, uint64_t value) override { indent()<<propertyName<<" "<<value<<"\n"; }
        void write(const char* propertyName, float value) override    { indent()<<propertyName<<" "<<value<<"\n"; }
        void write(const char* propertyName, double value) override   { indent()<<propertyName<<" "<<value<<"\n"; }

        void write(const char* propertyName, const vsg::Object* object)
        {
            if (auto itr = _objectIDMap.find(object); itr !=  _objectIDMap.end())
            {
                // write out the objectID
                indent()<<propertyName<<" id="<<itr->second<<"\n";
                return;
            }

            ObjectID id = _objectID++;
            _objectIDMap[object] = id;
            indent()<<propertyName<<" id="<<id<<" "<<object->className()<<"\n";

            indent()<<"{\n";
            _indentation += _indentationStep;
            object->write(*this);
            _indentation -= _indentationStep;
            indent()<<"}\n";
        }

protected:

        std::ostream&   _output;

        using ObjectID = uint32_t;

#if 1
        using ObjectIDMap = std::map<const vsg::Object*, ObjectID>;
#else
        // 47% faster for overall write for large scene graph than std::map<>!
        using ObjectIDMap = std::unordered_map<const vsg::Object*, ObjectID>;
#endif

        ObjectIDMap     _objectIDMap;
        ObjectID        _objectID = 0;
        std::size_t     _indentationStep = 2;
        std::size_t     _indentation = 0;
        std::size_t     _maximumIndentation = 0;
        // 24 characters long enough for 12 levels of nesting
        const char*     _indentationString = "                        ";
};

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);
    //auto numLevels = arguments.value(4u, {"--levels", "-l"});
    auto numLevels = arguments.value(4u, "-l");
    auto inputFilename = arguments.value(std::string(), "-i");
    auto outputFilename = arguments.value(std::string(), "-o");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    std::cout<<"inputFilename "<<inputFilename<<std::endl;
    std::cout<<"outputFilename "<<outputFilename<<std::endl;


    auto leaf = vsg::Node::create();
    auto node = createQuadTree(numLevels, leaf);

    if (node)
    {
        AsciiOutput output(std::cout);
        output.write("Root", node);
    }

    std::cout<<"type_name "<<vsg::type_name(*leaf)<<std::endl;
    std::cout<<"type_name "<<vsg::type_name(*node)<<std::endl;
    std::cout<<"type_name "<<vsg::type_name(vsg::vec4(1.0, 2.0, 3.0, 4.0))<<std::endl;

    return 0;
}
