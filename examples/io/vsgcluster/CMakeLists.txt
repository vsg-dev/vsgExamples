set(SOURCES
    Broadcaster.cpp
    Receiver.cpp
    Packet.cpp
    vsgcluster.cpp
)

add_executable(vsgcluster ${SOURCES})

target_link_libraries(vsgcluster vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgcluster PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgcluster vsgXchange::vsgXchange)
endif()

if (WIN32)
   target_link_libraries(vsgcluster ws2_32)
else()
   check_library_exists("nsl" "gethostbyname" "" LIB_NSL_HAS_GETHOSTBYNAME)
   if(LIB_NSL_HAS_GETHOSTBYNAME)
      target_link_libraries(vsgcluster nsl)
   endif()

   check_library_exists("socket" "socket" "" LIB_SOCKET_HAS_SOCKET)
   if(LIB_SOCKET_HAS_SOCKET)
      target_link_libraries(vsgcluster socket)
   endif()
endif()

