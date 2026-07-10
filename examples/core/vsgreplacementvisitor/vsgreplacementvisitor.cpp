#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <chrono>
#include <functional>
#include <iostream>
#include <vector>

class MyGroup : public vsg::Inherit<vsg::Node, MyGroup>
{
public:
    using Children = std::unordered_map<std::string, vsg::ref_ptr<vsg::Node>>;
    Children namedChildren;

    MyGroup(const vsg::Group& group) :
        Inherit(group),
        namedChildren()
    {
        for (size_t i = 0; i < group.children.size(); ++i)
        {
            std::string name;
            if (group.children[i]->getValue("name", name))
            {
                namedChildren[name] = group.children[i];
            }
            else
            {
                namedChildren[std::to_string(i)] = group.children[i];
            }
        }
    }

    template<class N, class V>
    static void t_traverse(N& node, V& visitor)
    {
        for (auto& [name, child] : node.namedChildren) child->accept(visitor);
    }

    void traverse(vsg::Visitor& visitor) override { t_traverse(*this, visitor); }
    void traverse(vsg::ConstVisitor& visitor) const override { t_traverse(*this, visitor); }
    void traverse(vsg::RecordTraversal& visitor) const override { t_traverse(*this, visitor); }
};

class MyStateGroup : public vsg::Inherit<MyGroup, MyStateGroup>
{
public:
    vsg::StateCommands stateCommands;

    vsg::ref_ptr<vsg::ArrayState> prototypeArrayState;

    MyStateGroup(const vsg::StateGroup& stateGroup) :
        Inherit(stateGroup),
        stateCommands(stateGroup.stateCommands),
        prototypeArrayState(stateGroup.prototypeArrayState)
    {
    }

    template<class N, class V>
    static void t_traverse(N& node, V& visitor)
    {
        for (auto& stateCommand : node.stateCommands) stateCommand->accept(visitor);
        Inherit::t_traverse(node, visitor);
    }

    void traverse(vsg::Visitor& visitor) override { t_traverse(*this, visitor); }
    void traverse(vsg::ConstVisitor& visitor) const override { t_traverse(*this, visitor); }
    void traverse(vsg::RecordTraversal& visitor) const override
    {
        visitor.state->push(stateCommands);
        Inherit::t_traverse(*this, visitor);
        visitor.state->pop(stateCommands);
    }
};

class MyGroupVisitor : public vsg::ReplacementVisitor
{
public:
    using ReplacementVisitor::apply;

    // Sometimes the same node will be accessible via multiple node paths.
    // Typically, it's beneficial to return the same replacement every time it's encountered.
    // Use ref_ptr so the original doesn't go out of scope and have its address reused by an unrelated node.
    // Don't use a map like this if you specifically don't want to return the same replacement, e.g. if replacing every reference to a node with a unique copy that can be safely edited.
    std::map<vsg::ref_ptr<Object>, std::optional<vsg::ref_ptr<Object>>> replacementMap;

    bool getExistingReplacement(vsg::Object& object, std::optional<vsg::ref_ptr<vsg::Object>>& replacement)
    {
        auto itr = replacementMap.find(vsg::ref_ptr(&object));
        if (itr == replacementMap.end())
            return false;
        replacement = itr->second;
        return true;
    }

    void saveReplacement(vsg::Object& object, std::optional<vsg::ref_ptr<vsg::Object>>& replacement)
    {
        replacementMap.emplace(&object, replacement);
    }

    std::optional<vsg::ref_ptr<vsg::Object>> apply(vsg::Object& object) override
    {
        object.traverse(*this);
        return std::nullopt;
    }

    std::optional<vsg::ref_ptr<vsg::Object>> apply(vsg::Group& group) override
    {
        std::optional<vsg::ref_ptr<vsg::Object>> replacement;
        if (getExistingReplacement(group, replacement))
            return replacement;

        group.traverse(*this);

        replacement = MyGroup::create(group);
        saveReplacement(group, replacement);
        return replacement;
    }

    std::optional<vsg::ref_ptr<vsg::Object>> apply(vsg::StateGroup& group) override
    {
        std::optional<vsg::ref_ptr<vsg::Object>> replacement;
        if (getExistingReplacement(group, replacement))
            return replacement;

        group.traverse(*this);

        replacement = MyStateGroup::create(group);
        saveReplacement(group, replacement);
        return replacement;
    }

    // don't replace special-purpose classes our custom class can't do the job of
    std::optional<vsg::ref_ptr<vsg::Object>> apply(vsg::Transform& transform) override
    {
        return apply(static_cast<vsg::Node&>(transform));
    }
};

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create(arguments);

    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->sharedObjects = vsg::SharedObjects::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    options->readOptions(arguments);
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    vsg::ref_ptr<vsg::Node> scene;

    if (argc > 1)
    {
        vsg::Path filename = argv[1];
        auto model = vsg::read_cast<vsg::Node>(filename, options);
        if (!model)
        {
            std::cout << "Failed to load " << filename << std::endl;
            return 1;
        }

        scene = model;
    }

    MyGroupVisitor visitor;
    scene->accept(visitor);

    return 0;
}
