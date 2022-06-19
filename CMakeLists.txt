﻿cmake_minimum_required(VERSION 3.5)

project(langscore-app VERSION 0.1 LANGUAGES CXX)

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
        src/ui/MainComponent.h
        src/ui/MainComponent.cpp
        src/ui/MainComponent.ui
        src/ui/WriteModeComponent.h
        src/ui/WriteModeComponent.cpp
        src/ui/WriteModeComponent.ui
        src/ui/SwitchButton.h
        src/ui/SwitchButton.cpp
        src/ui/ScriptViewer.h
        src/ui/ScriptViewer.cpp
        src/csv.hpp
        src/invoker.h
        src/invoker.cpp
        src/settings.h
        src/settings.cpp
        resources.qrc
        ${TS_FILES}
)

include_directories(
        "../langscore-divisi/include"
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(langscore-app
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
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

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
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

file(COPY ${PROJECT_SOURCE_DIR}/../langscore-divisi/rvcnv/rvcnv.exe
          DESTINATION ${PROJECT_BINARY_DIR}/bin)
file(COPY ${PROJECT_SOURCE_DIR}/../langscore-divisi/bin/divisi.exe
          DESTINATION ${PROJECT_BINARY_DIR}/bin)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(langscore-app)
endif()