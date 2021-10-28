set(SOURCES vsgmemory.cpp)

add_executable(vsgmemory ${SOURCES})

target_link_libraries(vsgmemory vsg::vsg)

install(TARGETS vsgmemory RUNTIME DESTINATION bin)
