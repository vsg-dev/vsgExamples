#include "TextGroup.h"

using namespace vsg;

int TextGroup::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_pointer_container(children, rhs.children);
}

void TextGroup::read(Input& input)
{
    Node::read(input);

    input.readObjects("children", children);
}

void TextGroup::write(Output& output) const
{
    Node::write(output);

    output.writeObjects("children", children);
}

void TextGroup::setup(uint32_t minimumAllocation)
{
    info("TextGroup::setup( ", minimumAllocation," ) ", children.size());
    for(auto& child : children)
    {
        child->setup(minimumAllocation);
    }
}
