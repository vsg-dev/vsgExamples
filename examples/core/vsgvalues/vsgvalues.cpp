#include <vsg/core/Auxiliary.h>
#include <vsg/core/ConstVisitor.h>
#include <vsg/core/Object.h>
#include <vsg/core/Value.h>
#include <vsg/core/Visitor.h>
#include <vsg/io/stream.h>

#include <algorithm>
#include <iostream>
#include <typeinfo>

namespace engine
{
    // provide a custom struct for storing data, such as for passing to the GPU as a uniform value
    struct property
    {
        float speed = 0.0f;
    };

    // declare a Value object for the property struct
    using propertyValue = vsg::Value<property>;
} // namespace engine
// provide a vsg::type_name<> for our custom propertyValue class using EVSG_type_name() macro, note must be done in global scope.
EVSG_type_name(engine::propertyValue)

    int main(int /*argc*/, char** /*argv*/)
{
    vsg::ref_ptr<vsg::Object> object(new vsg::Object());
    object->setValue("name", "Name field contents");
    object->setValue("time", 10.0);
    object->setValue("size", 3.1f);
    object->setValue("count", 5);
    object->setValue("pos", 4u);

    if (object->getAuxiliary())
    {
        struct VisitValues : public vsg::Visitor
        {
            using Visitor::apply;

            void apply(vsg::Object& object) override
            {
                std::cout << "Object, " << typeid(object).name() << std::endl;
            }

            void apply(vsg::intValue& value) override
            {
                std::cout << "intValue,  value = " << value << std::endl;
            }

            void apply(vsg::uintValue& value) override
            {
                std::cout << "uintValue,  value = " << value << std::endl;
            }

            void apply(vsg::floatValue& value) override
            {
                std::cout << "floatValue, value  = " << value << std::endl;
            }

            void apply(vsg::doubleValue& value) override
            {
                std::cout << "doubleValue, value  = " << value << std::endl;
            }

            void apply(vsg::stringValue& value) override
            {
                std::cout << "stringValue, value  = " << value.value() << std::endl;
            }
        };

        VisitValues visitValues;

        auto& userObjects = object->getAuxiliary()->userObjects;

        std::cout << "Object has Auxiliary so check its ObjectMap for our values. " << object->getAuxiliary() << std::endl;
        for (auto& [key, user_object] : userObjects)
        {
            std::cout << "   key[" << key << "] ";
            user_object->accept(visitValues);
        }

        std::cout << "\nUse for_each " << std::endl;
        std::for_each(userObjects.begin(), userObjects.end(), [&visitValues](vsg::Auxiliary::ObjectMap::value_type& key_value) {
            std::cout << "   key[" << key_value.first << "] ";
            key_value.second->accept(visitValues);
        });
    }

    // create a propertyValue object wrapper around an engine::property struct.
    auto my_property = engine::propertyValue::create(engine::property{10.0});
    std::cout << "\nmy_property = " << my_property << ", className = " << my_property->className() << std::endl;
    std::cout << "    after constructor my_property->value->speed = " << my_property->value().speed << std::endl;

    // we can modify the value struct by getting the value from the value object and modifying it:
    my_property->value().speed += 2.0f;
    std::cout << "    after increment my_property->value->speed = " << my_property->value().speed << std::endl;

    // or get a reference to underlying engine::property value held by the my_property object
    auto& prop = my_property->value();
    prop.speed *= 2.0f; // modifying a member of the struct
    std::cout << "    after multiplication my_property->value->speed = " << prop.speed << std::endl;

    return 0;
}
