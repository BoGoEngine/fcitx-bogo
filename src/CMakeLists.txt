include_directories (
${FCITX4_FCITX_INCLUDE_DIRS}
${FCITX4_FCITX_CONFIG_INCLUDE_DIRS}
${FCITX4_FCITX_UTILS_INCLUDE_DIRS}
${PROJECT_BINARY_DIR}
${LIBICONV_INCLUDE_DIR}
${PYTHON_INCLUDE_DIRS}
${X11_INCLUDE_DIR}
)

set(fcitx_bogo_sources
    bogo-im.c
    python3compat.h
    )

add_definitions( -DLOCALEDIR=\"${CMAKE_INSTALL_PREFIX}/share/locale\" )

fcitx_add_addon_full(bogo
   SOURCES ${fcitx_bogo_sources}
   DESC
   IM_CONFIG bogo.conf
   )

target_link_libraries(fcitx-bogo ${PYTHON_LIBRARIES})
target_link_libraries(fcitx-bogo ${X11_LIBRARIES})
