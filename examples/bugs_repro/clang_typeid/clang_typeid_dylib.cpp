#include "clang_typeid_dylib.h"

vsg::ref_ptr<vsg::Object> create_object_in_dylib()
{
    auto factory = vsg::ObjectFactory::instance();
    return factory->create("vsg::MatrixTransform");
}
