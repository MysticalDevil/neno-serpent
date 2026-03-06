import QtQuick
import "icons" as Icons
import "components" as Components

Components.ListCardFrame {
    id: choiceCard
    property string gameFont: ""
    property string titleText: ""
    property string descriptionText: ""
    property string badgeText: ""
    property int powerType: 0
    property real elapsed: 0
    property color accent: "white"
    property color fillColor: "white"
    property color fillSelectedColor: "white"
    property color frameBorderColor: "black"
    property color frameBorderSelectedColor: "black"
    property color titleColor: "black"
    property color descriptionColor: "black"
    property color iconSocketColor: "white"
    property color iconBorderColor: "black"
    property color iconGlyphColor: "black"
    property color badgeColor: "white"
    property color badgeBorderColor: "black"
    property color badgeTextColor: "black"
    property var rarityTier

    width: parent ? parent.width : 0
    height: 48
    radius: 4
    normalFill: choiceCard.fillColor
    selectedFill: choiceCard.fillSelectedColor
    borderColor: selected ? frameBorderSelectedColor : frameBorderColor

    readonly property int badgeWidth: 50
    readonly property int sidePadding: 8
    readonly property int iconSize: Math.min(26, Math.max(20, height - 18))
    readonly property bool pulseAccent: rarityTier ? rarityTier(powerType) >= 3 : false

    Rectangle {
        anchors.fill: parent
        radius: choiceCard.radius
        color: choiceCard.accent
        opacity: selected ? 0.08 : 0.02
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: selected ? 5 : 4
        radius: 3
        color: selected ? Qt.darker(accent, 1.18) : accent
        opacity: selected ? 1.0 : 0.78
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        radius: 1
        color: Qt.rgba(1, 1, 1, selected ? 0.24 : 0.12)
        opacity: 1.0
    }

    Row {
        anchors.fill: parent
        anchors.margins: choiceCard.sidePadding
        anchors.leftMargin: choiceCard.sidePadding + 6
        anchors.rightMargin: choiceCard.sidePadding + 1
        spacing: 10

        Icons.PowerIcon {
            width: choiceCard.iconSize
            height: width
            anchors.verticalCenter: parent.verticalCenter
            radius: 6
            contentMargin: 2
            fillColor: choiceCard.iconSocketColor
            borderColor: choiceCard.selected ? choiceCard.frameBorderSelectedColor : choiceCard.iconBorderColor
            borderWidth: 1
            powerType: choiceCard.powerType
            glyphColor: choiceCard.iconGlyphColor
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - choiceCard.iconSize - choiceCard.badgeWidth - 24
            spacing: 1

            Text {
                text: choiceCard.titleText
                color: choiceCard.titleColor
                font.family: choiceCard.gameFont
                font.pixelSize: 10
                font.bold: true
                width: parent.width
                elide: Text.ElideRight
            }

            Text {
                text: choiceCard.descriptionText
                color: choiceCard.descriptionColor
                font.family: choiceCard.gameFont
                font.pixelSize: 8
                font.bold: true
                width: parent.width
                opacity: 0.88
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
            }
        }
    }

    Components.StatusPill {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: choiceCard.sidePadding
        anchors.topMargin: 3
        width: choiceCard.badgeWidth
        height: 13
        color: choiceCard.badgeColor
        borderColor: choiceCard.badgeBorderColor
        borderLineWidth: selected ? 2 : 1
        textColor: choiceCard.badgeTextColor
        gameFont: choiceCard.gameFont
        label: choiceCard.badgeText
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: accent
        border.width: 1
        opacity: choiceCard.pulseAccent
                 ? ((Math.floor(choiceCard.elapsed * 6) % 2 === 0) ? 0.28 : 0.08)
                 : 0.0
    }
}
