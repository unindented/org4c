# Build & install rules for org2html command line utility
if(BUILD_ORG2HTML_EXECUTABLE)
    add_executable(org2html cmdline.c cmdline.h org2html.c)
    target_link_libraries(org2html PRIVATE org4c-html)
    target_compile_definitions(org2html PRIVATE
        ORG_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        ORG_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        ORG_VERSION_RELEASE=${PROJECT_VERSION_PATCH}
    )

    # On macOS, the installed executable needs an rpath to locate the shared
    # libraries (installed into ${CMAKE_INSTALL_LIBDIR}) from the bin directory.
    # @loader_path resolves relative to the executable's location at runtime.
    if(APPLE AND BUILD_SHARED_LIBS)
        set_target_properties(org2html PROPERTIES
            INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}"
        )
    endif()

    install(
        TARGETS org2html
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(FILES "org2html.1" DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
endif()
