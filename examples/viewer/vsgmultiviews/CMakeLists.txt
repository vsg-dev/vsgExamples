set(SOURCES
    vsgmultiviews.cpp
)

add_executable(vsgmultiviews ${SOURCES})

target_link_libraries(vsgmultiviews vsgImGui::vsgImGui vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgmultiviews PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgmultiviews vsgXchange::vsgXchange)
endif()
