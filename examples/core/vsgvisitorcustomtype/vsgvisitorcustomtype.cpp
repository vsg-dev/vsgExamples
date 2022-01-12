#include <vsg/all.h>

/*
 * shows how to setup visitors that can visit custom types
 * https://groups.google.com/g/vsg-users/c/YpSZwrKlcKI
 * https://groups.google.com/g/vsg-users/c/dpazkzRLO_8/m/TsqQWnN6AQAJ
 */

class CustomGroupNode;
class CustomLODNode;

class CustomVisitorBase : public vsg::Visitor
{
public:

    virtual void apply(CustomGroupNode& node) {}
    virtual void apply(CustomLODNode& node) {}
};

class CustomGroupNode : public vsg::Inherit<vsg::Group, CustomGroupNode>
{
    public:

        void accept(vsg::Visitor& visitor) override
        {
            if (auto v = dynamic_cast<CustomVisitorBase*>(&visitor); v != nullptr)
            {
                v->apply(*this);
            }
            else
            {
                visitor.apply(*this);
            }
        }

    protected:

        ~CustomGroupNode() = default;
};

class CustomLODNode : public vsg::Inherit<vsg::Group, CustomLODNode>
{
    public:

        void accept(vsg::Visitor& visitor) override
        {
            if (auto v = dynamic_cast<CustomVisitorBase*>(&visitor); v != nullptr)
            {
                v->apply(*this);
            }
            else
            {
                visitor.apply(*this);
            }
        }

    protected:

        ~CustomLODNode() = default;
};

class VisitCustomTypes : public CustomVisitorBase
{
    public:

        virtual void apply(CustomGroupNode& node)
        {
            std::cout << "apply(CustomGroupNode& node)" << std::endl;
        }

        virtual void apply(CustomLODNode& node)
        {
            std::cout << "apply(CustomLODNode& node)" << std::endl;
        }
};

int main(int argc, char* argv[])
{
    auto group = vsg::Group::create();

    auto child1 = CustomGroupNode::create();
    auto child2 = CustomLODNode::create();

    group->addChild(child1);
    group->addChild(child2);

    VisitCustomTypes v;
    group->traverse(v);
}