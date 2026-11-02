cmake_minimum_required(VERSION 3.16)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Test)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/ui/editor
    ${CMAKE_SOURCE_DIR}/src/ui/csvtable
    ${CMAKE_SOURCE_DIR}/src/service
    ${CMAKE_BINARY_DIR}
)

function(add_langscore_test test_name)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
    add_executable(${test_name} ${ARG_SOURCES})
    target_link_libraries(${test_name} PRIVATE Qt6::Widgets Qt6::Network Qt6::Test)
    if(MSVC)
        target_compile_options(${test_name} PRIVATE "/MP")
    endif()
    add_test(NAME ${test_name} COMMAND ${test_name})

    # Qt の DLL と platform plugin を PATH に通さずに実行できるようにする
    set(qt_bin_dir "$<TARGET_FILE_DIR:Qt6::Core>")
    set_tests_properties(${test_name} PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${qt_bin_dir};QT_QPA_PLATFORM=set:offscreen")
    set_target_properties(${test_name} PROPERTIES VS_DEBUGGER_ENVIRONMENT
        "PATH=${qt_bin_dir};%PATH%\nQT_QPA_PLATFORM=offscreen")
endfunction()

# アプリ本体のソース（main.cpp を除く）を静的ライブラリ化し、GUIクラスのテストで使用する
set(LANGSCORE_APP_LIB_SOURCES ${PROJECT_SOURCES})
list(REMOVE_ITEM LANGSCORE_APP_LIB_SOURCES main.cpp resources.qrc)
list(TRANSFORM LANGSCORE_APP_LIB_SOURCES PREPEND "${CMAKE_SOURCE_DIR}/")
add_library(langscore_app_lib STATIC ${LANGSCORE_APP_LIB_SOURCES})
set_target_properties(langscore_app_lib PROPERTIES AUTOUIC ON)
target_include_directories(langscore_app_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(langscore_app_lib PUBLIC Qt6::Widgets Qt6::Network icu.lib)
if(MSVC)
    target_compile_options(langscore_app_lib PRIVATE "/MP")
endif()

function(add_langscore_app_test test_name)
    add_langscore_test(${test_name} SOURCES ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE langscore_app_lib)
endfunction()

# FastCSVContainer のテスト
add_langscore_test(tst_FastCSVContainer
    SOURCES
        tst_FastCSVContainer.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/editor/FastCSVContainer.cpp
)

# utility.hpp のテスト
add_langscore_test(tst_Utility
    SOURCES
        tst_Utility.cpp
)

# LanguageNames のテスト
add_langscore_test(tst_LanguageNames
    SOURCES
        tst_LanguageNames.cpp
        ${CMAKE_SOURCE_DIR}/src/service/LanguageNames.cpp
)

# csv.hpp のテスト
add_langscore_test(tst_Csv
    SOURCES
        tst_Csv.cpp
)

# CSVEditCommand のテスト
add_langscore_test(tst_CSVEditCommand
    SOURCES
        tst_CSVEditCommand.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/editor/CSVEditCommand.cpp
)

# TranslationService のテスト
add_langscore_test(tst_TranslationService
    SOURCES
        tst_TranslationService.cpp
        ${CMAKE_SOURCE_DIR}/src/translation/TranslationService.cpp
)

# settings のテスト
add_langscore_test(tst_Settings
    SOURCES
        tst_Settings.cpp
        ${CMAKE_SOURCE_DIR}/src/settings.cpp
        ${CMAKE_SOURCE_DIR}/src/settings_io.cpp
)

# アプリ本体のライブラリを使うテスト
add_langscore_app_test(tst_DeepLTranslationService)
add_langscore_app_test(tst_CSVEditorSortFilterProxyModel)
add_langscore_app_test(tst_CSVEditorTableModel)
add_langscore_app_test(tst_ScriptCSVTableModel)
add_langscore_app_test(tst_Invoker)
add_langscore_app_test(tst_FileTree)
add_langscore_app_test(tst_ScriptCSVTable)
