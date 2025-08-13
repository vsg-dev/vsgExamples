set(SOURCES
    vsgtext.cpp
)

add_executable(vsgtext ${SOURCES})
set_target_properties(vsgtext PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgtext vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgtext PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgtext vsgXchange::vsgXchange)
endif()

install(TARGETS vsgtext RUNTIME DESTINATION bin)
