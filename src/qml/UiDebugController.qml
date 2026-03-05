import QtQuick
import NenoSerpent 1.0

QtObject {
    id: controller

    property var commandController
    property var inputInjector
    property var uiLogger
    property var actionMap: ({})
    property int currentState: AppState.Splash
    property bool iconDebugMode: false
    property string staticDebugScene: ""
    property var staticDebugOptions: ({})
    property var screen
    property var inputController
    property var clearDirectionVisuals
    property var stateOwner

    readonly property var konamiSequence: ["U", "U", "D", "D", "L", "R", "L", "R", "B", "A"]
    readonly property var debugStateTokens: ({
        DBG_GAMEOVER: { state: AppState.GameOver, osd: "DBG: GAMEOVER" },
        DBG_REPLAY: { state: AppState.Replaying, osd: "DBG: REPLAY" },
        DBG_CATALOG: { state: AppState.Library, osd: "DBG: CATALOG" },
        DBG_ACHIEVEMENTS: { state: AppState.MedalRoom, osd: "DBG: ACHIEVEMENTS" }
    })
    readonly property var staticSceneAliases: ({
        DBG_STATIC_BOOT: "boot",
        DBG_STATIC_GAME: "game",
        DBG_STATIC_REPLAY: "replay",
        DBG_STATIC_CHOICE: "choice",
        DBG_STATIC_OSD: "osd_text",
        DBG_STATIC_VOL: "osd_volume",
        DBG_STATIC_OFF: "",
        STATIC_BOOT: "boot",
        STATIC_GAME: "game",
        STATIC_REPLAY: "replay",
        STATIC_CHOICE: "choice",
        STATIC_OSD: "osd_text",
        STATIC_VOL: "osd_volume",
        STATIC_OFF: ""
    })
    readonly property var injectedActionTokens: ({
        UP: "NavUp",
        U: "NavUp",
        DOWN: "NavDown",
        D: "NavDown",
        LEFT: "NavLeft",
        L: "NavLeft",
        RIGHT: "NavRight",
        R: "NavRight",
        A: "Primary",
        PRIMARY: "Primary",
        OK: "Primary",
        B: "Secondary",
        SECONDARY: "Secondary",
        START: "Start",
        SELECT: "SelectShort",
        BACK: "Back",
        ESC: "Escape",
        ESCAPE: "Escape",
        F6: "ToggleIconLab",
        ICON: "ToggleIconLab",
        COLOR: "ToggleShellColor",
        SHELL: "ToggleShellColor",
        MUSIC: "ToggleMusic",
        BOTMODE: "ToggleBot",
        BOTPANEL: "ToggleBotPanel"
    })
    readonly property var staticSceneOptionHandlers: ({
        BUFF: function(sceneName, options, value, helpers) {
            const buffType = helpers.parseBoundedInt(value, 1, 9)
            if (buffType !== null) {
                options.buffType = buffType
            }
        },
        SCORE: function(sceneName, options, value, helpers) {
            const score = helpers.parseBoundedInt(value, 0)
            if (score !== null) {
                options.score = score
            }
        },
        HI: function(sceneName, options, value, helpers) {
            const highScore = helpers.parseBoundedInt(value, 0)
            if (highScore !== null) {
                options.highScore = highScore
            }
        },
        REMAIN: function(sceneName, options, value, helpers) {
            const remaining = helpers.parseBoundedInt(value, 0)
            if (remaining !== null) {
                options.buffRemaining = remaining
            }
        },
        TOTAL: function(sceneName, options, value, helpers) {
            const total = helpers.parseBoundedInt(value, 1)
            if (total !== null) {
                options.buffTotal = total
            }
        },
        INDEX: function(sceneName, options, value, helpers) {
            const choiceIndex = helpers.parseBoundedInt(value, 0)
            if (choiceIndex !== null) {
                options.choiceIndex = choiceIndex
            }
        },
        TYPES: function(sceneName, options, value, helpers) {
            options.choiceTypes = helpers.parseChoiceTypes(value)
        },
        CHOICES: function(sceneName, options, value, helpers) {
            options.choiceTypes = helpers.parseChoiceTypes(value)
        },
        TITLE: function(sceneName, options, value, helpers) {
            if (sceneName === "boot") {
                options.bootTitle = helpers.decodeLabel(value)
            } else if (sceneName === "choice") {
                options.choiceTitle = helpers.decodeLabel(value)
            }
        },
        SUBTITLE: function(sceneName, options, value, helpers) {
            if (sceneName === "boot") {
                options.bootSubtitle = helpers.decodeLabel(value)
            } else if (sceneName === "choice") {
                options.choiceSubtitle = helpers.decodeLabel(value)
            }
        },
        LOAD: function(sceneName, options, value, helpers) {
            options.bootLoadLabel = helpers.decodeLabel(value)
        },
        LOADLABEL: function(sceneName, options, value, helpers) {
            options.bootLoadLabel = helpers.decodeLabel(value)
        },
        PROGRESS: function(sceneName, options, value, helpers) {
            const progress = helpers.parseProgress(value)
            if (progress !== null) {
                options.bootLoadProgress = progress
            }
        },
        LOADPROGRESS: function(sceneName, options, value, helpers) {
            const progress = helpers.parseProgress(value)
            if (progress !== null) {
                options.bootLoadProgress = progress
            }
        },
        FOOTER: function(sceneName, options, value, helpers) {
            options.choiceFooterHint = helpers.decodeLabel(value)
        },
        FOOTERHINT: function(sceneName, options, value, helpers) {
            options.choiceFooterHint = helpers.decodeLabel(value)
        },
        OSD: function(sceneName, options, value, helpers) {
            options.osdText = helpers.decodeLabel(value)
        },
        OSDTEXT: function(sceneName, options, value, helpers) {
            options.osdText = helpers.decodeLabel(value)
        },
        VOLUME: function(sceneName, options, value, helpers) {
            const volume = helpers.parseProgress(value)
            if (volume !== null) {
                options.osdVolume = volume
            }
        },
        SNAKE: function(sceneName, options, value, helpers) {
            options.snakeSegments = helpers.parsePointList(value)
        },
        FOOD: function(sceneName, options, value, helpers) {
            options.foodCells = helpers.parsePointList(value).map((cell) => ({ x: cell.x, y: cell.y }))
        },
        OBSTACLES: function(sceneName, options, value, helpers) {
            options.obstacleCells = helpers.parsePointList(value).map((cell) => ({ x: cell.x, y: cell.y }))
        },
        POWERUPS: function(sceneName, options, value, helpers) {
            options.powerupCells = helpers.parsePowerupList(value)
        }
    })
    property int konamiIndex: 0

    function resetKonamiProgress() {
        controller.konamiIndex = 0
        konamiResetTimer.stop()
    }

    function toggleIconLabMode() {
        const nextEnabled = !controller.iconDebugMode
        if (controller.stateOwner) {
            controller.stateOwner.iconDebugMode = nextEnabled
            controller.stateOwner.staticDebugScene = ""
            controller.stateOwner.staticDebugOptions = ({})
        }
        controller.resetKonamiProgress()
        if (controller.screen) {
            controller.screen.showOSD(nextEnabled ? "ICON LAB ON" : "ICON LAB OFF")
        }
        if (!nextEnabled && controller.commandController) {
            controller.commandController.dispatch("state_start_menu")
        }
    }

    function parseStaticSceneOptions(sceneName, rawOptions) {
        const helpers = {
            decodeLabel(value) {
                return String(value || "").replace(/\+/g, " ").replace(/_/g, " ")
            },
            parseBoundedInt(value, min, max) {
                const parsed = Number(value)
                if (!Number.isInteger(parsed)) {
                    return null
                }
                if (parsed < min) {
                    return null
                }
                if (max !== undefined && parsed > max) {
                    return null
                }
                return parsed
            },
            parseChoiceTypes(value) {
                return String(value || "")
                    .split(/[|/]/)
                    .map((entry) => Number(entry.trim()))
                    .filter((entry) => Number.isInteger(entry) && entry >= 1 && entry <= 9)
                    .slice(0, 3)
            },
            parseProgress(value) {
                const progress = Number(value)
                if (Number.isNaN(progress)) {
                    return null
                }
                return progress > 1 ? progress / 100.0 : progress
            },
            parsePointList(value) {
                return value
                    .split(/[|/]/)
                    .map((entry) => entry.trim())
                    .filter((entry) => entry.length > 0)
                    .map((entry) => {
                        const parts = entry.split(":").map((part) => part.trim())
                        if (parts.length < 2) {
                            return null
                        }
                        const x = Number(parts[0])
                        const y = Number(parts[1])
                        if (!Number.isInteger(x) || !Number.isInteger(y)) {
                            return null
                        }
                        const point = { x, y }
                        if (parts.length >= 3) {
                            const marker = parts[2]
                            point.head = marker === "H" || marker === "h" || marker === "1"
                        }
                        return point
                    })
                    .filter((entry) => !!entry)
            },
            parsePowerupList(value) {
                return value
                    .split(/[|/]/)
                    .map((entry) => entry.trim())
                    .filter((entry) => entry.length > 0)
                    .map((entry) => {
                        const parts = entry.split(":").map((part) => part.trim())
                        if (parts.length < 2) {
                            return null
                        }
                        const x = Number(parts[0])
                        const y = Number(parts[1])
                        const type = parts.length >= 3 ? Number(parts[2]) : 1
                        if (!Number.isInteger(x) || !Number.isInteger(y) ||
                                !Number.isInteger(type) || type < 1 || type > 9) {
                            return null
                        }
                        return { x, y, type }
                    })
                    .filter((entry) => !!entry)
            }
        }

        const options = {}
        const raw = String(rawOptions || "").trim()
        if (raw.length === 0) {
            return options
        }

        const parts = raw.split(",").map((part) => part.trim()).filter((part) => part.length > 0)
        const bareValues = []

        for (const part of parts) {
            const separatorIndex = part.indexOf("=")
            if (separatorIndex < 0) {
                const bareInt = Number(part)
                if (Number.isInteger(bareInt)) {
                    bareValues.push(bareInt)
                }
                continue
            }

            const key = part.slice(0, separatorIndex).trim().toUpperCase()
            const value = part.slice(separatorIndex + 1).trim()
            const handler = controller.staticSceneOptionHandlers[key]
            if (handler) {
                handler(sceneName, options, value, helpers)
            }
        }

        if (sceneName === "choice") {
            if (!options.choiceTypes || options.choiceTypes.length === 0) {
                options.choiceTypes = bareValues
                    .filter((entry) => Number.isInteger(entry) && entry >= 1 && entry <= 9)
                    .slice(0, 3)
            }
        } else if (bareValues.length > 0) {
            const buffType = bareValues[0]
            if (buffType >= 1 && buffType <= 9) {
                options.buffType = buffType
            }
        }

        return options
    }

    function setStaticScene(sceneName, options) {
        if (controller.stateOwner) {
            controller.stateOwner.iconDebugMode = false
            controller.stateOwner.staticDebugScene = sceneName
            controller.stateOwner.staticDebugOptions = options ? options : ({})
        }
        controller.resetKonamiProgress()
        if (controller.showOsd) {
            controller.showOsd(sceneName === ""
                ? "STATIC DEBUG OFF"
                : `STATIC DEBUG: ${sceneName.toUpperCase()}`)
        }
        if (controller.uiLogger) {
            controller.uiLogger.routingSummary(sceneName === ""
                ? "static scene cleared"
                : `static scene=${sceneName}`)
        }
    }

    function cycleStaticScene(direction) {
        const scenes = ["boot", "game", "replay", "choice"]
        if (controller.staticDebugScene === "") {
            controller.setStaticScene("boot", {})
            return
        }
        let index = scenes.indexOf(controller.staticDebugScene)
        if (index < 0) {
            index = 0
        }
        index = (index + direction + scenes.length) % scenes.length
        controller.setStaticScene(scenes[index], {})
    }

    function toggleBotDebugPanel() {
        if (!controller.stateOwner) {
            return
        }
        controller.stateOwner.botDebugPanelVisible = !controller.stateOwner.botDebugPanelVisible
        controller.showDebugOsd(controller.stateOwner.botDebugPanelVisible
            ? "BOT PANEL ON"
            : "BOT PANEL OFF")
    }

    function exitIconLab() {
        if (controller.stateOwner) {
            controller.stateOwner.iconDebugMode = false
        }
        if (controller.clearDirectionVisuals) {
            controller.clearDirectionVisuals()
        }
        controller.resetKonamiProgress()
        if (controller.commandController) {
            controller.commandController.dispatch("state_start_menu")
        }
        if (controller.showOsd) {
            controller.showOsd("ICON LAB OFF")
        }
    }

    function feedKonamiToken(token) {
        if (token === controller.konamiSequence[controller.konamiIndex]) {
            controller.konamiIndex += 1
            konamiResetTimer.restart()
            if (controller.konamiIndex >= controller.konamiSequence.length) {
                const nextEnabled = !controller.iconDebugMode
                controller.konamiIndex = 0
                konamiResetTimer.stop()
                if (controller.stateOwner) {
                    controller.stateOwner.iconDebugMode = nextEnabled
                }
                if (controller.showOsd) {
                    controller.showOsd(nextEnabled ? "ICON LAB ON" : "ICON LAB OFF")
                }
                if (!nextEnabled && controller.commandController) {
                    controller.commandController.dispatch("state_start_menu")
                }
                return "toggle"
            }
            return "progress"
        }
        controller.konamiIndex = token === controller.konamiSequence[0] ? 1 : 0
        if (controller.konamiIndex > 0) {
            konamiResetTimer.restart()
        } else {
            konamiResetTimer.stop()
        }
        return "mismatch"
    }

    function handleEasterInput(token) {
        const trackEaster = controller.iconDebugMode || controller.currentState === AppState.Paused
        if (!trackEaster) {
            return false
        }

        if (controller.iconDebugMode && token === "B" && controller.konamiIndex === 0) {
            controller.exitIconLab()
            return true
        }

        const previousIndex = controller.konamiIndex
        const status = controller.feedKonamiToken(token)
        if (controller.iconDebugMode) {
            return true
        }
        if (status === "toggle") {
            return true
        }
        return previousIndex > 0
    }

    function showDebugOsd(text) {
        if (controller.screen) {
            controller.screen.showOSD(text)
        }
    }

    function activateStaticScene(sceneName, rawOptions) {
        const options = rawOptions === undefined
            ? {}
            : controller.parseStaticSceneOptions(sceneName, rawOptions)
        controller.setStaticScene(sceneName, options)
        return true
    }

    function routeMappedDebugStateToken(token) {
        const entry = controller.debugStateTokens[token]
        if (!entry) {
            return false
        }
        controller.commandController.requestStateChange(entry.state)
        controller.showDebugOsd(entry.osd)
        return true
    }

    function routeMappedStaticSceneToken(token, rawOptions) {
        if (!(token in controller.staticSceneAliases)) {
            return false
        }
        const sceneName = controller.staticSceneAliases[token]
        if (rawOptions !== undefined && token.startsWith("DBG_STATIC_") && sceneName !== "") {
            return controller.activateStaticScene(sceneName, rawOptions)
        }
        controller.setStaticScene(sceneName, {})
        return true
    }

    function dispatchInjectedAlias(token) {
        const actionKey = controller.injectedActionTokens[token]
        if (!actionKey) {
            return false
        }
        if (controller.inputController) {
            controller.inputController.dispatchAction(controller.actionMap[actionKey])
        }
        return true
    }

    function routeDebugToken(token) {
        if (token === "DBG_BOT_PANEL") {
            controller.toggleBotDebugPanel()
            return true
        }
        if (token === "DBG_BOT_MODE") {
            controller.commandController.cycleBotMode()
            return true
        }
        if (token === "DBG_BOT_RESET") {
            controller.commandController.resetBotModeDefaults()
            return true
        }
        if (token.startsWith("DBG_BOT_PARAM:")) {
            const payload = token.slice("DBG_BOT_PARAM:".length)
            const parts = payload.split(",").map((part) => part.trim()).filter((part) => part.length > 0)
            let applied = false
            for (const part of parts) {
                const separator = part.indexOf("=")
                if (separator <= 0 || separator >= part.length - 1) {
                    continue
                }
                const key = part.slice(0, separator).trim()
                const value = Number(part.slice(separator + 1).trim())
                if (!Number.isInteger(value)) {
                    continue
                }
                if (controller.commandController.setBotParam(key, value)) {
                    applied = true
                }
            }
            if (!applied) {
                controller.showDebugOsd("DBG BOT PARAM INVALID")
            }
            return true
        }
        if (token.startsWith("DBG_CHOICE")) {
            let choiceTypes = []
            const separatorIndex = token.indexOf(":")
            if (separatorIndex >= 0 && separatorIndex + 1 < token.length) {
                choiceTypes = token
                    .slice(separatorIndex + 1)
                    .split(",")
                    .map((part) => Number(part.trim()))
                    .filter((value) => Number.isInteger(value) && value >= 1 && value <= 9)
            }
            controller.commandController.seedChoicePreview(choiceTypes)
            controller.showDebugOsd(choiceTypes.length > 0
                ? `DBG: CHOICE ${choiceTypes.join("/")}`
                : "DBG: CHOICE")
            return true
        }
        if (token === "DBG_MENU") {
            if (controller.iconDebugMode) {
                controller.exitIconLab()
            } else {
                controller.commandController.dispatch("state_start_menu")
                controller.showDebugOsd("DBG: MENU")
            }
            return true
        }
        if (token === "DBG_PLAY") {
            controller.commandController.dispatch("state_start_menu")
            if (controller.inputController) {
                controller.inputController.dispatchAction(controller.actionMap.Start)
            }
            controller.showDebugOsd("DBG: PLAY")
            return true
        }
        if (token === "DBG_PAUSE") {
            controller.commandController.dispatch("state_start_menu")
            if (controller.inputController) {
                controller.inputController.dispatchAction(controller.actionMap.Start)
                controller.inputController.dispatchAction(controller.actionMap.Start)
            }
            controller.showDebugOsd("DBG: PAUSE")
            return true
        }
        if (controller.routeMappedDebugStateToken(token)) {
            return true
        }
        if (token === "DBG_REPLAY_BUFF") {
            controller.commandController.seedReplayBuffPreview()
            return true
        }
        if (token === "DBG_ICONS") {
            if (!controller.iconDebugMode) {
                controller.toggleIconLabMode()
            }
            controller.showDebugOsd("DBG: ICON LAB")
            return true
        }
        if (token.startsWith("DBG_STATIC_")) {
            const separatorIndex = token.indexOf(":")
            const baseToken = separatorIndex >= 0 ? token.slice(0, separatorIndex) : token
            const rawOptions = separatorIndex >= 0 ? token.slice(separatorIndex + 1) : ""
            return controller.routeMappedStaticSceneToken(baseToken, rawOptions)
        }
        return false
    }

    function routeInjectedToken(rawToken) {
        const token = String(rawToken).trim().toUpperCase()
        if (controller.uiLogger) {
            controller.uiLogger.inputDebug(`inject token=${token}`)
        }
        if (controller.routeDebugToken(token)) {
            return true
        }
        if (controller.dispatchInjectedAlias(token)) {
            return true
        }
        if (token === "PALETTE" || token === "NEXT_PALETTE") {
            controller.commandController.dispatch("next_palette")
            return true
        }
        if (controller.routeMappedStaticSceneToken(token)) {
            return true
        }
        controller.showDebugOsd(`UNKNOWN INPUT: ${token}`)
        if (controller.uiLogger) {
            controller.uiLogger.injectWarning(`unknown token=${token}`)
        }
        return false
    }

    property Timer konamiResetTimer: Timer {
        interval: 1400
        repeat: false
        onTriggered: controller.konamiIndex = 0
    }

    property var inputInjectorConnection: Connections {
        target: controller.inputInjector

        function onActionInjected(action) {
            controller.routeInjectedToken(action)
        }
    }
}
