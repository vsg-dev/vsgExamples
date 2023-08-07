set(DYLIB_SOURCES clang_typeid_dylib.cpp clang_typeid_dylib.h)
add_library(clang_typeid_dylib SHARED ${DYLIB_SOURCES})
target_link_libraries(clang_typeid_dylib vsg::vsg)
install(TARGETS clang_typeid_dylib RUNTIME DESTINATION bin)

set(SOURCES clang_typeid.cpp)
add_executable(clang_typeid ${SOURCES})
target_link_libraries(clang_typeid vsg::vsg clang_typeid_dylib)
install(TARGETS clang_typeid RUNTIME DESTINATION bin)

