#include <vsg/core/Visitor.h>
#include <vsg/nodes/Group.h>

class CustomGroupNode;
class CustomLODNode;

class CustomVisitorBase : public vsg::Inherit<vsg::Visitor, CustomVisitorBase>
{
public:
    inline bool handleCustomGroups(vsg::Group& group)
    {
        if (auto cgn = group.cast<CustomGroupNode>())
        {
            apply(*cgn);
            return true;
        }
        else if (auto cln = group.cast<CustomLODNode>())
        {
            apply(*cln);
            return true;
        }
        return false;
    }

    void apply(vsg::Group& group) override
    {
        handleCustomGroups(group);
    }

    virtual void apply(CustomGroupNode& /*node*/) {}
    virtual void apply(CustomLODNode& /*node*/) {}
};

class CustomGroupNode : public vsg::Inherit<vsg::Group, CustomGroupNode>
{
public:
    std::string name = "car";

protected:
    ~CustomGroupNode() = default;
};

class CustomLODNode : public vsg::Inherit<vsg::Group, CustomLODNode>
{
public:
    double maxDistance = 1.0;

protected:
    ~CustomLODNode() = default;
};

class VisitCustomTypes : public CustomVisitorBase
{
public:
    void apply(vsg::Group& group) override
    {
        if (handleCustomGroups(group)) return;

        std::cout << "apply(Group& node)" << std::endl;
        group.traverse(*this);
    }

    void apply(CustomGroupNode& node) override
    {
        std::cout << "apply(CustomGroupNode& node) name = " << node.name << std::endl;
        node.traverse(*this);
    }

    void apply(CustomLODNode& node) override
    {
        std::cout << "apply(CustomLODNode& node) maxDistance = " << node.maxDistance << std::endl;
        node.traverse(*this);
    }
};
