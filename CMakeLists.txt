cmake_minimum_required(VERSION 3.5)

project(langscore-app VERSION 0.6.2 LANGUAGES CXX)
set(CONFIG_FILE_VERSION 1.0.0)
configure_file(application_config.h.in application_config.h @ONLY)
configure_file(version.rc.in version.rc @ONLY)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(TS_FILES langscore-app_ja_JP.ts)

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
    src/ui/PopupDialogBase.h
    src/ui/PopupDialogBase.cpp
    src/ui/MessageDialog.cpp
    src/ui/MessageDialog.h
    resources.qrc
)

include_directories(
    "${DIVISI_PATH}/include"
    ${CMAKE_CURRENT_BINARY_DIR}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
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

target_link_libraries(langscore-app PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

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

install(TARGETS langscore-app    # Install to CMAKE_INSTALL_PREFIX/bin/MyApp.exe
                                   #                           and ./binHelperApp.exe
        BUNDLE  DESTINATION .      # Install to CMAKE_INSTALL_PREFIX/MyApp.app/Contents/MacOS/MyApp
)
install(SCRIPT ${deploy_script})    # Add its runtime dependencies

file(COPY ${PROJECT_SOURCE_DIR}/${DIVISI_PATH}/rvcnv/rvcnv.exe
            DESTINATION ${PROJECT_BINARY_DIR}/bin)
file(COPY ${PROJECT_SOURCE_DIR}/${DIVISI_PATH}/bin/divisi.exe
            DESTINATION ${PROJECT_BINARY_DIR}/bin)
file(COPY ${PROJECT_SOURCE_DIR}/${DIVISI_PATH}/resource
            DESTINATION ${PROJECT_BINARY_DIR}/bin)
file(COPY ${PROJECT_SOURCE_DIR}/resources/fonts DESTINATION ${PROJECT_BINARY_DIR}/resources)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(langscore-app)
endif()
