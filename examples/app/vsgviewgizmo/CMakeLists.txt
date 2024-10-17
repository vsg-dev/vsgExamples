set(SOURCES
    vsgviewgizmo.cpp
)

add_executable(vsgviewgizmo ${SOURCES})

target_link_libraries(vsgviewgizmo vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgviewgizmo PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgviewgizmo vsgXchange::vsgXchange)
endif()

install(TARGETS vsgviewgizmo RUNTIME DESTINATION bin)
