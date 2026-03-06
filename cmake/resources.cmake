set(NENOSERPENT_QML_RESOURCE_FILES
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
    "src/qml/BotDebugPanel.qml"
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
    "src/levels/levels.json")

set(NENOSERPENT_SHADER_FILES
    "src/qml/blur.frag"
    "src/qml/lcd.frag")

function(nenoserpent_add_app_resources target_name)
    qt_add_resources("${target_name}" "res"
        PREFIX "/"
        FILES
            ${NENOSERPENT_QML_RESOURCE_FILES}
    )

    qt6_add_shaders("${target_name}" "shaders"
        PREFIX "/shaders"
        FILES
            ${NENOSERPENT_SHADER_FILES}
    )
endfunction()
