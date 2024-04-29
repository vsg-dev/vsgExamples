/*
 * shows how to setup visitors that can visit custom types
 * https://groups.google.com/g/vsg-users/c/YpSZwrKlcKI
 * https://groups.google.com/g/vsg-users/c/dpazkzRLO_8/m/TsqQWnN6AQAJ
 */

#include <iostream>

#include "AlternateVisitorCustomType.h"
#include "VisitorCustomType.h"

int main(int, char**)
{
    // Approach 1
    {
        std::cout << "First approach to implementing custom types and custom visitors that support these custom types." << std::endl;

        auto group = vsg::Group::create();

        auto child1 = CustomGroupNode::create();
        auto child2 = CustomLODNode::create();

        group->addChild(child1);
        group->addChild(child2);

        VisitCustomTypes v;
        group->accept(v);
    }

    // Approach 2
    {
        std::cout << "\nSecond/Alternate approach to implementing custom types and custom visitors that support these custom types." << std::endl;

        auto group = vsg::Group::create();

        auto child1 = AlternateCustomGroupNode::create();
        auto child2 = AlternateCustomLODNode::create();

        group->addChild(child1);
        group->addChild(child2);

        AlternateVisitCustomTypes v;
        group->accept(v);
    }

    return 0;
}
