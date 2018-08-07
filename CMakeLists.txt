cmake_minimum_required(VERSION 2.8)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

find_package(PkgConfig)
pkg_check_modules(LIB_WEBSOCKETS REQUIRED libwebsockets)
pkg_check_modules(LIB_OPENSSL REQUIRED openssl)
pkg_check_modules(LIB_JSON REQUIRED json-c)

get_filename_component(
  LIB_WEBSOCKETS_INSTALL_DIR
  ${LIB_WEBSOCKETS_LIBRARY_DIRS}
  DIRECTORY
)

add_executable(
    client
      ws-client.c
      ws-protocol.c
)

target_link_libraries(
    client
      ${LIB_WEBSOCKETS_LIBRARIES}
	  ${LIB_JSON_LIBRARIES}
)

set_target_properties(
    client
    PROPERTIES
      INCLUDE_DIRECTORIES
        ${LIB_WEBSOCKETS_INCLUDE_DIRS}
      LINK_FLAGS
        "-L${LIB_WEBSOCKETS_LIBRARY_DIRS}"
      COMPILE_DEFINITIONS
        INSTALL_DATADIR="${LIB_WEBSOCKETS_INSTALL_DIR}/share"
)

set_target_properties(
    client
    PROPERTIES
      INCLUDE_DIRECTORIES
        ${LIB_OPENSSL_INCLUDE_DIRS}
      LINK_FLAGS
        "-L${LIB_OPENSSL_LIBRARY_DIRS}"
      COMPILE_DEFINITIONS
        INSTALL_DATADIR="${LIB_OPENSSL_INSTALL_DIR}/share"
)