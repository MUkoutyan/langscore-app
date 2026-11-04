cmake_minimum_required(VERSION 3.16)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# googletest を取得する（langscore-divisi と同じバージョン）
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# アプリと同じ MSVC ランタイム（/MD, /MDd）を使う
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/ui/editor
    ${CMAKE_SOURCE_DIR}/src/ui/csvtable
    ${CMAKE_SOURCE_DIR}/src/service
    ${CMAKE_BINARY_DIR}
)

# アプリ本体のソース（main.cpp を除く）を静的ライブラリ化し、テストから使用する
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

# test フォルダの tst_*.cpp をすべて1つのテスト用 exe にまとめる（ファイルを追加すれば自動で含まれる）
file(GLOB LANGSCORE_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tst_*.cpp)
add_executable(langscore_tests test_main.cpp test_common.h ${LANGSCORE_TEST_SOURCES})
target_link_libraries(langscore_tests PRIVATE langscore_app_lib GTest::gtest Qt6::Widgets Qt6::Network)
if(MSVC)
    target_compile_options(langscore_tests PRIVATE "/MP")
endif()
# PATH を設定しなくても exe 単体で起動できるよう、Qt の DLL を exe の隣にコピーする。
# テスト一覧の取得（gtest_discover_tests）はビルド直後に exe を実行するため、この処理を先に登録しておく
add_custom_command(TARGET langscore_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:langscore_tests> $<TARGET_FILE_DIR:langscore_tests>
    COMMAND_EXPAND_LISTS
)
# 更新処理のテストで divisi.exe の代わりに使う、何もせずに成功する実行ファイル（tst_ProjectUpdate.cpp）
add_executable(langscore_fake_divisi fake_divisi.cpp)
add_dependencies(langscore_tests langscore_fake_divisi)

# コピーした DLL からは Qt のプラグインを探せないため、場所を main で設定する（test_main.cpp）
target_compile_definitions(langscore_tests PRIVATE
    LANGSCORE_QT_PLUGIN_DIR="${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}"
    "LANGSCORE_FAKE_DIVISI_PATH=\"$<TARGET_FILE:langscore_fake_divisi>\""
    # 日本語パスの確認（tst_DivisiJapanesePath.cpp）で、アプリと一緒に置かれる実際の divisi.exe とテスト用のゲームを使う
    "LANGSCORE_DIVISI_PATH=\"$<TARGET_FILE_DIR:langscore-app>/bin/divisi.exe\""
    LANGSCORE_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

# テストを1件ずつ CTest に登録し、Visual Studio のテストエクスプローラーに表示できるようにする
include(GoogleTest)
# DeepL のテストは exe の隣の settings.ini を書き換え・削除するため、並列実行時に同時に動かないようにする
gtest_discover_tests(langscore_tests
    TEST_FILTER "DeepLTranslationServiceTest.*"
    PROPERTIES RESOURCE_LOCK app_settings_ini
    DISCOVERY_TIMEOUT 60
)
# Invoker のテストは exe の隣に divisi.exe が無いことを前提にし、更新処理のテストは偽の divisi.exe を置くため、同時に動かさない
gtest_discover_tests(langscore_tests
    TEST_FILTER "Invoker.*:ProjectUpdateTest.*"
    PROPERTIES RESOURCE_LOCK fake_divisi_exe
    DISCOVERY_TIMEOUT 60
)
gtest_discover_tests(langscore_tests
    TEST_FILTER "-DeepLTranslationServiceTest.*:Invoker.*:ProjectUpdateTest.*"
    DISCOVERY_TIMEOUT 60
)
