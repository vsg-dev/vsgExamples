set(SOURCES
    vsginput.cpp
)

add_executable(vsginput ${SOURCES})
set_target_properties(vsginput PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsginput vsg::vsg)

install(TARGETS vsginput RUNTIME DESTINATION bin)
