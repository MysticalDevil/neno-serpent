import QtQuick
import "meta/PowerMeta.js" as PowerMeta
import "meta/AchievementMeta.js" as AchievementMeta
import "icons" as Icons
import "components" as Components

Rectangle {
    id: iconLabLayer
    property bool active: false
    property int iconLabPage: 0
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
    readonly property int titleContainerHeight: 30
    readonly property int sectionHeaderHeight: 12
    readonly property int fruitTileHeight: 24
    readonly property int medalTileHeight: 28
    readonly property int fruitColumns: 3
    readonly property int medalColumns: 4
    readonly property int footerHeight: 12
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
    readonly property var fruitTypes: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
    readonly property int pageCount: 2
    readonly property bool onFruitPage: iconLabPage === 0
    readonly property bool onMedalPage: iconLabPage === 1
    readonly property string selectedLabel: onFruitPage
        ? PowerMeta.buffName(iconLabSelection + 1)
        : AchievementMeta.shortLabel(medalSpecs[iconLabSelection] ? medalSpecs[iconLabSelection].id : "")

    onVisibleChanged: {
        if (visible) {
            resetSelectionRequested()
        }
    }

    onIconLabPageChanged: pageFlash.restart()

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

        Rectangle {
            id: titleContainer
            width: parent.width
            height: iconLabLayer.titleContainerHeight
            radius: 4
            color: Qt.rgba(iconLabLayer.panelBgStrong.r, iconLabLayer.panelBgStrong.g, iconLabLayer.panelBgStrong.b, 0.82)
            border.color: iconLabLayer.borderStrong
            border.width: 1

            Components.SectionHeader {
                anchors.fill: parent
                color: "transparent"
                border.width: 0
                titleText: "ICON LAB"
                subtitleText: "FRUITS + MEDALS"
                gameFont: gameFont
                textColor: iconLabLayer.textStrong
                subtitleColor: Qt.rgba(iconLabLayer.textMuted.r, iconLabLayer.textMuted.g, iconLabLayer.textMuted.b, 0.9)
            }
        }

        Rectangle {
            id: pageContainer
            width: parent.width
            height: Math.max(
                        0,
                        parent.height - titleContainer.height - iconLabLayer.contentSpacing)
            radius: 4
            color: Qt.rgba(iconLabLayer.panelBg.r, iconLabLayer.panelBg.g, iconLabLayer.panelBg.b, 0.68)
            border.color: iconLabLayer.borderSoft
            border.width: 1
            clip: true

            Item {
                id: pageArea
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: footerPanel.top
                anchors.margins: 4
                clip: true

                Column {
                    anchors.fill: parent
                    spacing: iconLabLayer.contentSpacing
                    visible: iconLabLayer.onFruitPage

                    Grid {
                        id: iconLabGrid
                        width: parent.width
                        height: (Math.ceil(iconLabLayer.fruitTypes.length / iconLabLayer.fruitColumns) * iconLabLayer.fruitTileHeight)
                                + ((Math.ceil(iconLabLayer.fruitTypes.length / iconLabLayer.fruitColumns) - 1) * iconLabLayer.contentSpacing)
                        columns: iconLabLayer.fruitColumns
                        columnSpacing: iconLabLayer.contentSpacing
                        rowSpacing: iconLabLayer.contentSpacing

                        Repeater {
                            model: iconLabLayer.fruitTypes

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
                                    font.pixelSize: 6
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                            }
                        }
                        }
                    }
                }

                Column {
                    anchors.fill: parent
                    spacing: iconLabLayer.contentSpacing
                    visible: iconLabLayer.onMedalPage

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

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    radius: 2
                                    color: "transparent"
                                    border.color: iconLabLayer.borderStrong
                                    border.width: 1
                                    visible: iconLabLayer.iconLabSelection === index
                                    opacity: (Math.floor(iconLabLayer.elapsed * 8) % 2 === 0) ? 0.9 : 0.5
                                }

                                Row {
                                    anchors.fill: parent
                                    anchors.margins: 3
                                    spacing: 4

                                    Icons.AchievementBadgeIcon {
                                        width: 20
                                        height: 20
                                        anchors.verticalCenter: parent.verticalCenter
                                        achievementId: modelData.id
                                        unlocked: true
                                        selected: iconLabLayer.iconLabSelection === index
                                        badgeFill: iconLabLayer.panelAccent
                                        badgeText: iconLabLayer.textOnAccent
                                        iconFill: iconLabLayer.panelBgSoft
                                        unknownText: iconLabLayer.textMuted
                                        borderColor: iconLabLayer.borderStrong
                                        borderWidth: 1
                                    }

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: parent.width - 26
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

            SequentialAnimation {
                id: pageFlash
                running: false

                NumberAnimation {
                    target: pageFlashOverlay
                    property: "opacity"
                    from: 0.0
                    to: 0.12
                    duration: 45
                }

                NumberAnimation {
                    target: pageFlashOverlay
                    property: "opacity"
                    from: 0.12
                    to: 0.0
                    duration: 80
                }
            }

            Rectangle {
                id: pageFlashOverlay
                anchors.top: pageArea.top
                anchors.bottom: pageArea.bottom
                anchors.left: pageArea.left
                anchors.right: pageArea.right
                color: iconLabLayer.textOnAccent
                opacity: 0
            }

            Rectangle {
                id: footerPanel
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 4
                height: iconLabLayer.footerHeight
                radius: 3
                color: Qt.rgba(iconLabLayer.panelBgStrong.r, iconLabLayer.panelBgStrong.g, iconLabLayer.panelBgStrong.b, 0.84)
                border.color: iconLabLayer.borderSoft
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: `PAGE ${iconLabLayer.iconLabPage + 1}/${iconLabLayer.pageCount}  SELECTED: ${iconLabLayer.selectedLabel}`
                    color: iconLabLayer.textStrong
                    font.family: gameFont
                    font.pixelSize: 8
                    font.bold: true
                }
            }
        }
    }
}
