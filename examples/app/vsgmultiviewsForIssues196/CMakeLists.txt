set(SOURCES
    vsgmultiviewsForIssues196.cpp
)

add_executable(vsgmultiviewsForIssues196 ${SOURCES})

target_link_libraries(vsgmultiviewsForIssues196 vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgmultiviewsForIssues196 PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgmultiviewsForIssues196 vsgXchange::vsgXchange)
endif()

install(TARGETS vsgmultiviewsForIssues196 RUNTIME DESTINATION bin)
