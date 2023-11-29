set(HEADERS Annotation.h)
set(SOURCES vsgannotation.cpp)

add_executable(vsgannotation ${HEADERS} ${SOURCES})
target_link_libraries(vsgannotation vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgannotation PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgannotation vsgXchange::vsgXchange)
endif()

install(TARGETS vsgannotation RUNTIME DESTINATION bin)
