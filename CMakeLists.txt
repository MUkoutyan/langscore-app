cmake_minimum_required(VERSION 3.21)

project(langscore-app VERSION 0.8.9 LANGUAGES CXX)

set(CONFIG_FILE_VERSION 1.1.0)
configure_file(application_config.h.in application_config.h @ONLY)
configure_file(version.rc.in version.rc @ONLY)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH ${QT_DIR})

#mutex.lockでのクラッシュへの対応
#https://stackoverflow.com/questions/78598141/first-stdmutexlock-crashes-in-application-built-with-latest-visual-studio
add_definitions(-D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(LANGUAGES "en" "ja" "zh_CN" "zh_TW")
set(TS_FILES)
foreach(LANG ${LANGUAGES})
    list(APPEND TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/langscore-app_${LANG}.ts")
endforeach()

set(PROJECT_SOURCES
    main.cpp
    MainWindow.cpp
    MainWindow.h
    MainWindow.ui
    src/ui/ComponentBase.h
    src/ui/FormTaskBar.h
    src/ui/FormTaskBar.cpp
    src/ui/FormTaskBar.ui
    src/ui/AnalyzeDialog.h
    src/ui/AnalyzeDialog.cpp
    src/ui/AnalyzeDialog.ui
    src/ui/WriteModeComponent.h
    src/ui/WriteModeComponent.cpp
    src/ui/WriteModeComponent.ui
    src/ui/LanguageSelectComponent.h
    src/ui/LanguageSelectComponent.cpp
    src/ui/PackingMode.h
    src/ui/PackingMode.cpp
    src/ui/PackingMode.ui
    src/ui/ScriptViewer.h
    src/ui/ScriptViewer.cpp
    src/csv.hpp
    src/utility.hpp
    src/invoker.h
    src/invoker.cpp
    src/settings.h
    src/settings.cpp
    src/graphics.hpp
    src/ui/WriteDialog.h
    src/ui/WriteDialog.cpp
    src/ui/WriteDialog.ui
    src/ui/UpdatePluginDialog.h
    src/ui/UpdatePluginDialog.cpp
    src/ui/UpdatePluginDialog.ui
    src/ui/FirstWriteDialog.h
    src/ui/FirstWriteDialog.cpp
    src/ui/FirstWriteDialog.ui
    src/ui/PopupDialogBase.h
    src/ui/PopupDialogBase.cpp
    src/ui/MessageDialog.cpp
    src/ui/MessageDialog.h
    src/ui/FramelessWindow.cpp
    src/ui/FramelessWindow.h
    src/ui/InvokerLogViewer.cpp
    src/ui/InvokerLogViewer.h
    src/ui/ColorTheme.cpp
    src/ui/ColorTheme.h
    src/ui/ClipDetectSettingTree.cpp
    src/ui/ClipDetectSettingTree.h
    src/ui/CSVEditor.cpp
    src/ui/CSVEditor.h
    src/ui/LoadFileManager.h
    src/ui/LoadFileManager.cpp
    src/ui/FileTree.cpp
    src/ui/FileTree.h
    src/ui/ScriptCSVTable.h
    src/ui/ScriptCSVTable.cpp
    src/ui/MainCSVTable.h
    src/ui/MainCSVTable.cpp
    src/ui/CSVEditor.h
    src/ui/CSVEditor.cpp
    resources.qrc
    ColorDialog.cpp
    ColorDialog.h
)

include_directories(
    "${DIVISI_PATH}/include"
    ${CMAKE_CURRENT_BINARY_DIR}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)

    qt_add_lupdate(
        SOURCE_TARGETS langscore-app
        TS_FILES ${TS_FILES}
    )
    
    qt_add_executable(langscore-app
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    )
    qt_add_translations(langscore-app TS_FILES ${TS_FILES} SOURCES ${PROJECT_SOURCES})
# App bundles on macOS have an .app suffix
if(APPLE)
    set(executable_path "$<TARGET_FILE_NAME:langscore-app>.app")
else()
    set(executable_path "\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:langscore-app>")
endif()
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET langscore-app APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

else()
    if(ANDROID)
        add_library(langscore-app SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(langscore-app
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# # Enable testing
# enable_testing()

# # Find the Qt6Test module
# find_package(Qt6 REQUIRED COMPONENTS Test)

# # Include directories for QtTest
# include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${Qt6Test_INCLUDE_DIRS})

# add_subdirectory(test)


target_link_libraries(langscore-app PRIVATE Qt${QT_VERSION_MAJOR}::Widgets icu.lib)

set_target_properties(langscore-app PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER breezesinfonia.langscore.app
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

set(deploy_script "${CMAKE_CURRENT_BINARY_DIR}/deploy_langscore.cmake")
file(GENERATE OUTPUT ${deploy_script} CONTENT "
# Including the file pointed to by QT_DEPLOY_SUPPORT ensures the generated
# deployment script has access to qt_deploy_runtime_dependencies()
include(\"${QT_DEPLOY_SUPPORT}\")
qt_deploy_runtime_dependencies(
    EXECUTABLE \"${executable_path}\"
    ADDITIONAL_EXECUTABLES \"${helper_app_path}\"
    GENERATE_QT_CONF
    VERBOSE
)")

add_dependencies(langscore-app update_translations)

install(TARGETS langscore-app    # Install to CMAKE_INSTALL_PREFIX/bin/MyApp.exe
                                   #                           and ./binHelperApp.exe
        BUNDLE  DESTINATION .      # Install to CMAKE_INSTALL_PREFIX/MyApp.app/Contents/MacOS/MyApp
)
install(SCRIPT ${deploy_script})    # Add its runtime dependencies

if(WIN32)
   find_package(Qt6 REQUIRED COMPONENTS Widgets)  # Qt6 の場合
   # find_package(Qt5 REQUIRED COMPONENTS Widgets)  # Qt5 の場合

   # Qtのbinディレクトリを取得
   get_target_property(QT_BIN_DIR Qt6::qmake IMPORTED_LOCATION) # Qt6
   # get_target_property(QT_BIN_DIR Qt5::qmake IMPORTED_LOCATION) # Qt5
   get_filename_component(QT_BIN_DIR "${QT_BIN_DIR}" DIRECTORY)
   
   # windeployqtのパス
   set(WINDEPLOYQT_EXECUTABLE "${QT_BIN_DIR}/windeployqt6.exe")

   # 出力ディレクトリを指定（CMAKE_RUNTIME_OUTPUT_DIRECTORYが未指定ならデフォルトのビルドディレクトリを使う）
   set(TARGET_PATH "$<TARGET_FILE_DIR:langscore-app>")  # MyApp は実行ファイルターゲット名

   # windeployqtを実行するカスタムコマンド
   add_custom_command(TARGET langscore-app POST_BUILD
       COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release "$<TARGET_FILE:langscore-app>"
       COMMAND "${WINDEPLOYQT_EXECUTABLE}" --debug "$<TARGET_FILE:langscore-app>"
       COMMENT "Running windeployqt on langscore-app"
   )

   get_filename_component(langscore_divisi_path "${PROJECT_SOURCE_DIR}/${DIVISI_PATH}" ABSOLUTE)
   
   file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
   file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/bin/resource)
   file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/resources/fonts)

   # ビルド時に毎回コピーするように変更
   add_custom_target(copy_files ALL
       COMMAND ${CMAKE_COMMAND} -E copy ${langscore_divisi_path}/bin/rvcnv.exe ${PROJECT_BINARY_DIR}/bin/
       COMMAND ${CMAKE_COMMAND} -E copy ${langscore_divisi_path}/bin/divisi.exe ${PROJECT_BINARY_DIR}/bin/
       COMMAND ${CMAKE_COMMAND} -E copy ${langscore_divisi_path}/3rdparty/icu/bin/icudt76.dll ${PROJECT_BINARY_DIR}/bin/
       COMMAND ${CMAKE_COMMAND} -E copy ${langscore_divisi_path}/3rdparty/icu/bin/icuuc76.dll ${PROJECT_BINARY_DIR}/bin/
       COMMAND ${CMAKE_COMMAND} -E copy_directory ${langscore_divisi_path}/resource ${PROJECT_BINARY_DIR}/bin/resource
       COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/resources/fonts ${PROJECT_BINARY_DIR}/resources/fonts
       COMMENT "Copying necessary files after build"
   )

   # langscore-app のビルド時に copy_files を実行
   add_dependencies(langscore-app copy_files)
endif()

#file(COPY ${PROJECT_SOURCE_DIR}/${DIVISI_PATH}/bin/rvcnv.exe
#            DESTINATION ${PROJECT_BINARY_DIR}/bin)
#file(COPY ${PROJECT_SOURCE_DIR}/${DIVISI_PATH}/bin/divisi.exe
#            DESTINATION ${PROJECT_BINARY_DIR}/bin)
#file(COPY ${PROJECT_SOURCE_DIR}/${DIVISI_PATH}/3rdparty/icu/bin/icudt76.dll
#            DESTINATION ${PROJECT_BINARY_DIR}/bin)
#file(COPY ${PROJECT_SOURCE_DIR}/${DIVISI_PATH}/3rdparty/icu/bin/icuuc76.dll
#            DESTINATION ${PROJECT_BINARY_DIR}/bin)
#file(COPY ${PROJECT_SOURCE_DIR}/${DIVISI_PATH}/resource
#            DESTINATION ${PROJECT_BINARY_DIR}/bin)
#file(COPY ${PROJECT_SOURCE_DIR}/resources/fonts DESTINATION ${PROJECT_BINARY_DIR}/resources)


if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(langscore-app)
endif()
