import QtQuick

Rectangle {
    id: sectionHeader

    property string titleText: ""
    property string subtitleText: ""
    property string gameFont: ""
    property color textColor: "white"
    property color subtitleColor: "white"
    property int titlePixelSize: 12
    property int subtitlePixelSize: 7

    radius: 3
    border.width: 1

    Column {
        anchors.centerIn: parent
        spacing: 0

        Text {
            text: sectionHeader.titleText
            color: sectionHeader.textColor
            font.family: sectionHeader.gameFont
            font.pixelSize: sectionHeader.titlePixelSize
            font.bold: true
        }

        Text {
            visible: sectionHeader.subtitleText.length > 0
            text: sectionHeader.subtitleText
            color: sectionHeader.subtitleColor
            font.family: sectionHeader.gameFont
            font.pixelSize: sectionHeader.subtitlePixelSize
            font.bold: true
        }
    }
}
