import QtQuick
import NenoSerpent 1.0

Item {
    id: overlays
    property int currentState: AppState.Splash
    property int currentScore: 0
    property var choices: []
    property int choiceIndex: 0
    property var menuColor
    property string gameFont: ""
    property real elapsed: 0
    property var rarityTier
    property var rarityName
    property var rarityColor
    property var readableText
    property var readableSecondaryText
    property bool showPausedAndGameOver: true
    property bool showReplayAndChoice: true
    property var blurSourceItem: null
    property int safeInsetTop: 0
    property int safeInsetRight: 0
    property int safeInsetBottom: 0
    property int safeInsetLeft: 0

    readonly property color modalTintColor: Qt.rgba(menuColor("cardPrimary").r, menuColor("cardPrimary").g, menuColor("cardPrimary").b, 0.08)
    readonly property color modalPanelFill: Qt.lighter(menuColor("cardSecondary"), 1.08)
    readonly property color modalPanelBorder: menuColor("borderSecondary")
    readonly property color modalInnerBorder: Qt.rgba(1, 1, 1, 0.08)
    readonly property color modalTitleInk: readableText ? readableText(modalPanelFill) : menuColor("titleInk")
    readonly property color modalSecondaryInk: readableSecondaryText ? readableSecondaryText(modalPanelFill) : menuColor("secondaryInk")
    readonly property color modalMetaInk: Qt.rgba(modalSecondaryInk.r, modalSecondaryInk.g, modalSecondaryInk.b, 0.96)
    readonly property color modalHintInk: Qt.rgba(modalSecondaryInk.r, modalSecondaryInk.g, modalSecondaryInk.b, 0.68)
    readonly property color modalCardFill: Qt.lighter(modalPanelFill, 1.04)
    readonly property color modalCardFillSelected: Qt.lighter(modalPanelFill, 1.10)
    readonly property color modalCardTitleInk: readableText ? readableText(modalCardFill) : menuColor("titleInk")
    readonly property color modalCardDescInk: Qt.rgba(
                                                   (readableSecondaryText ? readableSecondaryText(modalCardFill) : menuColor("secondaryInk")).r,
                                                   (readableSecondaryText ? readableSecondaryText(modalCardFill) : menuColor("secondaryInk")).g,
                                                   (readableSecondaryText ? readableSecondaryText(modalCardFill) : menuColor("secondaryInk")).b,
                                                   0.88)
    readonly property int layerPause: 100
    readonly property int layerReplayBanner: 110
    readonly property int layerGameOver: 120
    readonly property int layerChoiceModal: 200

    anchors.fill: parent

    ModalSurface {
        active: showPausedAndGameOver && overlays.currentState === AppState.Paused
        z: overlays.layerPause
        blurSourceItem: overlays.blurSourceItem
        blurScale: 1.8
        tintColor: overlays.modalTintColor
        panelWidth: 176
        panelHeight: 62
        panelColor: overlays.modalPanelFill
        panelBorderColor: overlays.modalPanelBorder
        panelInnerBorderColor: overlays.modalInnerBorder
        contentMargin: 8

        ModalTextPanel {
            anchors.fill: parent
            titleText: "PAUSED"
            hintText: "START RESUME   SELECT MENU"
            gameFont: overlays.gameFont
            titleColor: overlays.modalTitleInk
            hintColor: overlays.modalHintInk
            titleSize: 22
            hintSize: 7
            hintBold: false
            lineSpacing: 6
        }
    }

    ModalSurface {
        active: showPausedAndGameOver && overlays.currentState === AppState.GameOver
        z: overlays.layerGameOver
        blurSourceItem: overlays.blurSourceItem
        blurScale: 1.8
        tintColor: overlays.modalTintColor
        panelWidth: 184
        panelHeight: 78
        panelColor: overlays.modalPanelFill
        panelBorderColor: overlays.modalPanelBorder
        panelInnerBorderColor: overlays.modalInnerBorder
        contentMargin: 8

        ModalTextPanel {
            anchors.fill: parent
            titleText: "GAME OVER"
            bodyText: `SCORE ${overlays.currentScore}`
            hintText: "START RESTART   SELECT MENU"
            gameFont: overlays.gameFont
            titleColor: overlays.modalTitleInk
            bodyColor: overlays.modalMetaInk
            hintColor: overlays.modalHintInk
            titleSize: 22
            bodySize: 12
            bodyBold: true
            hintSize: 7
            hintBold: false
            lineSpacing: 6
        }
    }

    ReplayBanner {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: overlays.safeInsetTop + 4
        active: showReplayAndChoice && overlays.currentState === AppState.Replaying
        menuColor: overlays.menuColor
        gameFont: overlays.gameFont
        hintText: "START MENU   SELECT MENU"
        z: overlays.layerReplayBanner
    }

    LevelUpModal {
        id: levelUpModal
        anchors.fill: parent
        active: showReplayAndChoice && overlays.currentState === AppState.ChoiceSelection
        z: overlays.layerChoiceModal
        choices: overlays.choices
        choiceIndex: overlays.choiceIndex
        gameFont: overlays.gameFont
        elapsed: overlays.elapsed
        rarityTier: overlays.rarityTier
        rarityName: overlays.rarityName
        rarityColor: overlays.rarityColor
        blurSourceItem: overlays.blurSourceItem
        blurScale: 1.4
        tintColor: overlays.modalTintColor
        modalPanelFill: overlays.modalPanelFill
        modalPanelBorder: overlays.modalPanelBorder
        modalInnerBorder: overlays.modalInnerBorder
        modalTitleInk: overlays.modalTitleInk
        modalHintInk: overlays.modalHintInk
        modalCardFill: overlays.modalCardFill
        modalCardFillSelected: overlays.modalCardFillSelected
        modalCardTitleInk: overlays.modalCardTitleInk
        modalCardDescInk: overlays.modalCardDescInk
        cardBorderColor: overlays.menuColor("borderSecondary")
        horizontalInset: Math.max(4, Math.max(overlays.safeInsetLeft, overlays.safeInsetRight) - 6)
        verticalInsetTop: overlays.safeInsetTop
        verticalInsetBottom: overlays.safeInsetBottom
    }
}
