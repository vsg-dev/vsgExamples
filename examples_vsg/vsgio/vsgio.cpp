#include <vsg/core/ref_ptr.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/Object.h>
#include <vsg/core/Auxiliary.h>
#include <vsg/core/Version.h>

#include <vsg/nodes/Group.h>
#include <vsg/nodes/QuadGroup.h>
#include <vsg/nodes/StateGroup.h>

#include <vsg/utils/CommandLine.h>

#include <vsg/utils/FileSystem.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>

#include <iostream>
#include <fstream>
#include <cstring>
#include <unordered_map>
#include <functional>


vsg::ref_ptr<vsg::Node> createQuadTree(unsigned int numLevels, vsg::Node* sharedLeaf)
{
#if 0
    if (numLevels==0) return sharedLeaf ? vsg::ref_ptr<vsg::Node>(sharedLeaf) : vsg::Node::create();
#else
    if (numLevels==0) return vsg::ref_ptr<vsg::Node>();
#endif

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

        template<typename T> void _write(const char* propertyName, T value)
        {
            indent()<<propertyName<<" "<<value<<"\n";
        }

        // write single values
        void write(const char* propertyName, int8_t value) override   { _write(propertyName, value); }
        void write(const char* propertyName, uint8_t value) override  { _write(propertyName, value); }
        void write(const char* propertyName, int16_t value) override  { _write(propertyName, value); }
        void write(const char* propertyName, uint16_t value) override { _write(propertyName, value); }
        void write(const char* propertyName, int32_t value) override  { _write(propertyName, value); }
        void write(const char* propertyName, uint32_t value) override { _write(propertyName, value); }
        void write(const char* propertyName, int64_t value) override  { _write(propertyName, value); }
        void write(const char* propertyName, uint64_t value) override { _write(propertyName, value); }
        void write(const char* propertyName, float value) override    { _write(propertyName, value); }
        void write(const char* propertyName, double value) override   { _write(propertyName, value); }

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

            if (object)
            {
                indent()<<propertyName<<" id="<<id<<" "<<object->className()<<"\n";
                indent()<<"{\n";
                _indentation += _indentationStep;
                object->write(*this);
                _indentation -= _indentationStep;
                indent()<<"}\n";
            }
            else
            {
                indent()<<propertyName<<" id="<<id<<" nullptr\n";
            }
        }

protected:

        std::ostream&   _output;

        using ObjectID = uint32_t;
#if 0
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

class ObjectFactory : public vsg::Object
{
public:


    using CreateFunction = std::function<vsg::ref_ptr<vsg::Object>()>;
    using CreateMap = std::map<std::string, CreateFunction>;

    CreateMap _createMap;


    ObjectFactory()
    {
        _createMap["nulltr"] = [](){ return vsg::ref_ptr<vsg::Object>(); };
        _createMap["vsg::Object"] = [](){ return vsg::ref_ptr<vsg::Object>(new vsg::Object()); };

        // ndodes
        _createMap["vsg::Node"] = [](){ return vsg::Node::create(); };
        _createMap["vsg::Group"] = [](){ return vsg::Group::create(); };
        _createMap["vsg::QuadGroup"] = [](){ return vsg::QuadGroup::create(); };
        _createMap["vsg::StateGroup"] = [](){ return vsg::StateGroup::create(); };
    }

    vsg::ref_ptr<vsg::Object> create(const std::string& className)
    {
        if (auto itr = _createMap.find(className); itr!=_createMap.end())
        {
            std::cout<<"Using _createMap for "<<className<<std::endl;
            return (itr->second)();
        }

        std::cout<<"Warnig: ObjectFactory::create("<<className<< ") failed to find means to create object"<<std::endl;
        return vsg::ref_ptr<vsg::Object>();
    }
};

class AsciiInput : public vsg::Input
{
public:

        using ObjectID = uint32_t;

        AsciiInput(std::istream& input) :
            _input(input)
        {
            _objectFactory = new ObjectFactory;

            // write header
            const char* match_token="#vsga";
            char read_token[5];
            _input.read(read_token, 5);
            if (std::strncmp(match_token, read_token, 5)!=0)
            {
                std::cout<<"Header token not matched"<<std::endl;
                throw std::string("Error: header not matched.");
            }

            char read_line[1024];
            _input.getline(read_line, sizeof(read_line)-1);
            std::cout<<"First line ["<<read_line<<"]"<<std::endl;
        }

        bool match(const char* propertyName)
        {
            if (propertyName)
            {
                _input >> _readPropertyName;
                if (_readPropertyName!=propertyName)
                {
                    std::cout<<"Error: unable to match "<<propertyName<<std::endl;
                    throw vsg::make_string("Error: unable to match ", propertyName);
                }
            }
            return true;
        }

