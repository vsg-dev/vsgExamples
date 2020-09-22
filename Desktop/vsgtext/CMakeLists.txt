set(SOURCES
    TextLayout.cpp
    Text.cpp
    vsgtext.cpp
)

add_executable(vsgtext ${SOURCES})

target_link_libraries(vsgtext vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgtext PRIVATE USE_VSGXCHANGE)
    target_link_libraries(vsgtext vsgXchange::vsgXchange)
endif()
