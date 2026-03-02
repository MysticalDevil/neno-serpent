import QtQuick
import QtQuick.Controls
import NenoSerpent 1.0

Window {
    id: window
    readonly property int shellBaseWidth: 350
    readonly property int shellBaseHeight: 620
    readonly property int screenBaseWidth: 240
    readonly property int screenBaseHeight: 216
    readonly property real screenOnlyScale: 3.0
    readonly property string uiMode: typeof appUiMode === "string" ? appUiMode : "full"
    readonly property bool fullUiMode: uiMode === "full"
    readonly property bool screenOnlyUiMode: uiMode === "screen"
    readonly property bool shellOnlyUiMode: uiMode === "shell"
    readonly property bool androidPlatform: Qt.platform.os === "android"

    width: screenOnlyUiMode ? Math.round(screenBaseWidth * screenOnlyScale) : shellBaseWidth
    height: screenOnlyUiMode ? Math.round(screenBaseHeight * screenOnlyScale) : shellBaseHeight
    visible: true
    title: qsTr("NenoSerpent")
    color: "#1a1a1a"

    readonly property color p0: themeViewModel.palette[0]
    readonly property color p1: themeViewModel.palette[1]
    readonly property color p2: themeViewModel.palette[2]
    readonly property color p3: themeViewModel.palette[3]
    readonly property string gameFont: "Monospace"
    readonly property var commandControllerRef: uiCommandController
    readonly property var audioSettingsViewModelRef: audioSettingsViewModel
    readonly property var sessionRenderViewModelRef: sessionRenderViewModel
    readonly property var inputInjectorRef: inputInjector
    readonly property var themeViewModelRef: themeViewModel
    property real elapsed: 0.0
    readonly property int elapsedTickMs: androidPlatform ? 50 : 16
    readonly property int currentState: sessionRenderViewModelRef.state
    readonly property var inputAction: ({
        NavUp: "nav_up",
        NavDown: "nav_down",
        NavLeft: "nav_left",
        NavRight: "nav_right",
        Primary: "primary",
        Secondary: "secondary",
        Start: "start",
        SelectShort: "select_short",
        Back: "back",
        Escape: "escape",
        ToggleIconLab: "toggle_icon_lab",
        ToggleBotPanel: "toggle_bot_panel",
        ToggleShellColor: "toggle_shell_color",
        ToggleMusic: "toggle_music",
        ToggleBot: "cycle_bot_mode"
    })
    Timer {
        interval: window.elapsedTickMs
        repeat: true
        running: true
        onTriggered: window.elapsed = (window.elapsed + (interval / 1000.0)) % 1000.0
    }

    UiRuntimeState {
        id: uiRuntimeState
    }

    UiLog {
        id: uiLog
        runtimeLogger: typeof runtimeLogBridge === "undefined" ? null : runtimeLogBridge
    }

    UiActionRouter {
        id: uiActionRouter
        commandController: window.commandControllerRef
        inputController: uiInputController
        debugController: uiDebugController
        uiLogger: uiLog
        screen: compositionHost.screenItem
        currentState: window.currentState
        actionMap: window.inputAction
        iconDebugMode: uiRuntimeState.iconDebugMode
        staticDebugScene: uiRuntimeState.staticDebugScene
    }

    UiDebugController {
        id: uiDebugController
        commandController: window.commandControllerRef
        inputInjector: window.inputInjectorRef
        uiLogger: uiLog
        actionMap: window.inputAction
        currentState: window.currentState
        iconDebugMode: uiRuntimeState.iconDebugMode
        staticDebugScene: uiRuntimeState.staticDebugScene
        staticDebugOptions: uiRuntimeState.staticDebugOptions
        screen: compositionHost.screenItem
        inputController: uiInputController
        clearDirectionVisuals: uiInputController.clearDirectionVisuals
        stateOwner: uiRuntimeState
    }

    InputPressController {
        id: inputPressController
        currentState: window.currentState
        hasSave: sessionStatusViewModel.hasSave
        iconDebugMode: uiRuntimeState.iconDebugMode
        actionMap: window.inputAction
        commandController: window.commandControllerRef
        screen: compositionHost.screenItem
    }

    UiInputController {
        id: uiInputController
        commandController: window.commandControllerRef
        actionRouter: uiActionRouter
        inputPressController: inputPressController
        debugController: uiDebugController
        uiLogger: uiLog
        shellBridge: compositionHost.bridge
        sessionRenderViewModel: window.sessionRenderViewModelRef
        audioSettingsViewModel: window.audioSettingsViewModelRef
        screen: compositionHost.screenItem
        iconDebugMode: uiRuntimeState.iconDebugMode
        actionMap: window.inputAction
    }

    UiCompositionHost {
        id: compositionHost
        anchors.fill: parent
        fullUiMode: window.fullUiMode
        screenOnlyUiMode: window.screenOnlyUiMode
        shellOnlyUiMode: window.shellOnlyUiMode
        shellBaseWidth: window.shellBaseWidth
        shellBaseHeight: window.shellBaseHeight
        screenBaseWidth: window.screenBaseWidth
        screenBaseHeight: window.screenBaseHeight
        commandController: window.commandControllerRef
        inputController: uiInputController
        themeViewModel: window.themeViewModelRef
        audioSettingsViewModel: window.audioSettingsViewModelRef
        uiRuntimeState: uiRuntimeState
        p0: window.p0
        p1: window.p1
        p2: window.p2
        p3: window.p3
        gameFont: window.gameFont
        elapsed: window.elapsed
    }
}
