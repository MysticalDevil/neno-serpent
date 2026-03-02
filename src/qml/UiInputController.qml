import QtQuick
import NenoSerpent 1.0

QtObject {
    id: controller

    property var commandController
    property var actionRouter
    property var inputPressController
    property var debugController
    property var uiLogger
    property var shellBridge
    property var sessionRenderViewModel
    property var audioSettingsViewModel
    property var screen
    property bool iconDebugMode: false
    property var actionMap: ({})
    readonly property bool logButtons: uiLogger ? uiLogger.isDevOrDebug : false
    readonly property bool detailedButtonLogs: uiLogger ? uiLogger.isDebug : false
    readonly property var directionActionByAxis: ({
        up: "NavUp",
        down: "NavDown",
        left: "NavLeft",
        right: "NavRight"
    })
    readonly property var pressedKeyActions: ({
        [Qt.Key_Up]: { type: "dispatch", action: "NavUp" },
        [Qt.Key_Down]: { type: "dispatch", action: "NavDown" },
        [Qt.Key_Left]: { type: "dispatch", action: "NavLeft" },
        [Qt.Key_Right]: { type: "dispatch", action: "NavRight" },
        [Qt.Key_A]: { type: "primary_press" },
        [Qt.Key_Z]: { type: "primary_press" },
        [Qt.Key_B]: { type: "secondary_press" },
        [Qt.Key_X]: { type: "secondary_press" },
        [Qt.Key_S]: { type: "start_press" },
        [Qt.Key_Space]: { type: "start_press" },
        [Qt.Key_Return]: { type: "start_press" },
        [Qt.Key_Enter]: { type: "start_press" },
        [Qt.Key_C]: { type: "dispatch", action: "ToggleShellColor" },
        [Qt.Key_Y]: { type: "dispatch", action: "ToggleShellColor" },
        [Qt.Key_M]: { type: "dispatch", action: "ToggleMusic" },
        [Qt.Key_F8]: { type: "dispatch", action: "ToggleBot" },
        [Qt.Key_F9]: { type: "dispatch", action: "ToggleBotPanel" },
        [Qt.Key_Back]: { type: "dispatch", action: "Back" },
        [Qt.Key_Escape]: { type: "dispatch", action: "Escape" },
        [Qt.Key_F6]: { type: "dispatch", action: "ToggleIconLab" },
        [Qt.Key_F7]: { type: "static_cycle", step: 1 },
        [Qt.Key_Shift]: { type: "select_press" }
    })
    readonly property var releasedKeyActions: ({
        [Qt.Key_A]: { type: "primary_release" },
        [Qt.Key_Z]: { type: "primary_release" },
        [Qt.Key_B]: { type: "secondary_release" },
        [Qt.Key_X]: { type: "secondary_release" },
        [Qt.Key_S]: { type: "start_release" },
        [Qt.Key_Space]: { type: "start_release" },
        [Qt.Key_Return]: { type: "start_release" },
        [Qt.Key_Enter]: { type: "start_release" },
        [Qt.Key_Shift]: { type: "select_release" }
    })

    function dispatchAction(action) {
        if (!action || action === "") {
            controller.logButton("router", "action", "failed", "empty")
            return
        }
        controller.logAction("dispatch", action)
        controller.inputPressController.beforeDispatch(action)
        controller.actionRouter.route(action)
    }

    function logInputSummary(source, message) {
        if (!controller.logButtons) {
            return
        }
        controller.uiLogger.inputSummary(`${source}: ${message}`)
    }

    function logInputDebug(source, message) {
        if (!controller.detailedButtonLogs) {
            return
        }
        controller.uiLogger.inputDebug(`${source}: ${message}`)
    }

    function logButton(source, name, phase, detail) {
        if (!controller.logButtons) {
            return
        }
        if (controller.detailedButtonLogs) {
            const suffix = detail ? ` ${detail}` : ""
            controller.logInputDebug(source, `${name} ${phase}${suffix}`)
            return
        }
        if (phase === "dispatch" || phase === "semantic" || phase === "ignored" ||
                phase === "failed") {
            const suffix = detail ? ` ${detail}` : ""
            controller.logInputSummary(source, `${name} ${phase}${suffix}`)
        }
    }

    function logAction(source, action) {
        if (!controller.logButtons) {
            return
        }
        if (controller.detailedButtonLogs) {
            controller.logInputDebug(source, `action dispatch ${action}`)
            return
        }
        controller.logInputSummary(source, `action ${action}`)
    }

    function keyName(key) {
        switch (key) {
        case Qt.Key_Up:
            return "Up"
        case Qt.Key_Down:
            return "Down"
        case Qt.Key_Left:
            return "Left"
        case Qt.Key_Right:
            return "Right"
        case Qt.Key_A:
            return "A"
        case Qt.Key_B:
            return "B"
        case Qt.Key_C:
            return "C"
        case Qt.Key_M:
            return "M"
        case Qt.Key_Return:
            return "Return"
        case Qt.Key_Enter:
            return "Enter"
        case Qt.Key_S:
            return "S"
        case Qt.Key_Space:
            return "Space"
        case Qt.Key_Shift:
            return "Shift"
        case Qt.Key_X:
            return "X"
        case Qt.Key_Y:
            return "Y"
        case Qt.Key_Z:
            return "Z"
        case Qt.Key_Back:
            return "Back"
        case Qt.Key_Escape:
            return "Escape"
        case Qt.Key_F6:
            return "F6"
        case Qt.Key_F7:
            return "F7"
        case Qt.Key_F8:
            return "F8"
        case Qt.Key_F9:
            return "F9"
        default:
            return `Key(${key})`
        }
    }

    function setDpadPressed(dx, dy) {
        controller.shellBridge.setDirectionPressed(dx, dy)
    }

    function clearDirectionVisuals() {
        controller.shellBridge.clearDirectionPressed()
    }

    function actionForDirection(dx, dy) {
        if (dy < 0) {
            return controller.actionMap[controller.directionActionByAxis.up]
        }
        if (dy > 0) {
            return controller.actionMap[controller.directionActionByAxis.down]
        }
        if (dx < 0) {
            return controller.actionMap[controller.directionActionByAxis.left]
        }
        if (dx > 0) {
            return controller.actionMap[controller.directionActionByAxis.right]
        }
        return ""
    }

    function handleDirection(dx, dy) {
        const action = controller.actionForDirection(dx, dy)
        if (action !== "") {
            controller.logButton("shell", "dpad", "press", `${dx},${dy} -> ${action}`)
            controller.commandController.dispatch(action)
        }
        controller.setDpadPressed(dx, dy)
    }

    function handlePrimaryAction() {
        controller.logButton("shell", "primary", "press")
        if (controller.inputPressController.confirmSaveClear()) {
            return
        }
        if (controller.debugController.handleEasterInput("A")) {
            return
        }
        controller.commandController.dispatch(controller.actionMap.Primary)
    }

    function handleSecondaryAction() {
        controller.logButton("shell", "secondary", "press")
        if (controller.debugController.handleEasterInput("B")) {
            return
        }
        controller.commandController.dispatch(controller.actionMap.Secondary)
    }

    function handleStartAction() {
        controller.logButton("shell", "start", "dispatch")
        if (controller.iconDebugMode) {
            return
        }
        controller.commandController.dispatch(controller.actionMap.Start)
    }

    function handleSelectShortAction() {
        controller.logButton("shell", "select", "dispatch", "short")
        controller.inputPressController.triggerSelectShort()
    }

    function handleBackAction() {
        controller.logButton("shell", "back", "dispatch")
        if (controller.iconDebugMode) {
            controller.debugController.exitIconLab()
            return
        }
        controller.commandController.dispatch(controller.actionMap.Back)
    }

    function cancelSaveClearConfirm(showToast) {
        controller.inputPressController.cancelSaveClearConfirm(showToast)
    }

    function handleShellBridgeDirection(dx, dy) {
        const action = controller.actionForDirection(dx, dy)
        if (action !== "") {
            controller.logButton("shell", "dpad", "press", `${dx},${dy} -> ${action}`)
            controller.dispatchAction(action)
        }
    }

    function handlePressedDispatch(actionKey) {
        const action = controller.actionMap[actionKey]
        if (!action || action === "") {
            controller.logButton("keyboard", "key", "failed", actionKey)
            return
        }
        controller.logButton("keyboard", "key", "dispatch", actionKey)
        controller.dispatchAction(action)
    }

    function handlePrimaryPressed() {
        controller.logButton("keyboard", "primary", "press")
        controller.shellBridge.primaryPressed = true
        controller.dispatchAction(controller.actionMap.Primary)
    }

    function handleSecondaryPressed() {
        controller.logButton("keyboard", "secondary", "press")
        controller.shellBridge.secondaryPressed = true
        controller.dispatchAction(controller.actionMap.Secondary)
    }

    function handleStartPressed() {
        controller.logButton("keyboard", "start", "press")
        controller.shellBridge.startHeld = true
        controller.inputPressController.onStartPressed()
        controller.dispatchAction(controller.actionMap.Start)
    }

    function handleSelectPressed() {
        if (controller.inputPressController.selectKeyDown) {
            controller.logButton("keyboard", "select", "ignored", "already_down")
            return
        }
        controller.logButton("keyboard", "select", "press")
        controller.inputPressController.selectKeyDown = true
        controller.shellBridge.selectHeld = true
        controller.inputPressController.onSelectPressed()
    }

    function handleStartReleased() {
        controller.logButton("keyboard", "start", "release")
        controller.shellBridge.startHeld = false
        controller.inputPressController.onStartReleased()
    }

    function handleSelectReleased() {
        controller.logButton("keyboard", "select", "release")
        controller.inputPressController.selectKeyDown = false
        controller.shellBridge.selectHeld = false
        controller.inputPressController.onSelectReleased()
        controller.dispatchAction(controller.actionMap.SelectShort)
    }

    function applyKeyAction(actionSpec) {
        if (!actionSpec) {
            return
        }
        switch (actionSpec.type) {
        case "dispatch":
            controller.handlePressedDispatch(actionSpec.action)
            break
        case "primary_press":
            controller.handlePrimaryPressed()
            break
        case "secondary_press":
            controller.handleSecondaryPressed()
            break
        case "start_press":
            controller.handleStartPressed()
            break
        case "select_press":
            controller.handleSelectPressed()
            break
        case "static_cycle":
            controller.debugController.cycleStaticScene(actionSpec.step)
            break
        case "primary_release":
            controller.shellBridge.primaryPressed = false
            break
        case "secondary_release":
            controller.shellBridge.secondaryPressed = false
            break
        case "start_release":
            controller.handleStartReleased()
            break
        case "select_release":
            controller.handleSelectReleased()
            break
        }
    }

    function handleShellColorToggle() {
        controller.logButton("shell", "shell-color", "dispatch")
        controller.commandController.dispatch("feedback_ui")
        controller.commandController.dispatch("toggle_shell_color")
    }

    function handleVolumeRequested(value, withHaptic) {
        controller.logButton("shell", "volume", "dispatch", `${Math.round(value * 100)}%`)
        controller.audioSettingsViewModel.volume = value
        if (controller.screen) {
            controller.screen.showVolumeOSD(value)
        }
        if (withHaptic) {
            controller.commandController.dispatch("feedback_light")
        }
    }

    function handleKeyPressed(event) {
        if (event.isAutoRepeat) {
            return
        }
        controller.logButton("keyboard",
                             controller.keyName(event.key),
                             "press",
                             controller.detailedButtonLogs ? "raw" : "")
        controller.applyKeyAction(controller.pressedKeyActions[event.key])
    }

    function handleKeyReleased(event) {
        if (event.isAutoRepeat) {
            return
        }
        controller.logButton("keyboard",
                             controller.keyName(event.key),
                             "release",
                             controller.detailedButtonLogs ? "raw" : "")
        controller.clearDirectionVisuals()
        controller.applyKeyAction(controller.releasedKeyActions[event.key])
    }

    property var shellBridgeConnections: Connections {
        target: controller.shellBridge

        function onDirectionTriggered(dx, dy) {
            controller.handleShellBridgeDirection(dx, dy)
        }

        function onPrimaryTriggered() {
            controller.handlePrimaryAction()
        }

        function onSecondaryTriggered() {
            controller.handleSecondaryAction()
        }

        function onSelectPressed() {
            controller.handleSelectPressed()
        }

        function onSelectReleased() {
            controller.handleSelectReleased()
        }

        function onSelectTriggered() {
            controller.handleSelectShortAction()
        }

        function onStartPressed() {
            controller.handleStartPressed()
        }

        function onStartReleased() {
            controller.handleStartReleased()
        }

        function onStartTriggered() {
            controller.handleStartAction()
        }

        function onShellColorToggleTriggered() {
            controller.handleShellColorToggle()
        }

        function onVolumeRequested(value, withHaptic) {
            controller.handleVolumeRequested(value, withHaptic)
        }
    }

    property var sessionRenderConnections: Connections {
        target: controller.sessionRenderViewModel

        function onStateChanged() {
            if (controller.sessionRenderViewModel.state !== AppState.StartMenu) {
                controller.cancelSaveClearConfirm(false)
            }
        }
    }
}
