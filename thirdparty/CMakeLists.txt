#*******************************************************************************
# Scintilla + Lexilla, fetched at configure time.
#
# Neither project ships CMake - the Qt bindings are qmake .pro files only - so
# these targets are translated from qt/ScintillaEditBase/ScintillaEditBase.pro
# and Lexilla's own source layout. See doc/QT-PORT-BRIEF.md D2.
#
# Fetched rather than vendored: committing both trees would add ~4 MB of
# third-party source to a repo D8 already calls too big, and a pinned URL+hash
# is reproducible without the repo growth. Swapping to vendored copies later
# means changing only this file.
#
# Licences (both permissive, both compatible with VinaText's MIT):
#   Scintilla  HPND   https://www.scintilla.org/License.txt
#   Lexilla    permissive, same family
#*******************************************************************************

include(FetchContent)

set(SCINTILLA_VERSION 5.6.4)
set(LEXILLA_VERSION   5.5.1)

FetchContent_Declare(scintilla
    URL      https://www.scintilla.org/scintilla564.tgz
    URL_HASH SHA256=c62e19449ed066867fde4edc6e3864f56d16f893c9f7582792fcf27dd733a48e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(lexilla
    URL      https://www.scintilla.org/lexilla551.tgz
    URL_HASH SHA256=0d04ee551915ad1bb79148287951cfc5c791d6565d5e32badd14dfb8293f4231
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_MakeAvailable(scintilla lexilla)

#-------------------------------------------------------------------------------
# Scintilla core + Qt platform layer  (from ScintillaEditBase.pro)
#-------------------------------------------------------------------------------

# The .pro lists these 33 explicitly rather than globbing, and the set is not the
# same as src/*.cxx - it excludes the lexer-era files that moved to Lexilla.
set(_sci_core
    AutoComplete CallTip CaseConvert CaseFolder CellBuffer ChangeHistory
    CharacterCategoryMap CharacterType CharClassify ContractionState DBCS
    Decoration Document EditModel Editor EditView Geometry Indicator KeyMap
    LineMarker MarginView PerLine PositionCache RESearch RunStyles ScintillaBase
    Selection Style UndoHistory UniConversion UniqueString ViewStyle XPM)
list(TRANSFORM _sci_core PREPEND "${scintilla_SOURCE_DIR}/src/")
list(TRANSFORM _sci_core APPEND ".cxx")

add_library(scintilla_editbase STATIC
    ${_sci_core}
    ${scintilla_SOURCE_DIR}/qt/ScintillaEditBase/PlatQt.cpp
    ${scintilla_SOURCE_DIR}/qt/ScintillaEditBase/ScintillaQt.cpp
    ${scintilla_SOURCE_DIR}/qt/ScintillaEditBase/ScintillaEditBase.cpp)

add_library(Scintilla::EditBase ALIAS scintilla_editbase)

# ScintillaEditBase.cpp and ScintillaQt.cpp declare Q_OBJECT.
set_target_properties(scintilla_editbase PROPERTIES AUTOMOC ON)

# src/ has to be PUBLIC, not PRIVATE: ScintillaEditBase.h itself includes
# Debugging.h, which lives there - so anything including the widget header needs
# it too. The .pro sidesteps this by putting both dirs on the whole project.
target_include_directories(scintilla_editbase SYSTEM PUBLIC
    ${scintilla_SOURCE_DIR}/include
    ${scintilla_SOURCE_DIR}/src
    ${scintilla_SOURCE_DIR}/qt/ScintillaEditBase)

# Exactly the .pro's DEFINES.
target_compile_definitions(scintilla_editbase PUBLIC SCINTILLA_QT=1 MAKING_LIBRARY=1)

# Qt5Compat is required on Qt 6: PlatQt.cpp and ScintillaQt.cpp use QTextCodec in
# more than ten places. LGPLv3, same as the rest of Qt - see brief D2/D3.
target_link_libraries(scintilla_editbase PUBLIC
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat)

# The .pro asks for c++1z. This is third-party code held to its own standard,
# not to core/'s C++11.
target_compile_features(scintilla_editbase PUBLIC cxx_std_17)

# Third-party: do not apply our -Werror.
target_compile_options(scintilla_editbase PRIVATE -w)

#-------------------------------------------------------------------------------
# Lexilla — the language lexers, split out of Scintilla at 5.0
#-------------------------------------------------------------------------------

file(GLOB _lex_sources
    "${lexilla_SOURCE_DIR}/lexlib/*.cxx"
    "${lexilla_SOURCE_DIR}/lexers/*.cxx"
    "${lexilla_SOURCE_DIR}/src/*.cxx")

add_library(lexilla STATIC ${_lex_sources})
add_library(Lexilla::lexilla ALIAS lexilla)

target_include_directories(lexilla SYSTEM PUBLIC
    ${lexilla_SOURCE_DIR}/include
    ${scintilla_SOURCE_DIR}/include)
target_include_directories(lexilla PRIVATE ${lexilla_SOURCE_DIR}/lexlib)
target_compile_features(lexilla PUBLIC cxx_std_17)
target_compile_options(lexilla PRIVATE -w)

message(STATUS "  Scintilla   ${SCINTILLA_VERSION} (fetched, HPND)")
message(STATUS "  Lexilla     ${LEXILLA_VERSION} (fetched, permissive)")
