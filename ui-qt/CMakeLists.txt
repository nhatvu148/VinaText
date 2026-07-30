#*******************************************************************************
# ui-qt — the Qt frontend.
#
# Today this is a Phase 3 SPIKE, not the frontend: one window, one editor, no
# docks, no tabs, no dialogs. Its job is to prove D2 - that upstream Scintilla's
# qt/ScintillaEditBase works for this project off Windows - which had never been
# tested.
#
# D3: Qt MUST be linked dynamically. Never add a static Qt build here.
#*******************************************************************************

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Core5Compat)

qt_standard_project_setup()

add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty ${CMAKE_BINARY_DIR}/thirdparty)

qt_add_executable(vinatext-qt main.cpp)

target_link_libraries(vinatext-qt PRIVATE
    VinaText::core
    Scintilla::EditBase
    Lexilla::lexilla
    Qt6::Widgets)

# The spike runs from the build tree, where the MFC app's
# PathUtils::GetVinaTextPackagePath() convention does not apply. Point it at the
# JSON in the source tree; --data overrides.
target_compile_definitions(vinatext-qt PRIVATE
    VINATEXT_DATA_DIR="${CMAKE_SOURCE_DIR}/Packages/data-packages")

set_target_properties(vinatext-qt PROPERTIES
    MACOSX_BUNDLE FALSE          # a plain binary is easier to run from a terminal
    WIN32_EXECUTABLE FALSE)
