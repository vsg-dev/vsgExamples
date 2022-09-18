#include <vsg/all.h>

namespace vsg
{
    class TextGroup : public vsg::Inherit<vsg::Node, TextGroup>
    {
    public:

        template<class N, class V>
        static void t_traverse(N& node, V& visitor)
        {
            for (auto& child : node.children) child->accept(visitor);
        }

        void traverse(Visitor& visitor) override { t_traverse(*this, visitor); }
        void traverse(ConstVisitor& visitor) const override { t_traverse(*this, visitor); }
        void traverse(RecordTraversal& visitor) const override { t_traverse(*this, visitor); }

        int compare(const Object& rhs) const override;

        void read(Input& input) override;
        void write(Output& output) const override;

        using Children = std::vector<ref_ptr<Text>, allocator_affinity_nodes<ref_ptr<Text>>>;
        Children children;

        void addChild(ref_ptr<Text> text) { children.push_back(text); }

        /// create the rendering backend.
        /// minimumAllocation provides a hint for the minimum number of glyphs to allocate space for.
        virtual void setup(uint32_t minimumAllocation = 0);
    };
    VSG_type_name(vsg::TextGroup);
}
