function(nenoserpent_add_offscreen_test target_name test_name)
    cmake_parse_arguments(
        ARG
        ""
        ""
        "SOURCES;QT_COMPONENTS;OPTIONAL_QT_COMPONENTS;LINK_LIBS"
        ${ARGN}
    )

    add_executable("${target_name}" ${ARG_SOURCES})
    target_include_directories("${target_name}" PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries("${target_name}" PRIVATE Qt6::Test Qt6::Core ${ARG_LINK_LIBS})

    foreach(component IN LISTS ARG_QT_COMPONENTS)
        target_link_libraries("${target_name}" PRIVATE "Qt6::${component}")
    endforeach()

    if(ARG_OPTIONAL_QT_COMPONENTS)
        nenoserpent_link_optional_qt_components("${target_name}" ${ARG_OPTIONAL_QT_COMPONENTS})
    endif()

    add_test(NAME "${test_name}" COMMAND "${target_name}")
    set_tests_properties("${test_name}" PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    nenoserpent_apply_project_options("${target_name}")
endfunction()

nenoserpent_add_offscreen_test(
    engine-adapter-tests EngineAdapterTest
    SOURCES adapter/session/test_engine_adapter.cpp
    QT_COMPONENTS Gui Qml
    OPTIONAL_QT_COMPONENTS Multimedia Sensors
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    core-rules-tests CoreRulesTest
    SOURCES core/test_core_rules.cpp
    QT_COMPONENTS Gui
    LINK_LIBS nenoserpent_core nenoserpent_fsm
)

nenoserpent_add_offscreen_test(
    session-core-tests SessionCoreTest
    SOURCES core/test_session_core.cpp
    QT_COMPONENTS Gui
    LINK_LIBS nenoserpent_core
)

nenoserpent_add_offscreen_test(
    spawn-distribution-tests SpawnDistributionTest
    SOURCES core/test_spawn_distribution.cpp
    QT_COMPONENTS Gui
    LINK_LIBS nenoserpent_core
)

nenoserpent_add_offscreen_test(
    session-runner-tests SessionRunnerTest
    SOURCES core/test_session_runner.cpp
    QT_COMPONENTS Gui
    LINK_LIBS nenoserpent_core
)

nenoserpent_add_offscreen_test(
    adapter-tests AdapterTest
    SOURCES adapter/ui/test_ui_action_parser.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-ui-dispatch-tests AdapterUiDispatchTest
    SOURCES adapter/ui/test_ui_action_dispatch_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-ui-command-tests AdapterUiCommandTest
    SOURCES adapter/ui/test_ui_command_controller.cpp
    QT_COMPONENTS Gui Qml
    OPTIONAL_QT_COMPONENTS Multimedia Sensors
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-semantics-tests AdapterSemanticsTest
    SOURCES adapter/ui/test_input_semantics_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-theme-tests AdapterThemeTest
    SOURCES adapter/ui/test_theme_view_model.cpp
    QT_COMPONENTS Gui Qml
    OPTIONAL_QT_COMPONENTS Multimedia Sensors
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-session-status-tests AdapterSessionStatusTest
    SOURCES adapter/session/test_session_status_view_model.cpp
    QT_COMPONENTS Gui Qml
    OPTIONAL_QT_COMPONENTS Multimedia Sensors
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-session-render-tests AdapterSessionRenderTest
    SOURCES adapter/session/test_session_render_view_model.cpp
    QT_COMPONENTS Gui Qml
    OPTIONAL_QT_COMPONENTS Multimedia Sensors
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-selection-tests AdapterSelectionTest
    SOURCES adapter/ui/test_selection_view_model.cpp
    QT_COMPONENTS Gui Qml
    OPTIONAL_QT_COMPONENTS Multimedia Sensors
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-audio-settings-tests AdapterAudioSettingsTest
    SOURCES adapter/audio/test_audio_settings_view_model.cpp
    QT_COMPONENTS Gui Qml
    OPTIONAL_QT_COMPONENTS Multimedia Sensors
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-level-loader-tests AdapterLevelLoaderTest
    SOURCES adapter/level/test_level_loader_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    service-level-repository-tests ServiceLevelRepositoryTest
    SOURCES services/test_level_repository_service.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    service-audio-bus-tests ServiceAudioBusTest
    SOURCES services/test_audio_bus_service.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    service-sound-manager-tests ServiceSoundManagerTest
    SOURCES
        services/test_sound_manager_service.cpp
        "${CMAKE_SOURCE_DIR}/src/sound_manager.cpp"
    QT_COMPONENTS Gui
    OPTIONAL_QT_COMPONENTS Multimedia
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-level-applier-tests AdapterLevelApplierTest
    SOURCES adapter/level/test_level_applier_adapter.cpp
    QT_COMPONENTS Gui
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-level-script-runtime-tests AdapterLevelScriptRuntimeTest
    SOURCES adapter/level/test_level_script_runtime_adapter.cpp
    QT_COMPONENTS Qml
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-ghost-store-tests AdapterGhostStoreTest
    SOURCES adapter/session/test_ghost_store_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-session-state-tests AdapterSessionStateTest
    SOURCES adapter/session/test_session_state_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-library-models-tests AdapterLibraryModelsTest
    SOURCES adapter/ui/test_library_models_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-choice-models-tests AdapterChoiceModelsTest
    SOURCES adapter/ui/test_choice_models_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-bot-tests AdapterBotTest
    SOURCES adapter/bot/test_bot_controller_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-bot-features-tests AdapterBotFeaturesTest
    SOURCES adapter/bot/test_bot_features_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-bot-backend-tests AdapterBotBackendTest
    SOURCES adapter/bot/test_bot_backend_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-bot-runtime-tests AdapterBotRuntimeTest
    SOURCES adapter/bot/test_bot_runtime_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-bot-ml-tests AdapterBotMlTest
    SOURCES adapter/bot/test_bot_ml_backend_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-bot-config-tests AdapterBotConfigTest
    SOURCES adapter/bot/test_bot_config_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-bot-state-tests AdapterBotStateTest
    SOURCES adapter/bot/test_bot_state_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-bot-orchestrator-tests AdapterBotOrchestratorTest
    SOURCES adapter/bot/test_bot_orchestrator_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-bot-snapshot-tests AdapterBotSnapshotTest
    SOURCES adapter/bot/test_bot_snapshot_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-bot-decision-applier-tests AdapterBotDecisionApplierTest
    SOURCES adapter/bot/test_bot_applier_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-bot-route-telemetry-tests AdapterBotRouteTelemetryTest
    SOURCES adapter/bot/test_bot_telemetry_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-bot-runtime-facade-tests AdapterBotRuntimeFacadeTest
    SOURCES adapter/bot/test_bot_facade_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)

nenoserpent_add_offscreen_test(
    adapter-bot-config-loader-tests AdapterBotConfigLoaderTest
    SOURCES adapter/bot/test_bot_loader_adapter.cpp
    LINK_LIBS nenoserpent_adapter
)
