set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS 1)

# Handle absolute include and lib dirs outside of CMAKE_INSTALL_PREFIX
include(JoinPaths.cmake) # can be replaced by cmake_path(APPEND) in CMake 3.20
join_paths(PKGCONFIG_INCLUDEDIR "\${prefix}" "${CMAKE_INSTALL_INCLUDEDIR}")
join_paths(PKGCONFIG_LIBDIR "\${prefix}" "${CMAKE_INSTALL_LIBDIR}")

# Build & install rules for ORG4C parser library
configure_file(org4c.pc.in org4c.pc @ONLY)
add_library(org4c org4c.c org4c.h entity.c entity.h)
target_include_directories(org4c PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_definitions(org4c PRIVATE "$<$<CONFIG:Debug>:DEBUG>")
set_target_properties(org4c PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER org4c.h
)
install(
    TARGETS org4c
    EXPORT org4cConfig
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org4c.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# Build & install rules for HTML renderer library
if(BUILD_HTML_LIBRARY)
    configure_file(org4c-html.pc.in org4c-html.pc @ONLY)
    add_library(org4c-html org4c-html.c org4c-html.h)
    target_include_directories(org4c-html PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )
    set_target_properties(org4c-html PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER org4c-html.h
    )
    target_link_libraries(org4c-html PUBLIC org4c)
    install(
        TARGETS org4c-html
        EXPORT org4cConfig
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org4c-html.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

# Install rules
install(EXPORT org4cConfig DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/org4c/ NAMESPACE org4c::)
