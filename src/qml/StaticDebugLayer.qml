import QtQuick
import "meta/PowerMeta.js" as PowerMeta
import "icons" as Icons

Rectangle {
    id: staticSceneLayer
    property string staticScene: ""
    property var staticDebugOptions: ({})
    property string gameFont: ""
    property var menuColor
    property int boardWidth: 24
    property int boardHeight: 18
    property color playBg: "black"
    property color gameGrid: "gray"
    property color gameInk: "white"
    property color gameSubInk: "gray"
    property color gameBorder: "gray"
    property color gameObstacleFill: "dimgray"
    property color gameObstaclePulse: "black"
    property var drawFoodSymbol
    property var drawPowerSymbol
    property var buffName
    property var rarityTier
    property var rarityName
    property var rarityColor
    property var readableText

    function staticOption(name, fallback) {
        if (staticDebugOptions && staticDebugOptions[name] !== undefined) {
            return staticDebugOptions[name]
        }
        return fallback
    }

    anchors.fill: parent
    visible: staticScene !== ""
    color: (showGame || showReplay || showChoice || showOsdText || showOsdVolume)
           ? playBg
           : menuColor("cardPrimary")
    clip: true

    readonly property bool showBoot: staticScene === "boot"
    readonly property bool showGame: staticScene === "game"
    readonly property bool showReplay: staticScene === "replay"
    readonly property bool showChoice: staticScene === "choice"
    readonly property bool showOsdText: staticScene === "osd_text"
    readonly property bool showOsdVolume: staticScene === "osd_volume"
    readonly property color panelBg: menuColor("cardSecondary")
    readonly property color panelAccent: menuColor("actionCard")
    readonly property color panelBorder: menuColor("borderPrimary")
    readonly property color panelBorderSoft: menuColor("borderSecondary")
    readonly property color titleInk: menuColor("titleInk")
    readonly property color accentInk: menuColor("actionInk")
    readonly property color secondaryInk: menuColor("secondaryInk")
    readonly property color hintInk: menuColor("hintInk")
    readonly property color bootMetaFill: Qt.rgba(panelBg.r, panelBg.g, panelBg.b, 0.88)
    readonly property string sceneBadgeText: showBoot ? "DEBUG BOOT"
                                                      : (showGame ? "DEBUG GAME"
                                                                  : (showReplay ? "DEBUG REPLAY"
                                                                                : (showChoice
                                                                                   ? "DEBUG CHOICE"
                                                                                   : (showOsdText
                                                                                      ? "DEBUG OSD"
                                                                                      : "DEBUG VOL"))))
    readonly property color sceneBadgeFill: Qt.rgba(panelAccent.r, panelAccent.g, panelAccent.b, 0.88)
    readonly property color sceneBadgeInk: readableText ? readableText(sceneBadgeFill) : accentInk
    readonly property int layerHud: 60
    readonly property string bootTitleText: String(staticOption("bootTitle", "S N A K E"))
    readonly property string bootSubtitleText: String(staticOption("bootSubtitle", "PORTABLE ARCADE SURVIVAL"))
    readonly property string bootLoadLabel: String(staticOption("bootLoadLabel", "LOADING 72%"))
    readonly property real bootLoadProgress: Math.max(0.0, Math.min(1.0, Number(staticOption("bootLoadProgress", 0.72))))
    readonly property int previewHighScore: staticOption("highScore", 0)
    readonly property int previewScore: staticOption("score", showReplay ? 42 : 18)
    readonly property int previewBuffType: staticOption("buffType", showReplay ? 4 : 3)
    readonly property int previewBuffRemaining: staticOption("buffRemaining", showReplay ? 104 : 136)
    readonly property int previewBuffTotal: staticOption("buffTotal", 180)
    readonly property string previewChoiceTitle: String(staticOption("choiceTitle", "LEVEL UP!"))
    readonly property string previewChoiceSubtitle: String(staticOption("choiceSubtitle", "CHOOSE 1 POWER"))
    readonly property string previewChoiceFooterHint: String(staticOption("choiceFooterHint", "START PICK   SELECT MENU"))
    readonly property string previewOsdText: String(staticOption("osdText", "PALETTE 0"))
    readonly property real previewOsdVolume: Math.max(
                                               0.0,
                                               Math.min(1.0, Number(staticOption("osdVolume", 0.6))))
    readonly property var previewChoiceTypes: {
        const raw = staticOption("choiceTypes", [7, 4, 1])
        if (!raw || raw.length === 0) {
            return [7, 4, 1]
        }
        return raw
    }
    readonly property int previewChoiceIndex: Math.max(
                                                0,
                                                Math.min(2, staticOption("choiceIndex", 0)))
    readonly property var previewChoices: previewChoiceTypes.map((type) => {
        const spec = PowerMeta.choiceSpec(type)
        return {
            type: spec.type,
            name: spec.name,
            desc: spec.description
        }
    })
    readonly property var previewSnakeSegments: {
        const raw = staticOption("snakeSegments", [
                                     { x: 8, y: 7 },
                                     { x: 9, y: 7 },
                                     { x: 10, y: 7, head: true }
                                 ])
        if (!raw || raw.length === 0) {
            return []
        }
        return raw.map((segment, index) => ({
                           x: Number(segment.x),
                           y: Number(segment.y),
                           head: segment.head === true || segment.head === 1 || segment.head === "1" ||
                                 segment.head === "H" || segment.head === "h" || index === raw.length - 1
                       }))
    }
    readonly property var previewObstacleCells: {
        const raw = staticOption("obstacleCells", [{ x: 16, y: 8 }])
        if (!raw || raw.length === 0) {
            return []
        }
        return raw.map((cell) => ({ x: Number(cell.x), y: Number(cell.y) }))
    }
    readonly property var previewFoodCells: {
        const raw = staticOption("foodCells", [{ x: 13, y: 10 }])
        if (!raw || raw.length === 0) {
            return []
        }
        return raw.map((cell) => ({ x: Number(cell.x), y: Number(cell.y) }))
    }
    readonly property var previewPowerupCells: {
        const raw = staticOption("powerupCells", [])
        if (!raw || raw.length === 0) {
            return []
        }
        return raw.map((cell) => ({
                           x: Number(cell.x),
                           y: Number(cell.y),
                           type: Number(cell.type || 1)
                       }))
    }

    Rectangle {
        z: staticSceneLayer.layerHud + 20
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.top: staticSceneLayer.showGame || staticSceneLayer.showReplay ? undefined : parent.top
        anchors.topMargin: staticSceneLayer.showGame || staticSceneLayer.showReplay ? 0 : 4
        anchors.bottom: staticSceneLayer.showGame || staticSceneLayer.showReplay ? parent.bottom : undefined
        anchors.bottomMargin: staticSceneLayer.showGame || staticSceneLayer.showReplay ? 4 : 0
        width: staticSceneLayer.showReplay ? 76 : 72
        height: 15
        radius: 3
        color: staticSceneLayer.sceneBadgeFill
        border.color: staticSceneLayer.panelBorder
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: staticSceneLayer.sceneBadgeText
            color: staticSceneLayer.sceneBadgeInk
            font.family: gameFont
            font.pixelSize: 9
            font.bold: true
        }
    }

    Item {
        anchors.fill: parent
        visible: staticSceneLayer.showBoot

        Text {
            id: bootTitle
            text: staticSceneLayer.bootTitleText
            anchors.horizontalCenter: parent.horizontalCenter
            y: 46
            font.family: gameFont
            font.pixelSize: 32
            color: staticSceneLayer.titleInk
            font.bold: true
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            y: bootTitle.y + 35
            text: staticSceneLayer.bootSubtitleText
            font.family: gameFont
            font.pixelSize: 10
            font.bold: true
            color: staticSceneLayer.titleInk
            style: Text.Outline
            styleColor: Qt.rgba(staticSceneLayer.panelBg.r, staticSceneLayer.panelBg.g, staticSceneLayer.panelBg.b, 0.92)
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 102
            width: 154
            height: 24
            radius: 4
            color: Qt.rgba(staticSceneLayer.panelBg.r, staticSceneLayer.panelBg.g, staticSceneLayer.panelBg.b, 0.84)
            border.color: staticSceneLayer.panelBorder
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "MENU LANGUAGE PREVIEW"
                color: staticSceneLayer.secondaryInk
                font.family: gameFont
                font.pixelSize: 8
                font.bold: true
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 138
            width: 132
            height: 10
            color: staticSceneLayer.panelBg
            border.color: staticSceneLayer.panelBorder
            border.width: 1

            Rectangle {
                x: 1
                y: 1
                width: (parent.width - 2) * staticSceneLayer.bootLoadProgress
                height: parent.height - 2
                color: staticSceneLayer.panelAccent
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 152
            width: 92
            height: 14
            radius: 3
            color: staticSceneLayer.bootMetaFill
            border.color: staticSceneLayer.panelBorder
            border.width: 1

            Text {
                anchors.fill: parent
                text: staticSceneLayer.bootLoadLabel
                font.family: gameFont
                font.pixelSize: 9
                font.bold: true
                color: staticSceneLayer.titleInk
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    Item {
        anchors.fill: parent
        id: scenePreview
        visible: staticSceneLayer.showGame || staticSceneLayer.showReplay || staticSceneLayer.showChoice ||
                 staticSceneLayer.showOsdText || staticSceneLayer.showOsdVolume

        Item {
            id: previewBackdrop
            anchors.fill: parent
            readonly property real cellWidth: width / Math.max(1, staticSceneLayer.boardWidth)
            readonly property real cellHeight: height / Math.max(1, staticSceneLayer.boardHeight)

            Canvas {
                anchors.fill: parent
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const cw = width / Math.max(1, staticSceneLayer.boardWidth)
                    const ch = height / Math.max(1, staticSceneLayer.boardHeight)
                    ctx.strokeStyle = staticSceneLayer.gameGrid
                    ctx.lineWidth = 1
                    for (let x = 0; x <= width; x += cw) {
                        ctx.beginPath()
                        ctx.moveTo(x + 0.5, 0)
                        ctx.lineTo(x + 0.5, height)
                        ctx.stroke()
                    }
                    for (let y = 0; y <= height; y += ch) {
                        ctx.beginPath()
                        ctx.moveTo(0, y + 0.5)
                        ctx.lineTo(width, y + 0.5)
                        ctx.stroke()
                    }
                }
                Component.onCompleted: requestPaint()
            }

            Repeater {
                model: staticSceneLayer.previewSnakeSegments

                delegate: Rectangle {
                    x: modelData.x * previewBackdrop.cellWidth
                    y: modelData.y * previewBackdrop.cellHeight
                    width: previewBackdrop.cellWidth
                    height: previewBackdrop.cellHeight
                    radius: modelData.head ? 2 : 0
                    color: modelData.head ? staticSceneLayer.gameInk
                                          : Qt.darker(staticSceneLayer.gameSubInk, 1.04)
                }
            }

            Repeater {
                model: staticSceneLayer.previewObstacleCells

                delegate: Rectangle {
                    x: modelData.x * previewBackdrop.cellWidth
                    y: modelData.y * previewBackdrop.cellHeight
                    width: previewBackdrop.cellWidth
                    height: previewBackdrop.cellHeight
                    color: staticSceneLayer.gameObstacleFill
                    border.color: staticSceneLayer.gameBorder
                    border.width: 1

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 2
                        color: "transparent"
                        border.color: Qt.rgba(staticSceneLayer.playBg.r,
                                              staticSceneLayer.playBg.g,
                                              staticSceneLayer.playBg.b,
                                              0.28)
                        border.width: 1
                    }
                }
            }

            Repeater {
                model: staticSceneLayer.previewFoodCells

                delegate: Item {
                    x: modelData.x * previewBackdrop.cellWidth
                    y: modelData.y * previewBackdrop.cellHeight
                    width: previewBackdrop.cellWidth
                    height: previewBackdrop.cellHeight

                    Icons.FoodGlyph {
                        anchors.fill: parent
                        strokeColor: staticSceneLayer.gameBorder
                        coreColor: staticSceneLayer.panelAccent
                        highlightColor: staticSceneLayer.playBg
                        stemColor: staticSceneLayer.gameBorder
                        sparkColor: staticSceneLayer.secondaryInk
                    }
                }
            }

            Repeater {
                model: staticSceneLayer.previewPowerupCells

                delegate: Item {
                    x: modelData.x * previewBackdrop.cellWidth
                    y: modelData.y * previewBackdrop.cellHeight
                    width: previewBackdrop.cellWidth
                    height: previewBackdrop.cellHeight

                    Icons.PowerGlyph {
                        anchors.fill: parent
                        powerType: modelData.type
                        glyphColor: staticSceneLayer.gameInk
                    }
                }
            }

            BuffStatusPanel {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.topMargin: staticSceneLayer.showReplay ? 46 : 4
                anchors.leftMargin: 4
                active: staticSceneLayer.showGame || staticSceneLayer.showReplay
                gameFont: staticSceneLayer.gameFont
                menuColor: staticSceneLayer.menuColor
                readableText: staticSceneLayer.readableText
                elapsed: 0
                blurSourceItem: previewBackdrop
                lowPerfMode: Qt.platform.os === "android"
                blurScale: 1.05
                panelOpacity: 0.70
                buffLabel: buffName(previewBuffType)
                rarityLabel: rarityName(previewBuffType)
                accent: rarityColor(previewBuffType)
                buffTier: rarityTier(previewBuffType)
                ticksRemaining: previewBuffRemaining
                ticksTotal: previewBuffTotal
            }

            ReplayBanner {
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 4
                active: staticSceneLayer.showReplay
                menuColor: staticSceneLayer.menuColor
                gameFont: staticSceneLayer.gameFont
                hintText: ""
            }

            HudLayer {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: staticSceneLayer.showReplay ? 28 : 4
                anchors.rightMargin: 4
                z: staticSceneLayer.layerHud
                active: staticSceneLayer.showGame || staticSceneLayer.showReplay
                gameFont: staticSceneLayer.gameFont
                ink: staticSceneLayer.gameInk
                highScoreOverride: previewHighScore
                scoreOverride: previewScore
            }
        }

        LevelUpModal {
            anchors.fill: parent
            active: staticSceneLayer.showChoice
            choices: staticSceneLayer.previewChoices
            choiceIndex: staticSceneLayer.previewChoiceIndex
            gameFont: staticSceneLayer.gameFont
            elapsed: 0
            rarityTier: staticSceneLayer.rarityTier
            rarityName: staticSceneLayer.rarityName
            rarityColor: staticSceneLayer.rarityColor
            blurSourceItem: previewBackdrop
            blurScale: 1.4
            tintColor: Qt.rgba(staticSceneLayer.panelBg.r, staticSceneLayer.panelBg.g, staticSceneLayer.panelBg.b, 0.08)
            modalPanelFill: Qt.lighter(staticSceneLayer.panelBg, 1.08)
            modalPanelBorder: staticSceneLayer.panelBorderSoft
            modalInnerBorder: Qt.rgba(1, 1, 1, 0.08)
            modalTitleInk: staticSceneLayer.titleInk
            modalHintInk: staticSceneLayer.hintInk
            modalCardFill: Qt.lighter(staticSceneLayer.panelBg, 1.04)
            modalCardFillSelected: Qt.lighter(staticSceneLayer.panelBg, 1.10)
            modalCardTitleInk: staticSceneLayer.titleInk
            modalCardDescInk: Qt.rgba(staticSceneLayer.secondaryInk.r, staticSceneLayer.secondaryInk.g, staticSceneLayer.secondaryInk.b, 0.88)
            cardBorderColor: staticSceneLayer.panelBorderSoft
            headerTitleText: staticSceneLayer.previewChoiceTitle
            headerSubtitleText: staticSceneLayer.previewChoiceSubtitle
            footerHintText: staticSceneLayer.previewChoiceFooterHint
        }

        OSDLayer {
            id: staticOsdPreview
            anchors.fill: parent
            visible: staticSceneLayer.showOsdText || staticSceneLayer.showOsdVolume
            pinned: true
            bg: staticSceneLayer.panelAccent
            ink: staticSceneLayer.accentInk
            gameFont: staticSceneLayer.gameFont
        }
    }

    function refreshStaticOsd() {
        if (showOsdText) {
            staticOsdPreview.show(previewOsdText)
        } else if (showOsdVolume) {
            staticOsdPreview.showVolume(previewOsdVolume)
        }
    }

    onStaticSceneChanged: refreshStaticOsd()
    onStaticDebugOptionsChanged: refreshStaticOsd()
}
