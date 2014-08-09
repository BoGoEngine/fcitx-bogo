cmake_minimum_required(VERSION 2.6)

project(fcitx-bogo)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

set(CMAKE_C_FLAGS "-Wall -Wextra -Wno-sign-compare -Wno-unused-parameter -fvisibility=hidden ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-sign-compare -Wno-unused-parameter -fvisibility=hidden ${CMAKE_CXX_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--as-needed ${CMAKE_SHARED_LINKER_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS "-Wl,--as-needed ${CMAKE_MODULE_LINKER_FLAGS}")

set(CMAKE_C_FLAGS_DEBUG "-DDEBUG")

find_package(Fcitx 4.2.8 REQUIRED)
find_package(Libiconv REQUIRED)
find_package(PythonLibs 2.7 REQUIRED)
find_package(X11 REQUIRED)

set(DATA_INSTALL_PATH ${CMAKE_INSTALL_PREFIX}/share/fcitx-bogo)

configure_file(config.h.in config.h)

install(DIRECTORY bogo-python/bogo DESTINATION ${DATA_INSTALL_PATH})

add_subdirectory(src)
add_subdirectory(data)
