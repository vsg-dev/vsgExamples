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

#include <vsg/utils/FileSystem.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>
#include <vsg/io/ObjectFactory.h>

#include <iostream>
#include <fstream>
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

class AsciiOutput : public vsg::Output
{
public:

        AsciiOutput(std::ostream& output) :
            _output(output)
        {
            _maximumIndentation = std::strlen(_indentationString);

            _output.imbue(std::locale::classic());

            // write header
            _output<<"#vsga "<<vsgGetVersion()<<"\n";
        }

        std::ostream& indent() { _output.write(_indentationString, std::min(_indentation, _maximumIndentation)); return _output; }

        // write property name if appropriate for format
        virtual void writePropertyName(const char* propertyName)
        {
            indent()<<propertyName;
        }

        template<typename T> void _write(size_t num, const T* value)
        {
            if (num==1)
            {
                _output<<' '<<*value<<'\n';
            }
            else
            {
                for(;num>0;--num, ++value) _output<<' '<<*value;
                _output<<'\n';
            }
        }


        // write contiguous array of value(s)
        void write(size_t num, const int8_t* value) override        { _write(num, value); }
        void write(size_t num, const uint8_t* value) override       { _write(num, value); }
        void write(size_t num, const int16_t* value)  override      { _write(num, value); }
        void write(size_t num, const uint16_t* value)  override     { _write(num, value); }
        void write(size_t num, const int32_t* value)  override      { _write(num, value); }
        void write(size_t num, const uint32_t* value)  override     { _write(num, value); }
        void write(size_t num, const int64_t* value)  override      { _write(num, value); }
        void write(size_t num, const uint64_t* value)  override     { _write(num, value); }
        void write(size_t num, const float* value)  override        { _write(num, value); }
        void write(size_t num, const double* value)  override       { _write(num, value); }


        void _write(const std::string& str)
        {
            _output<<'"';
            for(auto c : str)
            {
                if (c=='"') _output<<"\\\"";
                else _output<<c;
            }
            _output<<'"';
        }

        void write(size_t num, const std::string* value) override
        {
            if (num==1)
            {
                _output<<' ';
                _write(*value);
                _output<<'\n';
            }
            else
            {
                for(;num>0;--num, ++value)
                {
                    _output<<' ';
                    _write(*value);
                }
                _output<<'\n';
            }
        }

        // write object
        void write(const vsg::Object* object) override
        {
            if (auto itr = _objectIDMap.find(object); itr !=  _objectIDMap.end())
            {
                // write out the objectID
                _output<<" id="<<itr->second<<"\n";
                return;
            }

            ObjectID id = _objectID++;
            _objectIDMap[object] = id;

            if (object)
            {
#if 0
                _output<<"id="<<id<<" "<<vsg::type_name(*object)<<"\n";
#else
                _output<<" id="<<id<<" "<<object->className()<<"\n";
#endif
                indent()<<"{\n";
                _indentation += _indentationStep;
                object->write(*this);
                _indentation -= _indentationStep;
                indent()<<"}\n";
            }
            else
            {
                _output<<" id="<<id<<" nullptr\n";
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

class AsciiInput : public vsg::Input
{
public:

        using ObjectID = uint32_t;

        AsciiInput(std::istream& input) :
            _input(input)
        {
            _input.imbue(std::locale::classic());

            _objectFactory = new vsg::ObjectFactory;

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

        bool matchPropertyName(const char* propertyName) override
        {
            _input >> _readPropertyName;
            if (_readPropertyName!=propertyName)
            {
                std::cout<<"Error: unable to match "<<propertyName<<std::endl;
                return false;
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

        template<typename T> void _read(size_t num, T* value)
        {
            if (num==1)
            {
                _input >> *value;
            }
            else
            {
                for(; num>0; --num, ++value)
                {
                    _input >> *value;
                }
            }
        }

        // read value(s)
        virtual void read(size_t num, int8_t* value) override   { _read(num, value); }
        virtual void read(size_t num, uint8_t* value) override  { _read(num, value); }
        virtual void read(size_t num, int16_t* value) override  { _read(num, value); }
        virtual void read(size_t num, uint16_t* value) override { _read(num, value); }
        virtual void read(size_t num, int32_t* value) override  { _read(num, value); }
        virtual void read(size_t num, uint32_t* value) override { _read(num, value); }
        virtual void read(size_t num, int64_t* value) override  { _read(num, value); }
        virtual void read(size_t num, uint64_t* value) override { _read(num, value); }
        virtual void read(size_t num, float* value) override    { _read(num, value); }
        virtual void read(size_t num, double* value) override   { _read(num, value); }

        // read in an individual string
        void _read(std::string& value)
        {
            char c;
            _input >> c;
            if (_input.good())
            {
                if (c=='"')
                {
                    _input.get(c);
                    while( _input.good())
                    {
                        if (c=='\\')
                        {
                            _input.get(c);
                            if (c=='"') value.push_back(c);
                            else
                            {
                                value.push_back('\\');
                                value.push_back(c);
                            }
                        }
                        else if (c!='"')
                        {
                            value.push_back(c);
                        }
                        else
                        {
                            break;
                        }
                        _input.get(c);
                    }
                }
                else
                {
                    _input>>value;
                }
            }
        }

        // read one or more strings
        void read(size_t num, std::string* value) override
        {
            if (num==1)
            {
                _read(*value);
            }
            else
            {
                for(;num>0;--num, ++value)
                {
                    _read(*value);
                }
            }
        }


        // read object
        vsg::ref_ptr<vsg::Object> read() override
        {
            auto result = objectID();
            if (result)
            {
                ObjectID id = result.value();
                //std::cout<<"   matched result="<<id<<std::endl;

                if (auto itr = _objectIDMap.find(id); itr != _objectIDMap.end())
                {
                    //std::cout<<"Returning existing object "<<itr->second.get()<<std::endl;
                    return itr->second;
                }
                else
                {
                    std::string className;
                    _input >> className;

                    //std::cout<<"Loading new object "<<className<<std::endl;

                    vsg::ref_ptr<vsg::Object> object = _objectFactory->create(className.c_str());

                    if (object)
                    {
                        matchPropertyName("{");

                        object->read(*this);

                        //std::cout<<"Loaded object, assigning to _objectIDMap."<<object.get()<<std::endl;

                        matchPropertyName("}");

                        _objectIDMap[id] = object;

                        return object;
                    }
                    else
                    {
                        std::cout<<"Could not find means to create object"<<std::endl;
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

        std::string                         _readPropertyName;
        ObjectIDMap                         _objectIDMap;
        vsg::ref_ptr<vsg::ObjectFactory>    _objectFactory;
};

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
        object->setValue("my vector", vsg::vec4{1.1, 2.2, 3.3, 4.4});

        object->setObject("my array", new vsg::floatArray{10.1, 21.2, 31.4, 55.0});
        object->setObject("my vec3Array", new vsg::vec3Array{
            {10.1, 21.2, 31.4},
            {55.0, 45.0, -20.0}
        });

        vsg::ref_ptr<vsg::vec4Array2D> image( new vsg::vec4Array2D(3, 3) );

        for(size_t i=0; i<image->width(); ++i)
        {
            for(size_t j=0; j<image->height(); ++j)
            {
                image->at(i, j) = vsg::vec4(i, j, i*j, 1.0);
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
            output.writeObject("Root", object);
        }
        else
        {
            // write to specified file
            std::ofstream fout(outputFilename);
            AsciiOutput output(fout);
            output.writeObject("Root", object);
        }
    }

    return 0;
}
