set(SRC_NETWORK
  SFML/src/SFML/Network/Http.cpp
  SFML/src/SFML/Network/IpAddress.cpp
  SFML/src/SFML/Network/Packet.cpp
  SFML/src/SFML/Network/Socket.cpp
  SFML/src/SFML/Network/SocketSelector.cpp
  SFML/src/SFML/Network/TcpListener.cpp
  SFML/src/SFML/Network/TcpSocket.cpp
  SFML/src/SFML/Network/UdpSocket.cpp
)

if(WIN32)
  list(APPEND SRC_NETWORK SFML/src/SFML/Network/Win32/SocketImpl.cpp)
else()
  list(APPEND SRC_NETWORK SFML/src/SFML/Network/Unix/SocketImpl.cpp)
endif()

set(SRC_SYSTEM
  SFML/src/SFML/System/Err.cpp
  SFML/include/SFML/System/String.hpp
  SFML/src/SFML/System/String.cpp
  SFML/src/SFML/System/Utils.cpp
)

add_library(sfml-network STATIC ${SRC_NETWORK})
add_library(sfml-system STATIC ${SRC_SYSTEM})
target_compile_features(sfml-network PUBLIC cxx_std_20)
target_compile_features(sfml-system PUBLIC cxx_std_20)
target_compile_definitions(sfml-system PUBLIC SFML_STATIC)
target_include_directories(sfml-system PUBLIC SFML/include PRIVATE SFML/src)
target_include_directories(sfml-network PUBLIC SFML/include PRIVATE SFML/src)

if(LIBRETRO AND SWITCH2)
  set(sfml_config "${CMAKE_CURRENT_SOURCE_DIR}/SFML/include/SFML/Config.hpp")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${sfml_config}")
  file(READ "${sfml_config}" sfml_config_contents)

  set(sfml_platform_branch "#elif defined(__unix__)")
  string(FIND "${sfml_config_contents}" "${sfml_platform_branch}" sfml_platform_offset)
  if(sfml_platform_offset EQUAL -1)
    message(FATAL_ERROR "SFML Config.hpp platform detection has changed")
  endif()

  set(sfml_switch_platform [=[
#elif defined(__LIBRETRO__) && defined(__SWITCH2__)

#define SFML_SYSTEM_SWITCH

#elif defined(__unix__)
]=])
  string(STRIP "${sfml_switch_platform}" sfml_switch_platform)
  string(REPLACE "${sfml_platform_branch}" "${sfml_switch_platform}"
    sfml_config_contents "${sfml_config_contents}")

  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/SFML")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/include/SFML/Config.hpp" "${sfml_config_contents}")

  target_include_directories(sfml-system BEFORE PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include")
  target_include_directories(sfml-network BEFORE PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include")
endif()

target_link_libraries(sfml-network PUBLIC sfml-system)
dolphin_disable_warnings(sfml-network)
dolphin_disable_warnings(sfml-system)
