import QtQuick
import NenoSerpent 1.0

Item {
    id: gameWorld
    property bool active: false
    property int currentState: AppState.Splash
    property int boardWidth: 1
    property int boardHeight: 1
    property var ghostModel: []
    property var snakeModel: []
    property bool shieldActive: false
    property var obstacleModel: []
    property string currentLevelName: ""
    property var foodPos: ({ x: 0, y: 0 })
    property var powerUpPos: ({ x: -1, y: -1 })
    property int powerUpType: 0
    property int activeBuff: 0
    property int buffTicksRemaining: 0
    property int buffTicksTotal: 0
    property real elapsed: 0
    property string gameFont: ""
    property var menuColor
    property color gameBg: "black"
    property color gamePanel: "black"
    property color gameInk: "white"
    property color gameSubInk: "gray"
    property color gameBorder: "gray"
    property color gameObstacleFill: "dimgray"
    property color gameObstaclePulse: "black"
    property var drawFoodSymbol
    property var drawPowerSymbol
    property var powerColor
    property var buffName
    property var rarityTier
    property var rarityName
    property var rarityColor
    property var readableText

    anchors.fill: parent
    visible: active
    readonly property real cellW: width / Math.max(1, boardWidth)
    readonly property real cellH: height / Math.max(1, boardHeight)
    readonly property int layerObstacle: 12
    readonly property int layerFood: 20
    readonly property int layerPowerUp: 30
    readonly property int layerBuffPanel: 40

    Repeater {
        model: ghostModel
        visible: gameWorld.currentState === AppState.Playing
        delegate: Rectangle {
            x: modelData.x * gameWorld.cellW
            y: modelData.y * gameWorld.cellH
            width: gameWorld.cellW
            height: gameWorld.cellH
            color: gameWorld.gameSubInk
            opacity: 0.2
        }
    }

    Repeater {
        model: snakeModel
        delegate: Rectangle {
            x: model.pos.x * gameWorld.cellW
            y: model.pos.y * gameWorld.cellH
            width: gameWorld.cellW
            height: gameWorld.cellH
            opacity: gameWorld.activeBuff === PowerUpId.Ghost ? 0.52 : 1.0
            color: gameWorld.activeBuff === 6
                   ? (Math.floor(gameWorld.elapsed * 10) % 2 === 0 ? powerColor(6) : gameWorld.gameInk)
                   : (index === 0 ? gameWorld.gameInk : Qt.darker(gameWorld.gameSubInk, 1.04))
            radius: index === 0 ? 3 : 1
            border.color: index === 0 ? gameWorld.gameBorder : Qt.rgba(gameWorld.gameBorder.r,
                                                                         gameWorld.gameBorder.g,
                                                                         gameWorld.gameBorder.b,
                                                                         0.55)
            border.width: 1

            Rectangle {
                anchors.fill: parent
                anchors.margins: index === 0 ? 2 : 1
                radius: Math.max(1, parent.radius - 1)
                color: "transparent"
                border.color: index === 0
                              ? Qt.rgba(gameWorld.gamePanel.r,
                                        gameWorld.gamePanel.g,
                                        gameWorld.gamePanel.b,
                                        0.34)
                              : Qt.rgba(gameWorld.gameInk.r,
                                        gameWorld.gameInk.g,
                                        gameWorld.gameInk.b,
                                        0.14)
                border.width: 1
            }

            Rectangle {
                visible: index === 0
                width: Math.max(2, parent.width * 0.24)
                height: width
                radius: width / 2
                x: parent.width * 0.26
                y: parent.height * 0.22
                color: gameWorld.gamePanel
            }

            Rectangle {
                visible: index === 0
                width: Math.max(2, parent.width * 0.24)
                height: width
                radius: width / 2
                x: parent.width * 0.56
                y: parent.height * 0.22
                color: gameWorld.gamePanel
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: -2
                border.color: powerColor(4)
                border.width: 1
                radius: parent.radius + 2
                visible: index === 0 && gameWorld.shieldActive
            }
        }
    }

    Repeater {
        model: obstacleModel
        delegate: Rectangle {
            x: modelData.x * gameWorld.cellW
            y: modelData.y * gameWorld.cellH
            width: gameWorld.cellW
            height: gameWorld.cellH
            color: gameWorld.currentLevelName === "Dynamic Pulse" || gameWorld.currentLevelName === "Crossfire" || gameWorld.currentLevelName === "Shifting Box"
                   ? ((Math.floor(gameWorld.elapsed * 8) % 2 === 0)
                      ? gameWorld.gameObstaclePulse
                      : gameWorld.gameObstacleFill)
                   : gameWorld.gameObstacleFill
            border.color: gameWorld.gameBorder
            border.width: 1
            radius: 1
            z: gameWorld.layerObstacle

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                color: "transparent"
                border.color: Qt.rgba(gameWorld.gamePanel.r,
                                      gameWorld.gamePanel.g,
                                      gameWorld.gamePanel.b,
                                      0.24)
                border.width: 1
            }

            Rectangle {
                width: Math.max(1, parent.width * 0.16)
                height: parent.height - 4
                x: parent.width * 0.22
                y: 2
                color: Qt.rgba(gameWorld.gameBorder.r,
                               gameWorld.gameBorder.g,
                               gameWorld.gameBorder.b,
                               0.26)
            }

            Rectangle {
                width: Math.max(1, parent.width * 0.16)
                height: parent.height - 4
                x: parent.width * 0.62
                y: 2
                color: Qt.rgba(gameWorld.gamePanel.r,
                               gameWorld.gamePanel.g,
                               gameWorld.gamePanel.b,
                               0.18)
            }
        }
    }

    Item {
        x: gameWorld.foodPos.x * gameWorld.cellW
        y: gameWorld.foodPos.y * gameWorld.cellH
        width: gameWorld.cellW
        height: gameWorld.cellH
        z: gameWorld.layerFood

        Canvas {
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                drawFoodSymbol(ctx, width, height)
            }
            Component.onCompleted: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }
    }

    Item {
        visible: gameWorld.powerUpPos.x !== -1
        x: gameWorld.powerUpPos.x * gameWorld.cellW
        y: gameWorld.powerUpPos.y * gameWorld.cellH
        width: gameWorld.cellW
        height: gameWorld.cellH
        z: gameWorld.layerPowerUp

        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 4
            height: parent.height + 4
            radius: 4
            color: "transparent"
            border.color: powerColor(gameWorld.powerUpType)
            border.width: 1
            opacity: 0.85
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 1
            height: parent.height + 1
            radius: 3
            color: Qt.rgba(gameWorld.gamePanel.r, gameWorld.gamePanel.g, gameWorld.gamePanel.b, 0.90)
            border.color: Qt.rgba(powerColor(gameWorld.powerUpType).r,
                                  powerColor(gameWorld.powerUpType).g,
                                  powerColor(gameWorld.powerUpType).b,
                                  0.30)
            border.width: 1
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 6
            height: parent.height + 6
            radius: 6
            color: "transparent"
            border.color: powerColor(gameWorld.powerUpType)
            border.width: 1
            opacity: (Math.floor(gameWorld.elapsed * 8) % 2 === 0) ? 0.45 : 0.1
        }

        Canvas {
            id: worldPowerIcon
            anchors.fill: parent
            anchors.margins: 1
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                drawPowerSymbol(ctx, width, height, gameWorld.powerUpType, powerColor(gameWorld.powerUpType))
            }
            Component.onCompleted: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onVisibleChanged: requestPaint()
        }
    }

    BuffStatusPanel {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: gameWorld.currentState === AppState.Replaying ? 44 : 4
        anchors.leftMargin: 4
        active: (gameWorld.currentState === AppState.Playing || gameWorld.currentState === AppState.Replaying) &&
                gameWorld.activeBuff !== 0 && gameWorld.buffTicksTotal > 0
        gameFont: gameFont
        menuColor: gameWorld.menuColor
        readableText: gameWorld.readableText
        elapsed: gameWorld.elapsed
        blurSourceItem: gameWorld
        lowPerfMode: Qt.platform.os === "android"
        blurScale: 1.05
        panelOpacity: 0.70
        buffLabel: buffName(gameWorld.activeBuff)
        rarityLabel: rarityName(gameWorld.activeBuff)
        accent: rarityColor(gameWorld.activeBuff)
        buffTier: rarityTier(gameWorld.activeBuff)
        ticksRemaining: gameWorld.buffTicksRemaining
        ticksTotal: gameWorld.buffTicksTotal
        z: gameWorld.layerBuffPanel
    }
}
