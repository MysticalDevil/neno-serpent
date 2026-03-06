import QtQuick
import "components" as Components

Rectangle {
    id: medalRoot
    anchors.fill: parent

    property color p0
    property color p1
    property color p2
    property color p3
    property var menuColor
    property var pageTheme: ({})
    property var medalLibraryModel: []
    property int medalIndex: 0
    property int unlockedCount: 0
    property var unlockedAchievementIds: []
    property var debugUnlockedAchievementIds: []
    property bool debugUnlockAll: false
    property var setMedalIndex
    property string gameFont

    readonly property color pageBg: menuColor("cardPrimary")
    readonly property color panelBgStrong: menuColor("cardPrimary")
    readonly property color panelBg: menuColor("cardSecondary")
    readonly property color panelBgSoft: menuColor("hintCard")
    readonly property color panelAccent: pageTheme && pageTheme.cardSelected ? pageTheme.cardSelected : menuColor("actionCard")
    readonly property color titleColor: menuColor("titleInk")
    readonly property color dividerColor: menuColor("borderPrimary")
    readonly property color cardNormal: Qt.rgba(panelBg.r, panelBg.g, panelBg.b, 0.84)
    readonly property color cardSelected: Qt.rgba(panelAccent.r, panelAccent.g, panelAccent.b, 0.92)
    readonly property color cardBorder: menuColor("borderSecondary")
    readonly property color badgeFill: pageTheme && pageTheme.badgeFill ? pageTheme.badgeFill : menuColor("actionCard")
    readonly property color badgeText: pageTheme && pageTheme.badgeText ? pageTheme.badgeText : menuColor("actionInk")
    readonly property color secondaryText: menuColor("secondaryInk")
    readonly property color iconFill: pageTheme && pageTheme.iconFill ? pageTheme.iconFill : panelBgStrong
    readonly property color unknownText: pageTheme && pageTheme.unknownText ? pageTheme.unknownText : secondaryText
    readonly property color scrollbarHandle: pageTheme && pageTheme.scrollbarHandle ? pageTheme.scrollbarHandle : dividerColor
    readonly property color scrollbarTrack: pageTheme && pageTheme.scrollbarTrack ? pageTheme.scrollbarTrack : panelBgSoft
    readonly property int cardSpacing: 4
    readonly property var effectiveUnlockedAchievementIds: {
        if (debugUnlockAll) {
            return medalLibraryModel.map((entry) => entry.id)
        }
        return debugUnlockedAchievementIds && debugUnlockedAchievementIds.length > 0
            ? debugUnlockedAchievementIds
            : unlockedAchievementIds
    }
    readonly property int effectiveUnlockedCount: debugUnlockAll
        ? medalLibraryModel.length
        : effectiveUnlockedAchievementIds.length

    color: pageBg
    clip: true

    Rectangle {
        anchors.fill: parent
        anchors.margins: 6
        color: Qt.rgba(medalRoot.panelBgStrong.r, medalRoot.panelBgStrong.g, medalRoot.panelBgStrong.b, 0.42)
        border.color: medalRoot.dividerColor
        border.width: 1
    }

    Column {
        id: medalColumn
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        Components.SectionHeader {
            id: headerPanel
            width: parent.width
            height: 30
            color: Qt.rgba(medalRoot.panelBgStrong.r, medalRoot.panelBgStrong.g, medalRoot.panelBgStrong.b, 0.86)
            border.color: medalRoot.dividerColor
            titleText: "ACHIEVEMENTS"
            subtitleText: `${medalRoot.effectiveUnlockedCount} UNLOCKED`
            gameFont: gameFont
            textColor: medalRoot.titleColor
            subtitleColor: Qt.rgba(medalRoot.secondaryText.r, medalRoot.secondaryText.g, medalRoot.secondaryText.b, 0.92)
        }

        Components.RetroWindowList {
            id: medalList
            width: parent.width
            height: Math.max(
                        0,
                        medalColumn.height - headerPanel.height - footerPanel.height - (medalColumn.spacing * 2))
            model: medalLibraryModel
            selectedIndex: medalRoot.medalIndex
            visibleCount: 3
            itemHeight: Math.floor((height - (medalRoot.cardSpacing * (visibleCount - 1))) / visibleCount)
            spacing: medalRoot.cardSpacing
            scrollbarHandle: medalRoot.scrollbarHandle
            scrollbarTrack: medalRoot.scrollbarTrack
            flashColor: Qt.rgba(medalRoot.badgeText.r, medalRoot.badgeText.g, medalRoot.badgeText.b, 0.18)

            onRequestIndexChange: (nextIndex) => {
                if (nextIndex !== medalRoot.medalIndex && medalRoot.setMedalIndex) {
                    medalRoot.setMedalIndex(nextIndex)
                }
            }

            Repeater {
                model: medalList.visibleCount

                delegate: MedalRoomCard {
                    readonly property int modelIndex: medalList.modelIndexAt(index)
                    readonly property var entryData: medalList.entryAt(index)

                    x: 0
                    y: index * (height + medalList.spacing)
                    width: medalList.width - 12
                    height: medalList.itemHeight
                    visible: entryData !== null
                    medalData: entryData
                    unlocked: !!entryData && (medalRoot.debugUnlockAll
                        || medalRoot.effectiveUnlockedAchievementIds.indexOf(entryData.id) !== -1)
                    selected: modelIndex === medalRoot.medalIndex
                    cardNormal: medalRoot.cardNormal
                    cardSelected: medalRoot.cardSelected
                    cardBorder: medalRoot.cardBorder
                    badgeFill: medalRoot.badgeFill
                    badgeText: medalRoot.badgeText
                    titleColor: medalRoot.titleColor
                    secondaryText: medalRoot.secondaryText
                    iconFill: medalRoot.iconFill
                    unknownText: medalRoot.unknownText
                    gameFont: medalRoot.gameFont
                }
            }
        }

        Rectangle {
            id: footerPanel
            width: parent.width
            height: 16
            radius: 3
            color: Qt.rgba(medalRoot.panelBgStrong.r, medalRoot.panelBgStrong.g, medalRoot.panelBgStrong.b, 0.82)
            border.color: medalRoot.dividerColor
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "UP PREV   DOWN NEXT   SELECT MENU"
                color: medalRoot.secondaryText
                font.family: gameFont
                font.pixelSize: 8
                font.bold: true
            }
        }
    }
}
