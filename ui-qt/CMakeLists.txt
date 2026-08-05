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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Core5Compat)

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
    PreferencesDialog.cpp
    PreferencesDialog.h
    AboutDialog.cpp
    AboutDialog.h
    MessagePane.cpp
    MessagePane.h
    MainWindow.cpp
    MainWindow.h)

target_link_libraries(vinatext-qt PRIVATE
    VinaText::core
    Scintilla::EditBase
    Lexilla::lexilla
    Qt6::Widgets)

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

set_target_properties(vinatext-qt PROPERTIES
    MACOSX_BUNDLE FALSE          # a plain binary is easier to run from a terminal
    WIN32_EXECUTABLE FALSE)
