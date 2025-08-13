set(SOURCES
    vsgwin32.cpp
)

add_executable(vsgwin32 WIN32 ${SOURCES})
set_target_properties(vsgwin32 PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgwin32 vsg::vsg)

install(TARGETS vsgwin32 RUNTIME DESTINATION bin)
