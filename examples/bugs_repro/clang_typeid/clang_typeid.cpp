#include <cstdlib>
#include <iostream>
#include "clang_typeid_dylib.h"

int main(int argc, char** argv)
{
    auto local = vsg::MatrixTransform::create();
    auto dylib = create_object_in_dylib();

    std::cout << "Local: type name: " << local->type_info().name() << ", type hash: " << local->type_info().hash_code() << std::endl;
    std::cout << "Dylib: type name: " << dylib->type_info().name() << ", type hash: " << dylib->type_info().hash_code() << std::endl;

    std::cout << (dylib->is_compatible(local->type_info()) ? "types are compatible" : "types are not compatible") << std::endl;

    return EXIT_SUCCESS;
}
