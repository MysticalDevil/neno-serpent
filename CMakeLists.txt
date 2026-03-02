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

add_subdirectory(src)

# Standard Resource mapping
qt_add_resources(NenoSerpent "res"
    PREFIX "/"
    FILES
        "src/qml/main.qml"
        "src/qml/ScreenView.qml"
        "src/qml/IconLabLayer.qml"
        "src/qml/HudLayer.qml"
        "src/qml/ReplayBanner.qml"
        "src/qml/ModalSurface.qml"
        "src/qml/ModalTextPanel.qml"
        "src/qml/ModalChoiceCard.qml"
        "src/qml/LevelUpModal.qml"
        "src/qml/BuffStatusPanel.qml"
        "src/qml/LibraryLayer.qml"
        "src/qml/MenuLayer.qml"
        "src/qml/StaticDebugLayer.qml"
        "src/qml/OverlayLayer.qml"
        "src/qml/WorldLayer.qml"
        "src/qml/SplashLayer.qml"
        "src/qml/DPad.qml"
        "src/qml/GBButton.qml"
        "src/qml/SmallButton.qml"
        "src/qml/UiActionRouter.qml"
        "src/qml/UiDebugController.qml"
        "src/qml/UiInputController.qml"
        "src/qml/UiLog.qml"
        "src/qml/InputPressController.qml"
        "src/qml/UiRuntimeState.qml"
        "src/qml/UiCompositionHost.qml"
        "src/qml/OSDLayer.qml"
        "src/qml/MedalRoom.qml"
        "src/qml/MedalRoomCard.qml"
        "src/qml/Shell.qml"
        "src/qml/ShellBridge.qml"
        "src/qml/ShellSurface.qml"
        "src/qml/ScreenBezel.qml"
        "src/qml/ShellBranding.qml"
        "src/qml/SpeakerGrill.qml"
        "src/qml/VolumeWheel.qml"
        "src/qml/ThemeCatalog.js"
        "src/qml/ScreenThemeTokens.js"
        "src/qml/PowerMeta.js"
        "src/qml/ReadabilityRules.js"
        "src/qml/LayerScale.js"
        "src/qml/icon.svg"
        "src/themes/theme_catalog.json"
        "src/levels/levels.json"
)

# Shader compilation
qt6_add_shaders(NenoSerpent "shaders"
    PREFIX "/shaders"
    FILES
        "src/qml/blur.frag"
        "src/qml/lcd.frag"
)

