import QtQuick
import QtQuick.Controls
import "meta/PowerMeta.js" as PowerMeta
import "meta/AchievementMeta.js" as AchievementMeta
import "icons" as Icons
import "components" as Components

Rectangle {
    id: iconLabLayer
    property bool active: false
    property int iconLabSelection: 0
    property real elapsed: 0
    property string gameFont: ""
    property var menuColor
    property var powerColor
    signal resetSelectionRequested()

    visible: active
    anchors.fill: parent
    color: menuColor("cardPrimary")
    clip: true

    readonly property int contentMargin: 8
    readonly property int contentSpacing: 4
    readonly property int headerHeight: 24
    readonly property int infoHeight: 28
    readonly property int sectionHeaderHeight: 12
    readonly property int fruitTileHeight: 30
    readonly property int medalTileHeight: 18
    readonly property int fruitColumns: 3
    readonly property int medalColumns: 4
    readonly property int footerHeight: 14
    readonly property color panelBgStrong: menuColor("cardPrimary")
    readonly property color panelBg: menuColor("cardSecondary")
    readonly property color panelBgSoft: menuColor("hintCard")
    readonly property color panelAccent: menuColor("actionCard")
    readonly property color borderStrong: menuColor("borderPrimary")
    readonly property color borderSoft: menuColor("borderSecondary")
    readonly property color textStrong: menuColor("titleInk")
    readonly property color textMuted: menuColor("secondaryInk")
    readonly property color textOnAccent: menuColor("actionInk")
    readonly property var medalSpecs: AchievementMeta.all()

    onVisibleChanged: {
        if (visible) {
            resetSelectionRequested()
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 6
        color: Qt.rgba(iconLabLayer.panelBgStrong.r, iconLabLayer.panelBgStrong.g, iconLabLayer.panelBgStrong.b, 0.44)
        border.color: iconLabLayer.borderStrong
        border.width: 1
    }

    Column {
        anchors.fill: parent
        anchors.margins: iconLabLayer.contentMargin
        spacing: iconLabLayer.contentSpacing

        Components.SectionHeader {
            width: parent.width
            height: iconLabLayer.headerHeight
            color: Qt.rgba(iconLabLayer.panelBgStrong.r, iconLabLayer.panelBgStrong.g, iconLabLayer.panelBgStrong.b, 0.86)
            border.color: iconLabLayer.borderStrong
            titleText: "ICON LAB"
            subtitleText: "FRUITS + MEDALS"
            gameFont: gameFont
            textColor: iconLabLayer.textStrong
            subtitleColor: Qt.rgba(iconLabLayer.textMuted.r, iconLabLayer.textMuted.g, iconLabLayer.textMuted.b, 0.9)
        }

        Flickable {
            id: scrollArea
            width: parent.width
            height: Math.max(
                        0,
                        parent.height - iconLabLayer.headerHeight - iconLabLayer.footerHeight
                        - iconLabLayer.contentSpacing)
            clip: true
            contentWidth: width
            contentHeight: scrollContent.height
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                width: 5

                contentItem: Rectangle {
                    implicitWidth: 5
                    radius: 3
                    color: iconLabLayer.borderStrong
                }

                background: Rectangle {
                    radius: 3
                    color: iconLabLayer.panelBgSoft
                    opacity: 0.35
                }
            }

            Column {
                id: scrollContent
                width: scrollArea.width - 6
                spacing: iconLabLayer.contentSpacing

                Row {
                    width: parent.width
                    spacing: iconLabLayer.contentSpacing

                    Rectangle {
                        width: 90
                        height: iconLabLayer.infoHeight
                        radius: 3
                        color: Qt.rgba(iconLabLayer.panelBg.r, iconLabLayer.panelBg.g, iconLabLayer.panelBg.b, 0.84)
                        border.color: iconLabLayer.borderSoft
                        border.width: 1

                        Row {
                            anchors.centerIn: parent
                            spacing: 8
                            Rectangle {
                                width: 20
                                height: 20
                                radius: 3
                                color: Qt.rgba(iconLabLayer.panelBgSoft.r, iconLabLayer.panelBgSoft.g, iconLabLayer.panelBgSoft.b, 0.86)
                                border.color: iconLabLayer.borderStrong
                                border.width: 1
                                Icons.FoodGlyph {
                                    anchors.fill: parent
                                    strokeColor: iconLabLayer.borderStrong
                                    coreColor: iconLabLayer.panelAccent
                                    highlightColor: iconLabLayer.panelBgStrong
                                    stemColor: iconLabLayer.borderStrong
                                    sparkColor: iconLabLayer.textMuted
                                }
                            }
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                Text { text: "FOOD"; color: iconLabLayer.textStrong; font.family: gameFont; font.pixelSize: 8; font.bold: true }
                                Text { text: "BASE"; color: iconLabLayer.textMuted; font.family: gameFont; font.pixelSize: 7; font.bold: true }
                            }
                        }
                    }

                    Rectangle {
                        width: Math.max(64, parent.width - 90 - iconLabLayer.contentSpacing)
                        height: iconLabLayer.infoHeight
                        radius: 3
                        color: Qt.rgba(iconLabLayer.panelBg.r, iconLabLayer.panelBg.g, iconLabLayer.panelBg.b, 0.84)
                        border.color: iconLabLayer.borderSoft
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: "POWERUP ICON SUITE"
                            color: iconLabLayer.textStrong
                            font.family: gameFont
                            font.pixelSize: 8
                            font.bold: true
                        }
                    }
                }

                Grid {
                    id: iconLabGrid
                    width: parent.width
                    height: (Math.ceil(12 / iconLabLayer.fruitColumns) * iconLabLayer.fruitTileHeight)
                            + ((Math.ceil(12 / iconLabLayer.fruitColumns) - 1) * iconLabLayer.contentSpacing)
                    columns: iconLabLayer.fruitColumns
                    columnSpacing: iconLabLayer.contentSpacing
                    rowSpacing: iconLabLayer.contentSpacing

                    Repeater {
                        model: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
                        delegate: Rectangle {
                            width: Math.floor((iconLabGrid.width - (iconLabGrid.columnSpacing * (iconLabLayer.fruitColumns - 1))) / iconLabLayer.fruitColumns)
                            height: iconLabLayer.fruitTileHeight
                            radius: 4
                            property int iconIdx: index
                            clip: true
                            color: Qt.rgba(iconLabLayer.panelBg.r, iconLabLayer.panelBg.g, iconLabLayer.panelBg.b, 0.8)
                            border.color: powerColor(modelData)
                            border.width: 1

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 1
                                radius: 3
                                color: "transparent"
                                border.color: iconLabLayer.borderStrong
                                border.width: 1
                                visible: iconLabLayer.iconLabSelection === iconIdx
                                opacity: (Math.floor(iconLabLayer.elapsed * 8) % 2 === 0) ? 0.9 : 0.5
                            }

                            Components.StatusPill {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.rightMargin: 2
                                anchors.topMargin: 2
                                width: 20
                                height: 9
                                visible: iconLabLayer.iconLabSelection === iconIdx
                                color: iconLabLayer.panelAccent
                                borderColor: iconLabLayer.borderStrong
                                textColor: iconLabLayer.textOnAccent
                                gameFont: gameFont
                                label: "SEL"
                            }

                            Row {
                                anchors.fill: parent
                                anchors.margins: 4
                                spacing: 4

                                Rectangle {
                                    width: 18
                                    height: 18
                                    radius: 4
                                    color: Qt.rgba(iconLabLayer.panelBgSoft.r, iconLabLayer.panelBgSoft.g, iconLabLayer.panelBgSoft.b, 0.86)
                                    border.color: iconLabLayer.borderStrong
                                    border.width: 1
                                    anchors.verticalCenter: parent.verticalCenter

                                    Icons.PowerGlyph {
                                        anchors.fill: parent
                                        powerType: modelData
                                        glyphColor: powerColor(modelData)
                                    }
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 22
                                    text: PowerMeta.choiceName(modelData).toUpperCase()
                                    color: iconLabLayer.textStrong
                                    font.family: gameFont
                                    font.pixelSize: 7
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }

                Components.SectionHeader {
                    width: parent.width
                    height: iconLabLayer.sectionHeaderHeight
                    color: Qt.rgba(iconLabLayer.panelBg.r, iconLabLayer.panelBg.g, iconLabLayer.panelBg.b, 0.84)
                    border.color: iconLabLayer.borderSoft
                    titleText: "ACHIEVEMENT GLYPHS"
                    gameFont: gameFont
                    textColor: iconLabLayer.textStrong
                    titlePixelSize: 8
                }

                Grid {
                    width: parent.width
                    height: (Math.ceil(iconLabLayer.medalSpecs.length / iconLabLayer.medalColumns) * iconLabLayer.medalTileHeight)
                            + ((Math.ceil(iconLabLayer.medalSpecs.length / iconLabLayer.medalColumns) - 1) * iconLabLayer.contentSpacing)
                    columns: iconLabLayer.medalColumns
                    columnSpacing: iconLabLayer.contentSpacing
                    rowSpacing: iconLabLayer.contentSpacing

                    Repeater {
                        model: iconLabLayer.medalSpecs

                        delegate: Rectangle {
                            width: Math.floor((parent.width - (parent.columnSpacing * (iconLabLayer.medalColumns - 1))) / iconLabLayer.medalColumns)
                            height: iconLabLayer.medalTileHeight
                            radius: 3
                            color: Qt.rgba(iconLabLayer.panelBg.r, iconLabLayer.panelBg.g, iconLabLayer.panelBg.b, 0.8)
                            border.color: iconLabLayer.borderSoft
                            border.width: 1

                            Row {
                                anchors.fill: parent
                                anchors.margins: 2
                                spacing: 3

                                Icons.AchievementBadgeIcon {
                                    width: 14
                                    height: 14
                                    anchors.verticalCenter: parent.verticalCenter
                                    achievementId: modelData.id
                                    unlocked: true
                                    badgeFill: iconLabLayer.panelAccent
                                    badgeText: iconLabLayer.textOnAccent
                                    iconFill: iconLabLayer.panelBgSoft
                                    unknownText: iconLabLayer.textMuted
                                    borderColor: iconLabLayer.borderStrong
                                    borderWidth: 1
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 20
                                    text: AchievementMeta.shortLabel(modelData.id)
                                    color: iconLabLayer.textStrong
                                    font.family: gameFont
                                    font.pixelSize: 6
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            width: parent.width
            height: iconLabLayer.footerHeight
            radius: 3
            color: Qt.rgba(iconLabLayer.panelBg.r, iconLabLayer.panelBg.g, iconLabLayer.panelBg.b, 0.84)
            border.color: iconLabLayer.borderSoft
            border.width: 1
            Text {
                anchors.centerIn: parent
                text: `SELECTED: ${PowerMeta.buffName(iconLabLayer.iconLabSelection + 1)}`
                color: iconLabLayer.textStrong
                font.family: gameFont
                font.pixelSize: 8
                font.bold: true
            }
        }
    }
}
