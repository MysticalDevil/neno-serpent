cmake_minimum_required(VERSION 4.0...4.1)
if(POLICY CMP0168)
    cmake_policy(SET CMP0168 NEW)
endif()

project(NenoSerpent VERSION 2.0.0 LANGUAGES CXX)

# Place executables in the build root (not subdirectories like build/<profile>/src).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(CMAKE_CONFIGURATION_TYPES)
    foreach(cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${cfg}" cfg_upper)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} ${CMAKE_BINARY_DIR})
    endforeach()
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Use: cmake -S . -B build/<profile>")
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type")
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

option(NENOSERPENT_OPTIMIZE_SIZE "Enable binary size optimizations" ON)
option(NENOSERPENT_USE_CCACHE "Use ccache as compiler launcher when available" ON)

add_library(nenoserpent_project_options INTERFACE)
target_compile_features(nenoserpent_project_options INTERFACE cxx_std_23)
set_target_properties(nenoserpent_project_options PROPERTIES EXPORT_NAME project_options)

add_library(nenoserpent_project_warnings INTERFACE)
target_compile_options(nenoserpent_project_warnings INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-Wall;-Wextra;-Wpedantic>
)

function(nenoserpent_enable_ccache)
    if(NOT NENOSERPENT_USE_CCACHE)
        return()
    endif()
    find_program(NENOSERPENT_CCACHE_PROGRAM ccache)
    if(NOT NENOSERPENT_CCACHE_PROGRAM)
        return()
    endif()
    set(NENOSERPENT_CCACHE_DIR "${CMAKE_BINARY_DIR}/.ccache")
    set(CMAKE_C_COMPILER_LAUNCHER
        "${CMAKE_COMMAND};-E;env;CCACHE_DIR=${NENOSERPENT_CCACHE_DIR};${NENOSERPENT_CCACHE_PROGRAM}"
        PARENT_SCOPE)
    set(CMAKE_CXX_COMPILER_LAUNCHER
        "${CMAKE_COMMAND};-E;env;CCACHE_DIR=${NENOSERPENT_CCACHE_DIR};${NENOSERPENT_CCACHE_PROGRAM}"
        PARENT_SCOPE)
    message(STATUS "ccache enabled: ${NENOSERPENT_CCACHE_PROGRAM} (dir=${NENOSERPENT_CCACHE_DIR})")
endfunction()

function(nenoserpent_apply_project_options)
    foreach(target_name IN LISTS ARGN)
        if(TARGET "${target_name}")
            target_link_libraries("${target_name}" PRIVATE nenoserpent_project_options)
            target_link_libraries("${target_name}" PRIVATE nenoserpent_project_warnings)
        endif()
    endforeach()
endfunction()

nenoserpent_enable_ccache()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick OpenGL ShaderTools Test)
find_package(Qt6 QUIET COMPONENTS Multimedia Sensors)

qt_standard_project_setup(REQUIRES 6.7)
set(CMAKE_AUTOUIC OFF)

include(cmake/qt_optional_components.cmake)
include(cmake/resources.cmake)

add_subdirectory(src)

nenoserpent_add_app_resources(NenoSerpent)

target_link_libraries(NenoSerpent PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

nenoserpent_link_optional_qt_components(
    NenoSerpent
    Multimedia
    Sensors
)

# Keep verbose logs in Debug, compact logs in RelWithDebInfo, and silence release-like builds.
target_compile_definitions(NenoSerpent PRIVATE
    $<$<CONFIG:Debug>:NENOSERPENT_BUILD_DEBUG>
    $<$<CONFIG:RelWithDebInfo>:NENOSERPENT_BUILD_DEV>
    $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:NENOSERPENT_BUILD_RELEASE>
    $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:
        QT_NO_DEBUG_OUTPUT;QT_NO_INFO_OUTPUT;QT_NO_WARNING_OUTPUT>
)

if(NENOSERPENT_OPTIMIZE_SIZE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        foreach(target_name IN ITEMS nenoserpent_core nenoserpent_fsm nenoserpent_adapter NenoSerpent)
            if(TARGET ${target_name})
                target_compile_options(${target_name} PRIVATE
                    -ffunction-sections
                    -fdata-sections
                    $<$<CONFIG:MinSizeRel>:-Oz>
                )
            endif()
        endforeach()
        target_link_options(NenoSerpent PRIVATE -Wl,--gc-sections)
    endif()
endif()

set_property(TARGET NenoSerpent PROPERTY
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
set_property(TARGET NenoSerpent PROPERTY QT_ANDROID_PACKAGE_NAME "org.devil.nenoserpent")
set_property(TARGET NenoSerpent PROPERTY QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}")
set_property(TARGET NenoSerpent PROPERTY QT_ANDROID_VERSION_CODE 200)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(NenoSerpent)
    qt_finalize_project()
endif()

if(NOT ANDROID)
    enable_testing()
    add_subdirectory(tools)
    add_subdirectory(tests)
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
    install(TARGETS NenoSerpent RUNTIME DESTINATION bin)
    install(FILES linux/org.devil.nenoserpent.desktop DESTINATION share/applications)
    install(FILES src/qml/icon.svg DESTINATION share/icons/hicolor/scalable/apps RENAME nenoserpent.svg)
endif()
