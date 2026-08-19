#*******************************************************************************
# ui-qt — the Qt frontend.
#
# The Phase 3 alpha shell. It was a spike proving D2 (upstream Scintilla's
# qt/ScintillaEditBase works off Windows); under D9 it is now the shell, fixed to
# that decision's checklist: tabs, open/save, lexer + both themes, find, status
# bar. No dialogs, no docking, no settings UI.
#
# D3: Qt MUST be linked dynamically. Never add a static Qt build here.
#*******************************************************************************

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Core5Compat)

qt_standard_project_setup()

add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty ${CMAKE_BINARY_DIR}/thirdparty)

qt_add_executable(vinatext-qt
    main.cpp
    EditorData.cpp
    EditorData.h
    EditorWidget.cpp
    TagMatcher.cpp
    TagMatcher.h
    EditorWidget.h
    FindBar.cpp
    FindBar.h
    GotoBar.cpp
    GotoBar.h
    EncodingDialog.cpp
    EncodingDialog.h
    WindowListDialog.cpp
    WindowListDialog.h
    BookmarkPane.cpp
    BookmarkPane.h
    TransformDialog.cpp
    TransformDialog.h
    LineTransforms.cpp
    LineTransforms.h
    SingleInstance.cpp
    SingleInstance.h
    RegexPresets.cpp
    RegexPresets.h
    ResourcePaths.cpp
    ResourcePaths.h
    PreferencesDialog.cpp
    PreferencesDialog.h
    AboutDialog.cpp
    AboutDialog.h
    MessagePane.cpp
    MessagePane.h
    MainWindow.cpp
    MainWindow.h
    MacAppearance.h)

# The title bar is the one surface a QPalette cannot reach, and only macOS has a
# knob for it. The .mm is Objective-C++ and must not be fed to the compiler
# anywhere else; the stub keeps the call site free of #ifdef.
if(APPLE)
    target_sources(vinatext-qt PRIVATE MacAppearance.mm)
    target_link_libraries(vinatext-qt PRIVATE "-framework AppKit")
else()
    target_sources(vinatext-qt PRIVATE MacAppearanceStub.cpp)
endif()

# EXPLICIT, not via AUTORCC. Listing a .qrc among the sources compiles it only
# when AUTORCC is on, and qt_standard_project_setup() does not turn it on - the
# build then succeeds, the resource is absent, and QIcon(":/app.ico") is silently
# null. Measured: the icon self-test failed with "renders at 64px, got 0x0"
# against a build that had reported no error at all.
qt_add_resources(vinatext-qt "vinatext_assets"
    PREFIX "/"
    BASE "${CMAKE_SOURCE_DIR}/res"
    FILES "${CMAKE_SOURCE_DIR}/res/app.ico")

target_link_libraries(vinatext-qt PRIVATE
    VinaText::core
    Scintilla::EditBase
    Lexilla::lexilla
    Qt6::Widgets
    # QLocalServer/QLocalSocket, for the single-instance handoff. Qt Network is
    # LGPLv3 like the rest and adds no GPL-only module - see D3.
    Qt6::Network)

# ui-qt/ is new code and is held to the same wall core/ is: a warning here is a
# mistake, not a legacy artefact. The MFC tree is untouched by this.
if(MSVC)
    target_compile_options(vinatext-qt PRIVATE /W4 /WX)
else()
    target_compile_options(vinatext-qt PRIVATE -Wall -Wextra -Werror)
endif()

# Run from the build tree, where the MFC app's PathUtils::GetVinaTextPackagePath()
# convention does not apply. Point at the JSON in the source tree; --data overrides.
target_compile_definitions(vinatext-qt PRIVATE
    VINATEXT_DATA_DIR="${CMAKE_SOURCE_DIR}/Packages/data-packages"
    VINATEXT_VERSION="${PROJECT_VERSION}"
    VINATEXT_SCINTILLA_VERSION="${SCINTILLA_VERSION}"
    VINATEXT_LEXILLA_VERSION="${LEXILLA_VERSION}"
    VINATEXT_LICENSE_DIR="${CMAKE_SOURCE_DIR}/license")

