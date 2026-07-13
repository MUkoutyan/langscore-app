cmake_minimum_required(VERSION 3.28)

project(langscore-app VERSION 0.8.12 LANGUAGES CXX)

set(CONFIG_FILE_VERSION 1.1.0)
configure_file(application_config.h.in application_config.h @ONLY)
configure_file(version.rc.in version.rc @ONLY)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED Qt6_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${Qt6_DIR}")
endif()
if(DEFINED QT_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${QT_DIR}")
endif()


#mutex.lockでのクラッシュへの対応
#https://stackoverflow.com/questions/78598141/first-stdmutexlock-crashes-in-application-built-with-latest-visual-studio
add_definitions(-D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network LinguistTools)

set(LANGUAGES "en" "ja" "zh_CN" "zh_TW")
set(TS_FILES)
foreach(LANG ${LANGUAGES})
    list(APPEND TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/langscore-app_${LANG}.ts")
endforeach()

set(PROJECT_SOURCES
    resources.qrc
    main.cpp
    MainWindow.cpp
    MainWindow.h
    MainWindow.ui
    src/csv.hpp
    src/utility.hpp
    src/invoker.h
    src/invoker.cpp
    src/settings.h
    src/settings.cpp
    src/settings_io.cpp
    src/graphics.hpp
    src/ui/dialog/AnalyzeDialog.h
    src/ui/dialog/AnalyzeDialog.cpp
    src/ui/dialog/AnalyzeDialog.ui
    src/ui/dialog/WriteDialog.h
    src/ui/dialog/WriteDialog.cpp
    src/ui/dialog/WriteDialog.ui
    src/ui/dialog/UpdatePluginDialog.h
    src/ui/dialog/UpdatePluginDialog.cpp
    src/ui/dialog/UpdatePluginDialog.ui
    src/ui/dialog/FirstWriteDialog.h
    src/ui/dialog/FirstWriteDialog.cpp
    src/ui/dialog/FirstWriteDialog.ui
    src/ui/dialog/PopupDialogBase.h
    src/ui/dialog/PopupDialogBase.cpp
    src/ui/dialog/MessageDialog.cpp
    src/ui/dialog/MessageDialog.h
    src/ui/dialog/TranslationApiSettingsDialog.h
    src/ui/dialog/TranslationApiSettingsDialog.cpp
    src/ui/dialog/TranslationApiSettingsDialog.ui
    src/ui/dialog/TranslationProgressDialog.h
    src/ui/dialog/TranslationProgressDialog.cpp
    src/ui/dialog/TranslationProgressDialog.ui
    src/ui/dialog/LanguageColumnSelectionDialog.h
    src/ui/dialog/LanguageColumnSelectionDialog.cpp
    src/ui/csvtable/ScriptCSVTable.h
    src/ui/csvtable/ScriptCSVTable.cpp
    src/ui/csvtable/MainCSVTable.h
    src/ui/csvtable/MainCSVTable.cpp
    src/ui/csvtable/MainCSVTableModel.cpp
    src/ui/csvtable/MainCSVTableModel.h
    src/ui/editor/FastCSVContainer.h
    src/ui/editor/FastCSVContainer.cpp
    src/ui/editor/EditDataManager.h
    src/ui/editor/EditDataManager.cpp
    src/ui/editor/CSVEditor.h
    src/ui/editor/CSVEditor.cpp
    src/ui/editor/CSVEditorTableModel.h
    src/ui/editor/CSVEditorTableModel.cpp
    src/ui/editor/CSVEditorSortFilterProxyModel.h
    src/ui/editor/CSVEditorSortFilterProxyModel.cpp
    src/ui/editor/CSVEditCommand.h
    src/ui/editor/CSVEditCommand.cpp
    src/ui/editor/ScriptCSVTableModel.h
    src/ui/editor/ScriptCSVTableModel.cpp
    src/ui/editor/ScriptEditorSortFilterProxyModel.h
    src/ui/editor/ScriptEditorSortFilterProxyModel.cpp
    src/ui/editor/MultiLineEditDelegate.h
    src/ui/editor/MultiLineEditDelegate.cpp
    src/ui/editor/EditorTableDefines.h
    src/ui/ComponentBase.h
    src/ui/FormTaskBar.h
    src/ui/FormTaskBar.cpp
    src/ui/FormTaskBar.ui
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
    src/ui/FramelessWindow.cpp
    src/ui/FramelessWindow.h
    src/ui/InvokerLogViewer.cpp
    src/ui/InvokerLogViewer.h
    src/ui/ClipDetectSettingTree.cpp
    src/ui/ClipDetectSettingTree.h
    src/ui/FileTree.cpp
    src/ui/FileTree.h
    ColorDialog.cpp
    ColorDialog.h
    src/translation/TranslationService.h
    src/translation/TranslationService.cpp
    src/translation/DeepLTranslationService.h
    src/translation/DeepLTranslationService.cpp
    src/translation/TranslationManager.h
    src/translation/TranslationManager.cpp
    src/service/ColorTheme.cpp
    src/service/ColorTheme.h
    src/service/GraphicsImageLoader.h
    src/service/GraphicsImageLoader.cpp
    src/service/ValidationService.h
    src/service/ValidationService.cpp
    src/service/ValidationErrorInfo.h
    src/service/LanguageNames.h
    src/service/LanguageNames.cpp
)

include_directories(
    "${DIVISI_PATH}/include"
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/ui/editor
    ${CMAKE_SOURCE_DIR}/src/ui/csvtable
    ${CMAKE_SOURCE_DIR}/src/
    ${CMAKE_CURRENT_BINARY_DIR}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)

    qt_add_executable(langscore-app
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    )
    
    qt_add_lupdate(
        SOURCE_TARGETS langscore-app
        TS_FILES ${TS_FILES}
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

if(MSVC)
    target_compile_options(langscore-app PRIVATE "/MP")
endif()

target_link_libraries(langscore-app PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network icu.lib)

set_target_properties(langscore-app PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER breezesinfonia.langscore.app
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

add_dependencies(langscore-app update_translations)

install(TARGETS langscore-app    # Install to CMAKE_INSTALL_PREFIX/bin/MyApp.exe
                                   #                           and ./binHelperApp.exe
        BUNDLE  DESTINATION .      # Install to CMAKE_INSTALL_PREFIX/MyApp.app/Contents/MacOS/MyApp
)

if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)  # Qt6 の場合
    # find_package(Qt5 REQUIRED COMPONENTS Widgets)  # Qt5 の場合

    # Qtのbinディレクトリを取得
    get_target_property(QT_BIN_DIR Qt6::qmake IMPORTED_LOCATION) # Qt6
    # get_target_property(QT_BIN_DIR Qt5::qmake IMPORTED_LOCATION) # Qt5
    get_filename_component(QT_BIN_DIR "${QT_BIN_DIR}" DIRECTORY)
   
    # windeployqtのパス
    set(WINDEPLOYQT_EXECUTABLE "${QT_BIN_DIR}/windeployqt6.exe")

    set(TARGET_PATH "$<TARGET_FILE_DIR:langscore-app>")
    message("")

    # windeployqtを実行するカスタムコマンド
    add_custom_command(TARGET langscore-app POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release "$<TARGET_FILE:langscore-app>"
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --debug "$<TARGET_FILE:langscore-app>"
        COMMENT "Running windeployqt on langscore-app"
    )

    get_filename_component(langscore_divisi_path "${PROJECT_SOURCE_DIR}/${DIVISI_PATH}" ABSOLUTE)
  
    # ビルド時に毎回コピーするように変更
    add_custom_target(copy_files ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:langscore-app>/bin/resource"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:langscore-app>/resources/fonts/"

        COMMAND ${CMAKE_COMMAND} -E copy ${langscore_divisi_path}/bin/rvcnv.exe $<TARGET_FILE_DIR:langscore-app>/bin/
        COMMAND ${CMAKE_COMMAND} -E copy ${langscore_divisi_path}/bin/divisi.exe $<TARGET_FILE_DIR:langscore-app>/bin/
        COMMAND ${CMAKE_COMMAND} -E copy ${langscore_divisi_path}/3rdparty/icu/bin/icudt76.dll $<TARGET_FILE_DIR:langscore-app>/bin/
        COMMAND ${CMAKE_COMMAND} -E copy ${langscore_divisi_path}/3rdparty/icu/bin/icuuc76.dll $<TARGET_FILE_DIR:langscore-app>/bin/
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${langscore_divisi_path}/resource $<TARGET_FILE_DIR:langscore-app>/bin/resource
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/resources/fonts $<TARGET_FILE_DIR:langscore-app>/resources/fonts
        COMMENT "Copying necessary files after build"
    )

    # langscore-app のビルド時に copy_files を実行
    add_dependencies(langscore-app copy_files)
endif()


if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(langscore-app)
endif()
