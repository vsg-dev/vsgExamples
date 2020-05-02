set(SOURCES
    Builder.h
    Builder.cpp
    vsgintersection.cpp
)

add_executable(vsgintersection ${SOURCES})

target_link_libraries(vsgintersection vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgintersection PRIVATE USE_VSGXCHANGE)
    target_link_libraries(vsgintersection vsgXchange::vsgXchange)
endif()
