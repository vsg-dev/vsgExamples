set(SOURCES
    vsgmultiviews.cpp
)

add_executable(vsgmultiviews ${SOURCES})
set_target_properties(vsgmultiviews PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgmultiviews vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgmultiviews PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgmultiviews vsgXchange::vsgXchange)
endif()

install(TARGETS vsgmultiviews RUNTIME DESTINATION bin)
