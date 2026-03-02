add_library(nenoserpent_core
    core/game/rules.cpp
    core/buff/runtime.cpp
    core/session/core.cpp
    core/session/runner.cpp
    core/replay/timeline.cpp
    core/session/runtime.cpp
    core/level/runtime.cpp
    core/achievement/rules.cpp
    core/choice/runtime.cpp
)
target_include_directories(nenoserpent_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nenoserpent_core PRIVATE Qt6::Core)
target_compile_definitions(nenoserpent_core PRIVATE
    $<$<CONFIG:Debug>:NENOSERPENT_BUILD_DEBUG>
    $<$<CONFIG:RelWithDebInfo>:NENOSERPENT_BUILD_DEV>
    $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:NENOSERPENT_BUILD_RELEASE>
)

add_library(nenoserpent_fsm
    fsm/state_factory.cpp
    fsm/states.cpp
)
target_include_directories(nenoserpent_fsm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nenoserpent_fsm PRIVATE nenoserpent_core Qt6::Core)

add_library(nenoserpent_adapter
    app_state.h
    audio/cue.h
    audio/event.h
    audio/score.h
    audio/score.cpp
    adapter/engine_adapter.h
    adapter/engine_adapter.cpp
    adapter/haptics/controller.h
    adapter/haptics/controller.cpp
    adapter/view_models/audio.h
    adapter/view_models/audio.cpp
    adapter/view_models/theme.h
    adapter/view_models/theme.cpp
    adapter/view_models/status.h
    adapter/view_models/status.cpp
    adapter/view_models/render.h
    adapter/view_models/render.cpp
    adapter/view_models/selection.h
    adapter/view_models/selection.cpp
    adapter/bot/controller.h
    adapter/bot/controller.cpp
    adapter/bot/config.h
    adapter/bot/config.cpp
    adapter/bot/runtime.h
    adapter/bot/runtime.cpp
    adapter/input/router.cpp
    adapter/tick_driver.cpp
    adapter/simulation.cpp
    adapter/board_state.cpp
    adapter/session/runtime.cpp
    adapter/session/state.cpp
    adapter/level/flow.cpp
    adapter/persistence.cpp
    adapter/choices.cpp
    adapter/lifecycle.cpp
    adapter/view_model.cpp
    adapter/ui/action.cpp
    adapter/ui/controller.cpp
    adapter/input/semantics.cpp
    adapter/achievement/runtime.cpp
    adapter/ghost/store.cpp
    adapter/models/choice.cpp
    adapter/models/library.cpp
    adapter/profile/bridge.cpp
    adapter/level/applier.cpp
    adapter/level/loader.cpp
    adapter/level/script_runtime.cpp
    logging/categories.h
    logging/categories.cpp
    logging/mode.h
    logging/policy.h
    logging/policy.cpp
    logging/runtime_logger.h
    logging/runtime_logger.cpp
    services/audio/bus.cpp
    services/level/repository.cpp
    services/save/repository.cpp
    profile_manager.cpp
    profile_manager.h
)
target_include_directories(nenoserpent_adapter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nenoserpent_adapter PRIVATE nenoserpent_fsm nenoserpent_core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)
qt_add_resources(nenoserpent_adapter "audio_score_res"
    PREFIX "/"
    FILES
        "audio/score_catalog.json"
)

qt_add_resources(nenoserpent_adapter "bot_strategy_res"
    PREFIX "/"
    FILES
        "adapter/bot/strategy_profiles.json"
)

qt_add_executable(NenoSerpent
    MANUAL_FINALIZATION
    main.cpp
    power_up_id.h
    input_injection_pipe.cpp
    input_injection_pipe.h
    sound_manager.cpp
    sound_manager.h
)
target_link_libraries(NenoSerpent PRIVATE nenoserpent_adapter)

if(TARGET Qt6::Multimedia)
    target_link_libraries(nenoserpent_adapter PRIVATE Qt6::Multimedia)
    target_compile_definitions(nenoserpent_adapter PRIVATE NENOSERPENT_HAS_MULTIMEDIA)
endif()
if(TARGET Qt6::Sensors)
    target_link_libraries(nenoserpent_adapter PRIVATE Qt6::Sensors)
    target_compile_definitions(nenoserpent_adapter PRIVATE NENOSERPENT_HAS_SENSORS)
endif()
