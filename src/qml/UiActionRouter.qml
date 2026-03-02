import QtQuick
import NenoSerpent 1.0

QtObject {
    id: router

    property var commandController
    property var inputController
    property var debugController
    property var uiLogger
    property var screen
    property int currentState: AppState.Splash
    property var actionMap: ({})
    property bool iconDebugMode: false
    property string staticDebugScene: ""

    readonly property string modeOverlay: "overlay"
    readonly property string modePage: "page"
    readonly property string modeGame: "game"
    readonly property string modeShell: "shell"

    function route(action) {
        if (router.uiLogger) {
            router.uiLogger.routingDebug(`route action=${action}`)
        }
        if (routeGlobal(action)) {
            return true
        }
        if (router.staticDebugScene !== "") {
            return routeStaticScene(action)
        }
        if (router.iconDebugMode) {
            return routeIconDebug(action)
        }
        const handled = routeMode(router.modeForState(router.currentState), action)
        if (!handled && router.uiLogger) {
            router.uiLogger.routingSummary(`ignored action=${action} state=${router.currentState}`)
        }
        return handled
    }

    function modeForState(state) {
        if (state === AppState.Paused || state === AppState.GameOver ||
                state === AppState.Replaying || state === AppState.ChoiceSelection) {
            return router.modeOverlay
        }
        if (state === AppState.Library || state === AppState.MedalRoom) {
            return router.modePage
        }
        if (state === AppState.Playing) {
            return router.modeGame
        }
        return router.modeShell
    }

    function perform(callback) {
        if (callback) {
            callback()
        }
        return true
    }

    function dispatch(action) {
        if (router.uiLogger) {
            router.uiLogger.routingDebug(`dispatch action=${action}`)
        }
        router.commandController.dispatch(action)
        return true
    }

    function dispatchBack() {
        return dispatch(router.actionMap.Back)
    }

    function performInput(methodName) {
        if (!router.inputController || !router.inputController[methodName]) {
            if (router.uiLogger) {
                router.uiLogger.routingSummary(`missing input handler=${methodName}`)
            }
            return false
        }
        return perform(() => router.inputController[methodName]())
    }

    function clearStaticScene() {
        if (!router.debugController) {
            return false
        }
        return perform(() => router.debugController.setStaticScene("", {}))
    }

    function directionForAction(action) {
        if (action === router.actionMap.NavUp) {
            return { dx: 0, dy: -1, token: "U" }
        }
        if (action === router.actionMap.NavDown) {
            return { dx: 0, dy: 1, token: "D" }
        }
        if (action === router.actionMap.NavLeft) {
            return { dx: -1, dy: 0, token: "L" }
        }
        if (action === router.actionMap.NavRight) {
            return { dx: 1, dy: 0, token: "R" }
        }
        return null
    }

    function routeDirection(action) {
        const direction = router.directionForAction(action)
        if (!direction) {
            return false
        }

        if (router.debugController && router.debugController.handleEasterInput(direction.token)) {
            if (router.inputController) {
                router.inputController.clearDirectionVisuals()
            }
            return true
        }
        if (router.inputController) {
            router.inputController.handleDirection(direction.dx, direction.dy)
        }
        return true
    }

    function routeIconDirection(action) {
        const direction = router.directionForAction(action)
        if (!direction) {
            return false
        }

        if (router.debugController) {
            router.debugController.handleEasterInput(direction.token)
        }
        if (router.screen) {
            router.screen.iconLabMove(direction.dx, direction.dy)
        }
        if (router.inputController) {
            router.inputController.setDpadPressed(direction.dx, direction.dy)
        }
        return true
    }

    function routeGlobal(action) {
        switch (action) {
        case router.actionMap.ToggleIconLab:
            return router.debugController
                ? perform(router.debugController.toggleIconLabMode)
                : false
        case router.actionMap.ToggleBotPanel:
            return router.debugController
                ? perform(router.debugController.toggleBotDebugPanel)
                : false
        case router.actionMap.ToggleShellColor:
            return dispatch("toggle_shell_color")
        case router.actionMap.ToggleMusic:
            return dispatch("toggle_music")
        case router.actionMap.ToggleBot:
            return dispatch("toggle_bot")
        case router.actionMap.Escape:
            if (router.iconDebugMode) {
                return router.debugController
                    ? perform(router.debugController.exitIconLab)
                    : false
            }
            if (router.staticDebugScene !== "") {
                return clearStaticScene()
            }
            return dispatch("quit")
        default:
            return false
        }
    }

    function routeIconDebug(action) {
        if (routeIconDirection(action)) {
            return true
        }
        switch (action) {
        case router.actionMap.Secondary:
        case router.actionMap.Back:
            return router.debugController
                ? perform(router.debugController.exitIconLab)
                : false
        case router.actionMap.Primary:
            return performInput("handlePrimaryAction")
        case router.actionMap.Start:
        case router.actionMap.SelectShort:
            return true
        default:
            return false
        }
    }

    function routeStaticScene(action) {
        switch (action) {
        case router.actionMap.NavUp:
        case router.actionMap.NavLeft:
            return router.debugController
                ? perform(() => router.debugController.cycleStaticScene(-1))
                : false
        case router.actionMap.NavDown:
        case router.actionMap.NavRight:
            return router.debugController
                ? perform(() => router.debugController.cycleStaticScene(1))
                : false
        case router.actionMap.Primary:
        case router.actionMap.Secondary:
        case router.actionMap.Start:
        case router.actionMap.SelectShort:
        case router.actionMap.Back:
            return clearStaticScene()
        default:
            return true
        }
    }

    function routeMode(mode, action) {
        if (routeDirection(action)) {
            return true
        }

        switch (mode) {
        case router.modeOverlay:
            return routeOverlayAction(action)
        case router.modePage:
            return routePageAction(action)
        case router.modeGame:
            return routeGameAction(action)
        default:
            return routeShellAction(action)
        }
    }

    function routeOverlayAction(action) {
        switch (action) {
        case router.actionMap.Start:
            return performInput("handleStartAction")
        case router.actionMap.Primary:
            if (router.currentState === AppState.Paused && router.debugController) {
                router.debugController.handleEasterInput("A")
            }
            return true
        case router.actionMap.Secondary:
            if (router.currentState === AppState.Paused && router.debugController) {
                router.debugController.handleEasterInput("B")
            }
            return true
        case router.actionMap.SelectShort:
        case router.actionMap.Back:
            return dispatchBack()
        default:
            return false
        }
    }

    function routePageAction(action) {
        switch (action) {
        case router.actionMap.Secondary:
        case router.actionMap.Back:
        case router.actionMap.SelectShort:
            return dispatchBack()
        case router.actionMap.Primary:
        case router.actionMap.Start:
            return true
        default:
            return false
        }
    }

    function routeGameAction(action) {
        switch (action) {
        case router.actionMap.Primary:
            return true
        case router.actionMap.Secondary:
            return performInput("handleSecondaryAction")
        case router.actionMap.Start:
            return performInput("handleStartAction")
        case router.actionMap.SelectShort:
            return performInput("handleSelectShortAction")
        case router.actionMap.Back:
            return performInput("handleBackAction")
        default:
            return false
        }
    }

    function routeShellAction(action) {
        switch (action) {
        case router.actionMap.Primary:
            if (router.inputController &&
                    router.inputController.inputPressController.saveClearConfirmPending) {
                return performInput("handlePrimaryAction")
            }
            return true
        case router.actionMap.Secondary:
            return performInput("handleSecondaryAction")
        case router.actionMap.Back:
            return performInput("handleBackAction")
        case router.actionMap.Start:
            return performInput("handleStartAction")
        case router.actionMap.SelectShort:
            return performInput("handleSelectShortAction")
        default:
            return false
        }
    }
}
