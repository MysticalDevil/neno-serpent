import QtQuick

ModalSurface {
    id: levelUpModal

    property var choices: []
    property int choiceIndex: 0
    property string gameFont: ""
    property real elapsed: 0
    property var rarityTier
    property var rarityName
    property var rarityColor
    property color modalPanelFill: "white"
    property color modalPanelBorder: "black"
    property color modalInnerBorder: Qt.rgba(1, 1, 1, 0.08)
    property color modalTitleInk: "black"
    property color modalHintInk: Qt.rgba(0, 0, 0, 0.72)
    property color modalCardFill: "white"
    property color modalCardFillSelected: "white"
    property color modalCardTitleInk: "black"
    property color modalCardDescInk: Qt.rgba(0, 0, 0, 0.84)
    property color cardBorderColor: "black"
    property string headerTitleText: "LEVEL UP!"
    property string headerSubtitleText: "CHOOSE 1 POWER"
    property string footerHintText: "START PICK   SELECT MENU"
    property real horizontalInset: 0
    property real verticalInsetTop: 0
    property real verticalInsetBottom: 0

    panelWidth: Math.max(188, width - 28 - horizontalInset)
    panelHeight: Math.max(170, height - 42 - verticalInsetTop - verticalInsetBottom)
    panelColor: modalPanelFill
    panelBorderColor: modalPanelBorder
    panelInnerBorderColor: modalInnerBorder
    panelOffsetY: 6
    contentMargin: 8

    readonly property int verticalGap: 5
    readonly property int headerHeight: 26
    readonly property int footerHeight: 20
    readonly property int choiceCount: Math.max(
                                           1,
                                           levelUpModal.choices ? levelUpModal.choices.length : 3)

    Item {
        anchors.fill: parent

        Column {
            id: modalStack
            anchors.centerIn: parent
            width: parent.width
            spacing: levelUpModal.verticalGap
            readonly property int cardHeight: Math.max(
                                                  32,
                                                  Math.min(
                                                      40,
                                                      Math.floor(
                                                          (parent.height
                                                           - levelUpModal.headerHeight
                                                           - levelUpModal.footerHeight
                                                           - (spacing * (levelUpModal.choiceCount + 1)))
                                                          / levelUpModal.choiceCount)))

            Item {
                id: headerPanel
                width: parent.width
                height: levelUpModal.headerHeight

                Column {
                    anchors.fill: parent
                    spacing: 0

                    Text {
                        width: headerPanel.width
                        height: 15
                        text: levelUpModal.headerTitleText
                        color: levelUpModal.modalTitleInk
                        font.family: levelUpModal.gameFont
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    Text {
                        width: headerPanel.width
                        height: 9
                        text: levelUpModal.headerSubtitleText
                        color: levelUpModal.modalHintInk
                        font.family: levelUpModal.gameFont
                        font.pixelSize: 8
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            Repeater {
                model: levelUpModal.choices

                delegate: ModalChoiceCard {
                    width: modalStack.width
                    height: modalStack.cardHeight
                    gameFont: levelUpModal.gameFont
                    titleText: modelData.name
                    descriptionText: modelData.desc
                    badgeText: levelUpModal.rarityName(Number(modelData.type))
                    powerType: Number(modelData.type)
                    selected: levelUpModal.choiceIndex === index
                    elapsed: levelUpModal.elapsed
                    accent: levelUpModal.rarityColor(powerType)
                    fillColor: levelUpModal.modalCardFill
                    fillSelectedColor: levelUpModal.modalCardFillSelected
                    frameBorderColor: levelUpModal.cardBorderColor
                    frameBorderSelectedColor: levelUpModal.modalPanelBorder
                    titleColor: levelUpModal.modalCardTitleInk
                    descriptionColor: levelUpModal.modalCardDescInk
                    iconSocketColor: Qt.lighter(levelUpModal.modalCardFill, 1.02)
                    iconBorderColor: levelUpModal.cardBorderColor
                    iconGlyphColor: selected ? Qt.darker(accent, 1.45) : Qt.darker(accent, 1.22)
                    badgeColor: Qt.rgba(accent.r, accent.g, accent.b, selected ? 0.16 : 0.10)
                    badgeBorderColor: selected ? Qt.darker(accent, 1.30) : levelUpModal.cardBorderColor
                    badgeTextColor: levelUpModal.modalCardTitleInk
                    rarityTier: levelUpModal.rarityTier
                }
            }

            Item {
                id: footerPanel
                width: parent.width
                height: levelUpModal.footerHeight

                Row {
                    anchors.fill: parent
                    spacing: 8

                    Text {
                        width: parent.width
                        height: footerPanel.height
                        text: levelUpModal.footerHintText
                        color: levelUpModal.modalHintInk
                        font.family: levelUpModal.gameFont
                        font.pixelSize: 9
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}
