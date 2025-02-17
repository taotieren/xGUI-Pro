XGUIPRO_INCLUDE_CONFIG_FILES_IF_EXISTS()

configure_file(assets/hvml.js.in ${xGUIPro_DERIVED_SOURCES_DIR}/webext/hvml.js @ONLY)
configure_file(assets/about.html.in ${xGUIPro_DERIVED_SOURCES_DIR}/webext/about.html @ONLY)

set(XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR "${XGUIPRO_WEB_EXTENSIONS_OUTPUT_DIR}/assets")
set(XGUIPRO_WEBEXT_ASSETS_INSTALL_DIR "${XGUIPRO_WEB_EXTENSIONS_INSTALL_DIR}/assets")

set(assets_SOURCES
    "${xGUIPro_DERIVED_SOURCES_DIR}/webext/hvml.js"
    "${xGUIPro_DERIVED_SOURCES_DIR}/webext/about.html"
    "${XGUIPRO_BIN_DIR}/webext/assets/hvml.png"
    "${XGUIPRO_BIN_DIR}/webext/assets/hvml-v.png"
    "${XGUIPRO_BIN_DIR}/webext/assets/hvml-bright.png"
    "${XGUIPRO_BIN_DIR}/webext/assets/hvml-dark.png"
    "${XGUIPRO_BIN_DIR}/webext/assets/hvml-v-black.png"
    "${XGUIPRO_BIN_DIR}/webext/assets/hvml-v-fill-white.png"
    "${XGUIPRO_BIN_DIR}/webext/assets/hvml-v-fill.svg"
    "${XGUIPRO_BIN_DIR}/webext/assets/hvml-v.svg"
    "${XGUIPRO_BIN_DIR}/webext/assets/workspace-layouter.css"
    "${XGUIPRO_BIN_DIR}/webext/assets/splash.jpg"
)

set(bootstrap_DIRECTORY
    "assets/bootstrap-5.1.3-dist"
)

set(assets_FILES
    "${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}/hvml.js"
    "${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}/about.html"
)

add_custom_command(
    OUTPUT ${assets_FILES}
    DEPENDS ${assets_SOURCES}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${assets_SOURCES} ${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}
    COMMENT "Copy assets..."
    VERBATIM)

add_custom_target(assets DEPENDS ${assets_FILES})
add_dependencies(WebExtensionHVML assets)

file(ARCHIVE_EXTRACT
        INPUT "${XGUIPRO_BIN_DIR}/webext/assets/bootstrap-5.1.3-dist.zip"
        DESTINATION "${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}")

file(ARCHIVE_EXTRACT
        INPUT "${XGUIPRO_BIN_DIR}/webext/assets/bootstrap-icons-1.8.3.zip"
        DESTINATION "${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}")

install(FILES ${assets_SOURCES}
        DESTINATION ${XGUIPRO_WEBEXT_ASSETS_INSTALL_DIR})

install(DIRECTORY "${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}/bootstrap-5.1.3-dist"
        DESTINATION ${XGUIPRO_WEBEXT_ASSETS_INSTALL_DIR})

install(DIRECTORY "${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}/bootstrap-icons-1.8.3"
        DESTINATION ${XGUIPRO_WEBEXT_ASSETS_INSTALL_DIR})

