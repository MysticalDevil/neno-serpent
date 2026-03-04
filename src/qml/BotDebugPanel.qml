import QtQuick

Item {
    id: panel

    property var commandController
    property color bgColor: "#102010"
    property color borderColor: "#406040"
    property color textColor: "#d8f0d0"
    property string gameFont: "Monospace"
    property var statusMap: ({})

    width: 110
    height: 136

    function refreshStatus() {
        if (!panel.commandController || !panel.commandController.botStatus) {
            return
        }
        panel.statusMap = panel.commandController.botStatus()
    }

    function adjustParam(key, delta) {
        const current = Number(panel.statusMap[key])
        if (!Number.isInteger(current)) {
            return
        }
        if (panel.commandController && panel.commandController.setBotParam) {
            if (panel.commandController.setBotParam(key, current + delta)) {
                panel.refreshStatus()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 4
        color: panel.bgColor
        border.color: panel.borderColor
        border.width: 1
        opacity: 0.88
    }

    Column {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 3

        Text {
            text: "BOT TUNER"
            color: panel.textColor
            font.pixelSize: 8
            font.family: panel.gameFont
            font.bold: true
        }

        Text {
            text: `MODE ${panel.statusMap.mode || "off"}`
            color: panel.textColor
            font.pixelSize: 7
            font.family: panel.gameFont
        }

        Row {
            spacing: 4
            Text {
                text: "W"
                color: panel.textColor
                font.pixelSize: 7
                font.family: panel.gameFont
            }
            Text {
                text: String(panel.statusMap.lookaheadWeight || 0)
                color: panel.textColor
                font.pixelSize: 7
                font.family: panel.gameFont
            }
            Text {
                text: "-"
                color: panel.textColor
                font.pixelSize: 9
                font.family: panel.gameFont
                MouseArea {
                    anchors.fill: parent
                    onClicked: panel.adjustParam("lookaheadWeight", -1)
                }
            }
            Text {
                text: "+"
                color: panel.textColor
                font.pixelSize: 9
                font.family: panel.gameFont
                MouseArea {
                    anchors.fill: parent
                    onClicked: panel.adjustParam("lookaheadWeight", 1)
                }
            }
        }

        Row {
            spacing: 4
            Text {
                text: "D"
                color: panel.textColor
                font.pixelSize: 7
                font.family: panel.gameFont
            }
            Text {
                text: String(panel.statusMap.lookaheadDepth || 0)
                color: panel.textColor
                font.pixelSize: 7
                font.family: panel.gameFont
            }
            Text {
                text: "-"
                color: panel.textColor
                font.pixelSize: 9
                font.family: panel.gameFont
                MouseArea {
                    anchors.fill: parent
                    onClicked: panel.adjustParam("lookaheadDepth", -1)
                }
            }
            Text {
                text: "+"
                color: panel.textColor
                font.pixelSize: 9
                font.family: panel.gameFont
                MouseArea {
                    anchors.fill: parent
                    onClicked: panel.adjustParam("lookaheadDepth", 1)
                }
            }
        }

        Row {
            spacing: 4
            Text {
                text: "RISK"
                color: panel.textColor
                font.pixelSize: 7
                font.family: panel.gameFont
            }
            Text {
                text: String(panel.statusMap.safeNeighborWeight || 0)
                color: panel.textColor
                font.pixelSize: 7
                font.family: panel.gameFont
            }
            Text {
                text: "-"
                color: panel.textColor
                font.pixelSize: 9
                font.family: panel.gameFont
                MouseArea {
                    anchors.fill: parent
                    onClicked: panel.adjustParam("safeNeighborWeight", -1)
                }
            }
            Text {
                text: "+"
                color: panel.textColor
                font.pixelSize: 9
                font.family: panel.gameFont
                MouseArea {
                    anchors.fill: parent
                    onClicked: panel.adjustParam("safeNeighborWeight", 1)
                }
            }
        }

        Text {
            text: "MODE+ (F8)"
            color: panel.textColor
            font.pixelSize: 7
            font.family: panel.gameFont
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (panel.commandController && panel.commandController.cycleBotMode) {
                        panel.commandController.cycleBotMode()
                        panel.refreshStatus()
                    }
                }
            }
        }

        Text {
            text: "RESET DEFAULT"
            color: panel.textColor
            font.pixelSize: 7
            font.family: panel.gameFont
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (panel.commandController
                            && panel.commandController.resetBotModeDefaults) {
                        panel.commandController.resetBotModeDefaults()
                        panel.refreshStatus()
                    }
                }
            }
        }
    }

    Component.onCompleted: panel.refreshStatus()

    Timer {
        interval: 250
        running: panel.visible
        repeat: true
        onTriggered: panel.refreshStatus()
    }
}