target_link_libraries(NenoSerpent PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

if(TARGET Qt6::Multimedia)
    target_link_libraries(NenoSerpent PRIVATE Qt6::Multimedia)
    target_compile_definitions(NenoSerpent PRIVATE NENOSERPENT_HAS_MULTIMEDIA)
endif()

if(TARGET Qt6::Sensors)
    target_link_libraries(NenoSerpent PRIVATE Qt6::Sensors)
    target_compile_definitions(NenoSerpent PRIVATE NENOSERPENT_HAS_SENSORS)
endif()

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

add_executable(bot-benchmark
    src/tools/bot_benchmark.cpp
)
target_include_directories(bot-benchmark PRIVATE src)
target_link_libraries(bot-benchmark PRIVATE Qt6::Core nenoserpent_core nenoserpent_adapter)

add_executable(engine-adapter-tests
    tests/test_engine_adapter.cpp
)
target_include_directories(engine-adapter-tests PRIVATE src)
target_link_libraries(engine-adapter-tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml nenoserpent_adapter)
if(TARGET Qt6::Multimedia)
    target_link_libraries(engine-adapter-tests PRIVATE Qt6::Multimedia)
    target_compile_definitions(engine-adapter-tests PRIVATE NENOSERPENT_HAS_MULTIMEDIA)
endif()
if(TARGET Qt6::Sensors)
    target_link_libraries(engine-adapter-tests PRIVATE Qt6::Sensors)
    target_compile_definitions(engine-adapter-tests PRIVATE NENOSERPENT_HAS_SENSORS)
endif()
add_test(NAME EngineAdapterTest COMMAND engine-adapter-tests)
set_tests_properties(EngineAdapterTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(core-rules-tests
    tests/test_core_rules.cpp
)
target_include_directories(core-rules-tests PRIVATE src)
target_link_libraries(core-rules-tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui nenoserpent_core nenoserpent_fsm)
add_test(NAME CoreRulesTest COMMAND core-rules-tests)
set_tests_properties(CoreRulesTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(session-core-tests
    tests/test_session_core.cpp
)
target_include_directories(session-core-tests PRIVATE src)
target_link_libraries(session-core-tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui nenoserpent_core)
add_test(NAME SessionCoreTest COMMAND session-core-tests)
set_tests_properties(SessionCoreTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(session-runner-tests
    tests/test_session_runner.cpp
)
target_include_directories(session-runner-tests PRIVATE src)
target_link_libraries(session-runner-tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui nenoserpent_core)
add_test(NAME SessionRunnerTest COMMAND session-runner-tests)
set_tests_properties(SessionRunnerTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-tests
    tests/test_ui_action_parser.cpp
)
target_include_directories(adapter-tests PRIVATE src)
target_link_libraries(adapter-tests PRIVATE Qt6::Test Qt6::Core nenoserpent_adapter)
add_test(NAME AdapterTest COMMAND adapter-tests)
set_tests_properties(AdapterTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-ui-dispatch-tests
    tests/test_ui_action_dispatch_adapter.cpp
)
target_include_directories(adapter-ui-dispatch-tests PRIVATE src)
target_link_libraries(adapter-ui-dispatch-tests PRIVATE Qt6::Test Qt6::Core nenoserpent_adapter)
add_test(NAME AdapterUiDispatchTest COMMAND adapter-ui-dispatch-tests)
set_tests_properties(AdapterUiDispatchTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-ui-command-tests
    tests/test_ui_command_controller.cpp
)
target_include_directories(adapter-ui-command-tests PRIVATE src)
target_link_libraries(adapter-ui-command-tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml nenoserpent_adapter)
if(TARGET Qt6::Multimedia)
    target_link_libraries(adapter-ui-command-tests PRIVATE Qt6::Multimedia)
    target_compile_definitions(adapter-ui-command-tests PRIVATE NENOSERPENT_HAS_MULTIMEDIA)
endif()
if(TARGET Qt6::Sensors)
    target_link_libraries(adapter-ui-command-tests PRIVATE Qt6::Sensors)
    target_compile_definitions(adapter-ui-command-tests PRIVATE NENOSERPENT_HAS_SENSORS)
endif()
add_test(NAME AdapterUiCommandTest COMMAND adapter-ui-command-tests)
set_tests_properties(AdapterUiCommandTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-semantics-tests
    tests/test_input_semantics_adapter.cpp
)
target_include_directories(adapter-semantics-tests PRIVATE src)
target_link_libraries(adapter-semantics-tests PRIVATE Qt6::Test Qt6::Core nenoserpent_adapter)
add_test(NAME AdapterSemanticsTest COMMAND adapter-semantics-tests)
set_tests_properties(AdapterSemanticsTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-theme-tests
    tests/test_theme_view_model.cpp
)
target_include_directories(adapter-theme-tests PRIVATE src)
target_link_libraries(adapter-theme-tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml nenoserpent_adapter)
if(TARGET Qt6::Multimedia)
    target_link_libraries(adapter-theme-tests PRIVATE Qt6::Multimedia)
    target_compile_definitions(adapter-theme-tests PRIVATE NENOSERPENT_HAS_MULTIMEDIA)
endif()
if(TARGET Qt6::Sensors)
    target_link_libraries(adapter-theme-tests PRIVATE Qt6::Sensors)
    target_compile_definitions(adapter-theme-tests PRIVATE NENOSERPENT_HAS_SENSORS)
endif()
add_test(NAME AdapterThemeTest COMMAND adapter-theme-tests)
set_tests_properties(AdapterThemeTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-session-status-tests
    tests/test_session_status_view_model.cpp
)
target_include_directories(adapter-session-status-tests PRIVATE src)
target_link_libraries(adapter-session-status-tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml nenoserpent_adapter)
if(TARGET Qt6::Multimedia)
    target_link_libraries(adapter-session-status-tests PRIVATE Qt6::Multimedia)
    target_compile_definitions(adapter-session-status-tests PRIVATE NENOSERPENT_HAS_MULTIMEDIA)
endif()
if(TARGET Qt6::Sensors)
    target_link_libraries(adapter-session-status-tests PRIVATE Qt6::Sensors)
    target_compile_definitions(adapter-session-status-tests PRIVATE NENOSERPENT_HAS_SENSORS)
endif()
add_test(NAME AdapterSessionStatusTest COMMAND adapter-session-status-tests)
set_tests_properties(AdapterSessionStatusTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-session-render-tests
    tests/test_session_render_view_model.cpp
)
target_include_directories(adapter-session-render-tests PRIVATE src)
target_link_libraries(adapter-session-render-tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml nenoserpent_adapter)
if(TARGET Qt6::Multimedia)
    target_link_libraries(adapter-session-render-tests PRIVATE Qt6::Multimedia)
    target_compile_definitions(adapter-session-render-tests PRIVATE NENOSERPENT_HAS_MULTIMEDIA)
endif()
if(TARGET Qt6::Sensors)
    target_link_libraries(adapter-session-render-tests PRIVATE Qt6::Sensors)
    target_compile_definitions(adapter-session-render-tests PRIVATE NENOSERPENT_HAS_SENSORS)
endif()
add_test(NAME AdapterSessionRenderTest COMMAND adapter-session-render-tests)
set_tests_properties(AdapterSessionRenderTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-selection-tests
    tests/test_selection_view_model.cpp
)
target_include_directories(adapter-selection-tests PRIVATE src)
target_link_libraries(adapter-selection-tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml nenoserpent_adapter)
if(TARGET Qt6::Multimedia)
    target_link_libraries(adapter-selection-tests PRIVATE Qt6::Multimedia)
    target_compile_definitions(adapter-selection-tests PRIVATE NENOSERPENT_HAS_MULTIMEDIA)
endif()
if(TARGET Qt6::Sensors)
    target_link_libraries(adapter-selection-tests PRIVATE Qt6::Sensors)
    target_compile_definitions(adapter-selection-tests PRIVATE NENOSERPENT_HAS_SENSORS)
endif()
add_test(NAME AdapterSelectionTest COMMAND adapter-selection-tests)
set_tests_properties(AdapterSelectionTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-audio-settings-tests
    tests/test_audio_settings_view_model.cpp
)
target_include_directories(adapter-audio-settings-tests PRIVATE src)
target_link_libraries(adapter-audio-settings-tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml nenoserpent_adapter)
if(TARGET Qt6::Multimedia)
    target_link_libraries(adapter-audio-settings-tests PRIVATE Qt6::Multimedia)
    target_compile_definitions(adapter-audio-settings-tests PRIVATE NENOSERPENT_HAS_MULTIMEDIA)
endif()
if(TARGET Qt6::Sensors)
    target_link_libraries(adapter-audio-settings-tests PRIVATE Qt6::Sensors)
    target_compile_definitions(adapter-audio-settings-tests PRIVATE NENOSERPENT_HAS_SENSORS)
endif()
add_test(NAME AdapterAudioSettingsTest COMMAND adapter-audio-settings-tests)
set_tests_properties(AdapterAudioSettingsTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-level-loader-tests
    tests/test_level_loader_adapter.cpp
)
target_include_directories(adapter-level-loader-tests PRIVATE src)
target_link_libraries(adapter-level-loader-tests PRIVATE Qt6::Test Qt6::Core nenoserpent_adapter)
add_test(NAME AdapterLevelLoaderTest COMMAND adapter-level-loader-tests)
set_tests_properties(AdapterLevelLoaderTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(service-level-repository-tests
    tests/test_level_repository_service.cpp
)
target_include_directories(service-level-repository-tests PRIVATE src)
target_link_libraries(service-level-repository-tests PRIVATE Qt6::Test Qt6::Core nenoserpent_adapter)
add_test(NAME ServiceLevelRepositoryTest COMMAND service-level-repository-tests)
set_tests_properties(ServiceLevelRepositoryTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(service-audio-bus-tests
    tests/test_audio_bus_service.cpp
)
target_include_directories(service-audio-bus-tests PRIVATE src)
target_link_libraries(service-audio-bus-tests PRIVATE Qt6::Test Qt6::Core nenoserpent_adapter)
add_test(NAME ServiceAudioBusTest COMMAND service-audio-bus-tests)
set_tests_properties(ServiceAudioBusTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-level-applier-tests
    tests/test_level_applier_adapter.cpp
)
target_include_directories(adapter-level-applier-tests PRIVATE src)
target_link_libraries(adapter-level-applier-tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui nenoserpent_adapter)
add_test(NAME AdapterLevelApplierTest COMMAND adapter-level-applier-tests)
set_tests_properties(AdapterLevelApplierTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-level-script-runtime-tests
    tests/test_level_script_runtime_adapter.cpp
)
target_include_directories(adapter-level-script-runtime-tests PRIVATE src)
target_link_libraries(adapter-level-script-runtime-tests PRIVATE Qt6::Test Qt6::Core Qt6::Qml nenoserpent_adapter)
add_test(NAME AdapterLevelScriptRuntimeTest COMMAND adapter-level-script-runtime-tests)
set_tests_properties(AdapterLevelScriptRuntimeTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-ghost-store-tests
    tests/test_ghost_store_adapter.cpp
)
target_include_directories(adapter-ghost-store-tests PRIVATE src)
target_link_libraries(adapter-ghost-store-tests PRIVATE Qt6::Test Qt6::Core nenoserpent_adapter)
add_test(NAME AdapterGhostStoreTest COMMAND adapter-ghost-store-tests)
set_tests_properties(AdapterGhostStoreTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-session-state-tests
    tests/test_session_state_adapter.cpp
)
target_include_directories(adapter-session-state-tests PRIVATE src)
target_link_libraries(adapter-session-state-tests PRIVATE Qt6::Test Qt6::Core nenoserpent_adapter)
add_test(NAME AdapterSessionStateTest COMMAND adapter-session-state-tests)
set_tests_properties(AdapterSessionStateTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-library-models-tests
    tests/test_library_models_adapter.cpp
)
target_include_directories(adapter-library-models-tests PRIVATE src)
target_link_libraries(adapter-library-models-tests PRIVATE Qt6::Test Qt6::Core nenoserpent_adapter)
add_test(NAME AdapterLibraryModelsTest COMMAND adapter-library-models-tests)
set_tests_properties(AdapterLibraryModelsTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-choice-models-tests
    tests/test_choice_models_adapter.cpp
)
target_include_directories(adapter-choice-models-tests PRIVATE src)
target_link_libraries(adapter-choice-models-tests PRIVATE Qt6::Test Qt6::Core nenoserpent_adapter)
add_test(NAME AdapterChoiceModelsTest COMMAND adapter-choice-models-tests)
set_tests_properties(AdapterChoiceModelsTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-bot-tests
    tests/test_bot_controller_adapter.cpp
)
target_include_directories(adapter-bot-tests PRIVATE src)
target_link_libraries(adapter-bot-tests PRIVATE Qt6::Test Qt6::Core nenoserpent_adapter)
add_test(NAME AdapterBotTest COMMAND adapter-bot-tests)
set_tests_properties(AdapterBotTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-bot-runtime-tests
    tests/test_bot_runtime_adapter.cpp
)
target_include_directories(adapter-bot-runtime-tests PRIVATE src)
target_link_libraries(adapter-bot-runtime-tests PRIVATE Qt6::Test Qt6::Core nenoserpent_adapter)
add_test(NAME AdapterBotRuntimeTest COMMAND adapter-bot-runtime-tests)
set_tests_properties(AdapterBotRuntimeTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(adapter-bot-config-tests
    tests/test_bot_config_adapter.cpp
)
target_include_directories(adapter-bot-config-tests PRIVATE src)
target_link_libraries(adapter-bot-config-tests PRIVATE Qt6::Test Qt6::Core nenoserpent_adapter)
add_test(NAME AdapterBotConfigTest COMMAND adapter-bot-config-tests)
set_tests_properties(AdapterBotConfigTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

nenoserpent_apply_project_options(
    nenoserpent_core
    nenoserpent_fsm
    nenoserpent_adapter
    NenoSerpent
    engine-adapter-tests
    core-rules-tests
    session-core-tests
    session-runner-tests
    adapter-tests
    adapter-ui-dispatch-tests
    adapter-ui-command-tests
    adapter-semantics-tests
    adapter-theme-tests
    adapter-session-status-tests
    adapter-session-render-tests
    adapter-selection-tests
    adapter-audio-settings-tests
    adapter-level-loader-tests
    service-level-repository-tests
    service-audio-bus-tests
    adapter-level-applier-tests
    adapter-level-script-runtime-tests
    adapter-ghost-store-tests
    adapter-session-state-tests
    adapter-library-models-tests
    adapter-choice-models-tests
    adapter-bot-tests
    adapter-bot-runtime-tests
    adapter-bot-config-tests
    bot-benchmark
)

if(UNIX AND NOT APPLE AND NOT ANDROID)
    install(TARGETS NenoSerpent RUNTIME DESTINATION bin)
    install(FILES linux/org.devil.nenoserpent.desktop DESTINATION share/applications)
    install(FILES src/qml/icon.svg DESTINATION share/icons/hicolor/scalable/apps RENAME nenoserpent.svg)
endif()