# The bundle is OPT-IN, and the default stays a plain binary. Flipping
# MACOSX_BUNDLE on unconditionally would move the executable to
# vinatext-qt.app/Contents/MacOS/vinatext-qt and break every command in CI and in
# doc/PORTING.md that runs ./qtbuild/ui-qt/vinatext-qt - for a build nobody was
# asking to package. Packaging asks for it explicitly.
option(VINATEXT_MACOS_BUNDLE "Build ui-qt as a macOS .app bundle" OFF)

if(APPLE AND VINATEXT_MACOS_BUNDLE)
    # The icon, generated from the SAME res/app.ico the Windows build ships, so
    # there is one piece of artwork in the repository rather than two that drift.
    set(VINATEXT_ICNS "${CMAKE_CURRENT_BINARY_DIR}/VinaText.icns")
    add_custom_command(
        OUTPUT "${VINATEXT_ICNS}"
        COMMAND "${CMAKE_SOURCE_DIR}/tools/make_macos_icon.sh"
                "${CMAKE_SOURCE_DIR}/res/app.ico" "${VINATEXT_ICNS}"
        DEPENDS "${CMAKE_SOURCE_DIR}/res/app.ico"
                "${CMAKE_SOURCE_DIR}/tools/make_macos_icon.sh"
        COMMENT "Generating VinaText.icns from res/app.ico"
        VERBATIM)
    target_sources(vinatext-qt PRIVATE "${VINATEXT_ICNS}")
    set_source_files_properties("${VINATEXT_ICNS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")

    # The data and the licences travel INSIDE the bundle, at the first path
    # ResourcePaths::Candidates looks in. Without this the .app resolves nothing
    # and falls back to the builder's source tree - which is the failure 6u
    # exists to make visible, and it would look like success on this machine.
    # CONFIGURE_DEPENDS, so adding a data file re-globs on the next build rather
    # than waiting for someone to reconfigure. Measured without it: a new file
    # dropped into license/ built cleanly and simply was not in the bundle - a
    # SILENT omission, and for the licence glob below that is a D3 obligation
    # quietly going missing rather than a build failure anyone would notice.
    file(GLOB VINATEXT_DATA_FILES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/Packages/data-packages/*")
    foreach(DATA_FILE IN LISTS VINATEXT_DATA_FILES)
        target_sources(vinatext-qt PRIVATE "${DATA_FILE}")
        set_source_files_properties("${DATA_FILE}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/data"
            HEADER_FILE_ONLY TRUE)
    endforeach()

    # D3's obligation: the LGPL notices ship with the binary they describe. The
    # About box names license/License-Qt.txt, and a path true only in the build
    # tree makes that claim false in every copy handed to somebody else.
    file(GLOB VINATEXT_LICENSE_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/license/*")
    foreach(LICENSE_FILE IN LISTS VINATEXT_LICENSE_FILES)
        target_sources(vinatext-qt PRIVATE "${LICENSE_FILE}")
        set_source_files_properties("${LICENSE_FILE}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/license"
            HEADER_FILE_ONLY TRUE)
    endforeach()

    set_target_properties(vinatext-qt PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        # NOT MACOSX_BUNDLE_EXECUTABLE_NAME: it is not one of the properties
        # CMake substitutes into Info.plist.in, and the ${MACOSX_BUNDLE_EXECUTABLE_NAME}
        # there is filled from the target's real output name instead. Setting it
        # did nothing, and what it said - "vinatext-qt" - was the wrong name,
        # since OUTPUT_NAME below makes the binary VinaText. Verified with
        # plutil: CFBundleExecutable reads "VinaText", matching the file on disk.
        MACOSX_BUNDLE_BUNDLE_NAME "VinaText"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        OUTPUT_NAME "VinaText")
else()
    set_target_properties(vinatext-qt PROPERTIES
        MACOSX_BUNDLE FALSE      # a plain binary is easier to run from a terminal
        WIN32_EXECUTABLE FALSE)
endif()
