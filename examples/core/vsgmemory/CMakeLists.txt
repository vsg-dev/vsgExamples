set(SOURCES vsgmemory.cpp)

add_executable(vsgmemory ${SOURCES})
set_target_properties(vsgmemory PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(vsgmemory vsg::vsg)

install(TARGETS vsgmemory RUNTIME DESTINATION bin)