        std::optional<ObjectID> objectID()
        {
            std::string token;
            _input >> token;
            if (token.compare(0, 3, "id=")==0)
            {
                token.erase(0, 3);
                std::stringstream str(token);
                ObjectID id;
                str>>id;
                return std::optional<ObjectID>{id};
            }
            else
            {
                return std::nullopt;
            }
        }

        template<typename T> void _read(const char* propertyName, T& value)
        {
            if (match(propertyName)) _input >> value;
        }

        // read single values
        virtual void read(const char* propertyName, int8_t& value) { _read(propertyName, value); }
        virtual void read(const char* propertyName, uint8_t& value) {_read(propertyName, value); }
        virtual void read(const char* propertyName, int16_t& value) { _read(propertyName, value); }
        virtual void read(const char* propertyName, uint16_t& value) { _read(propertyName, value); }
        virtual void read(const char* propertyName, int32_t& value) { _read(propertyName, value); }
        virtual void read(const char* propertyName, uint32_t& value) { _read(propertyName, value); }
        virtual void read(const char* propertyName, int64_t& value) { _read(propertyName, value); }
        virtual void read(const char* propertyName, uint64_t& value) { _read(propertyName, value); }
        virtual void read(const char* propertyName, float& value) { _read(propertyName, value); }
        virtual void read(const char* propertyName, double& value) { _read(propertyName, value); }

#if 0
        // read contiguous array of values
        virtual void read(const char* propertyName, size_t num, int8_t* values) 0;
        virtual void read(const char* propertyName, size_t num, uint8_t& value) 0;
        virtual void read(const char* propertyName, size_t num, int16_t& value) 0;
        virtual void read(const char* propertyName, size_t num, uint16_t& value) 0;
        virtual void read(const char* propertyName, size_t num, int32_t& value) 0;
        virtual void read(const char* propertyName, size_t num, uint32_t& value) 0;
        virtual void read(const char* propertyName, size_t num, int64_t& value) 0;
        virtual void read(const char* propertyName, size_t num, uint64_t& value) 0;
        virtual void read(const char* propertyName, size_t num, float& value) 0;
        virtual void read(const char* propertyName, size_t num, double& value) 0;
#endif
        // read object
        virtual vsg::ref_ptr<vsg::Object> readObject(const char* propertyName)
        {
            if (match(propertyName))
            {
                std::cout<<"Matched "<<propertyName<<" need to read object"<<std::endl;

                auto result = objectID();
                if (result)
                {
                    ObjectID id = result.value();
                    std::cout<<"   matched result="<<id<<std::endl;

                    if (auto itr = _objectIDMap.find(id); itr != _objectIDMap.end())
                    {
                        std::cout<<"Returning existing object "<<itr->second.get()<<std::endl;
                        return itr->second;
                    }
                    else
                    {
                        std::string className;
                        _input >> className;

                        std::cout<<"Loading new object "<<className<<std::endl;

                        vsg::ref_ptr<vsg::Object> object = _objectFactory->create(className.c_str());

                        if (object)
                        {
                            match("{");

                            object->read(*this);

                            std::cout<<"Loaded object, assigning to _objectIDMap."<<object.get()<<std::endl;

                            match("}");

                            _objectIDMap[id] = object;

                            return object;
                        }
                        else
                        {
                            std::cout<<"Could not find means to create object"<<std::endl;
                        }
                    }

                }

            }
            return vsg::ref_ptr<vsg::Object>();
        }

protected:

        std::istream&   _input;

#if 0
        using ObjectIDMap = std::map<ObjectID, vsg::ref_ptr<vsg::Object>>;
#else
        // 47% faster for overall write for large scene graph than std::map<>!
        using ObjectIDMap = std::unordered_map<ObjectID, vsg::ref_ptr<vsg::Object>>;
#endif

        std::string                 _readPropertyName;
        ObjectIDMap                 _objectIDMap;
        vsg::ref_ptr<ObjectFactory> _objectFactory;
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

    vsg::ref_ptr<vsg::Object> object;
    if (inputFilename.empty())
    {
        auto leaf = vsg::Node::create();
        object = createQuadTree(numLevels, leaf);
    }
    else
    {
        if (vsg::fileExists(inputFilename))
        {
            std::ifstream fin(inputFilename);
            AsciiInput input(fin);
            object = input.readObject("Root");
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
            AsciiOutput output(std::cout);
            output.write("Root", object);
        }
        else
        {
            // write to specified file
            std::ofstream fout(outputFilename);
            AsciiOutput output(fout);
            output.write("Root", object);
        }
    }

    return 0;
}
