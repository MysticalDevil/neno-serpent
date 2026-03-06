import QtQuick
import "meta/CatalogMeta.js" as CatalogMeta
import "icons" as Icons
import "components" as Components

Rectangle {
    id: libraryLayer
    property bool active: false
    property string gameFont: ""
    property var fruitLibraryModel: []
    property int libraryIndex: 0
    property var setLibraryIndex
    property var menuColor
    property var pageTheme
    property var powerColor
    property var debugDiscoveredTypes: []
    property bool debugDiscoverAll: false

    readonly property color pageBg: menuColor("cardPrimary")
    readonly property color panelBgStrong: menuColor("cardPrimary")
    readonly property color panelBg: menuColor("cardSecondary")
    readonly property color panelBgSoft: menuColor("hintCard")
    readonly property color panelAccent: pageTheme && pageTheme.cardSelected ? pageTheme.cardSelected : menuColor("actionCard")
    readonly property color borderStrong: menuColor("borderPrimary")
    readonly property color borderSoft: menuColor("borderSecondary")
    readonly property color textStrong: menuColor("titleInk")
    readonly property color textMuted: menuColor("secondaryInk")
    readonly property color textOnAccent: menuColor("actionInk")
    readonly property color scrollbarHandle: pageTheme && pageTheme.scrollbarHandle ? pageTheme.scrollbarHandle : borderStrong
    readonly property color scrollbarTrack: pageTheme && pageTheme.scrollbarTrack ? pageTheme.scrollbarTrack : panelBgSoft
    readonly property color iconStroke: pageTheme && pageTheme.iconStroke ? pageTheme.iconStroke : textStrong
    readonly property color iconFill: pageTheme && pageTheme.iconFill ? pageTheme.iconFill : panelBgStrong
    readonly property color unknownText: pageTheme && pageTheme.unknownText ? pageTheme.unknownText : textMuted
    readonly property int cardRadius: 4
    readonly property int cardSpacing: 4
    readonly property int discoveredCount: CatalogMeta.discoveredCount(
        fruitLibraryModel,
        debugDiscoveredTypes,
        debugDiscoverAll)

    anchors.fill: parent
    color: pageBg
    visible: active
    clip: true

    Rectangle {
        anchors.fill: parent
        anchors.margins: 6
        color: Qt.rgba(libraryLayer.panelBgStrong.r, libraryLayer.panelBgStrong.g, libraryLayer.panelBgStrong.b, 0.42)
        border.color: libraryLayer.borderStrong
        border.width: 1
    }

    Column {
        id: libraryColumn
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        Components.SectionHeader {
            id: headerPanel
            width: parent.width
            height: 30
            color: Qt.rgba(libraryLayer.panelBgStrong.r, libraryLayer.panelBgStrong.g, libraryLayer.panelBgStrong.b, 0.86)
            border.color: libraryLayer.borderStrong
            titleText: "CATALOG"
            subtitleText: `${libraryLayer.discoveredCount}/${fruitLibraryModel.length} DISCOVERED`
            gameFont: gameFont
            textColor: libraryLayer.textStrong
            subtitleColor: Qt.rgba(libraryLayer.textMuted.r, libraryLayer.textMuted.g, libraryLayer.textMuted.b, 0.92)
        }

        Components.RetroWindowList {
            id: libraryList
            width: parent.width
            height: Math.max(
                        0,
                        libraryColumn.height - headerPanel.height - footerPanel.height - (libraryColumn.spacing * 2))
            model: fruitLibraryModel
            selectedIndex: libraryLayer.libraryIndex
            visibleCount: 3
            itemHeight: Math.floor((height - (libraryLayer.cardSpacing * (visibleCount - 1))) / visibleCount)
            spacing: libraryLayer.cardSpacing
            scrollbarHandle: libraryLayer.scrollbarHandle
            scrollbarTrack: libraryLayer.scrollbarTrack
            flashColor: Qt.rgba(libraryLayer.textOnAccent.r, libraryLayer.textOnAccent.g, libraryLayer.textOnAccent.b, 0.22)

            onRequestIndexChange: (nextIndex) => {
                if (nextIndex !== libraryLayer.libraryIndex && libraryLayer.setLibraryIndex) {
                    libraryLayer.setLibraryIndex(nextIndex)
                }
            }

            Repeater {
                model: libraryList.visibleCount

                delegate: Components.ListCardFrame {
                    id: libraryCard
                    readonly property int modelIndex: libraryList.modelIndexAt(index)
                    readonly property var entryData: libraryList.entryAt(index)
                    readonly property bool cardSelected: modelIndex === libraryLayer.libraryIndex
                    readonly property var resolvedEntry: entryData ? CatalogMeta.resolveEntry(
                        entryData,
                        libraryLayer.debugDiscoveredTypes,
                        libraryLayer.debugDiscoverAll) : ({ "discovered": false, "name": "", "description": "" })
                    readonly property color labelColor: cardSelected ? libraryLayer.textOnAccent : libraryLayer.textStrong
                    readonly property color descColor: cardSelected
                                                       ? Qt.rgba(libraryLayer.textOnAccent.r, libraryLayer.textOnAccent.g, libraryLayer.textOnAccent.b, 0.92)
                                                       : Qt.rgba(libraryLayer.textMuted.r, libraryLayer.textMuted.g, libraryLayer.textMuted.b, 0.96)

                    x: 0
                    y: index * (height + libraryList.spacing)
                    width: libraryList.width - 12
                    height: libraryList.itemHeight
                    visible: entryData !== null
                    radius: libraryLayer.cardRadius
                    selected: cardSelected
                    normalFill: Qt.rgba(libraryLayer.panelBg.r, libraryLayer.panelBg.g, libraryLayer.panelBg.b, 0.84)
                    selectedFill: Qt.rgba(libraryLayer.panelAccent.r, libraryLayer.panelAccent.g, libraryLayer.panelAccent.b, 0.92)
                    borderColor: cardSelected ? libraryLayer.borderStrong : libraryLayer.borderSoft

                    Row {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 12

                        Item {
                            width: 24
                            height: 24
                            anchors.verticalCenter: parent.verticalCenter

                            Icons.PowerIcon {
                                anchors.centerIn: parent
                                width: 20
                                height: 20
                                visible: entryData !== null && resolvedEntry.discovered
                                radius: 4
                                contentMargin: 2
                                fillColor: cardSelected
                                           ? Qt.rgba(libraryLayer.panelBgStrong.r, libraryLayer.panelBgStrong.g,
                                                     libraryLayer.panelBgStrong.b, 0.88)
                                           : Qt.rgba(libraryLayer.iconFill.r, libraryLayer.iconFill.g,
                                                     libraryLayer.iconFill.b, 0.96)
                                borderColor: cardSelected
                                             ? Qt.rgba(libraryLayer.textOnAccent.r, libraryLayer.textOnAccent.g,
                                                       libraryLayer.textOnAccent.b, 0.72)
                                             : (entryData !== null
                                                ? Qt.rgba(powerColor(entryData.type).r, powerColor(entryData.type).g,
                                                          powerColor(entryData.type).b, 0.72)
                                                : libraryLayer.borderSoft)
                                borderWidth: entryData !== null && entryData.type >= 9 ? 2 : 1
                                powerType: entryData !== null ? Number(entryData.type) : 0
                                glyphColor: entryData !== null
                                            ? (cardSelected ? libraryLayer.textOnAccent : powerColor(entryData.type))
                                            : libraryLayer.textMuted
                            }

                            Text {
                                text: "?"
                                color: libraryLayer.unknownText
                                visible: entryData !== null && !resolvedEntry.discovered
                                anchors.centerIn: parent
                                font.bold: true
                                font.pixelSize: 12
                            }
                        }

                        Column {
                            width: parent.width - 50
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                text: resolvedEntry.name
                                color: libraryCard.labelColor
                                font.family: gameFont
                                font.pixelSize: 11
                                font.bold: true
                            }

                            Text {
                                text: resolvedEntry.description
                                color: libraryCard.descColor
                                font.family: gameFont
                                font.pixelSize: 8
                                font.bold: true
                                opacity: 1.0
                                width: parent.width
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: footerPanel
            width: parent.width
            height: 16
            radius: 3
            color: Qt.rgba(libraryLayer.panelBgStrong.r, libraryLayer.panelBgStrong.g, libraryLayer.panelBgStrong.b, 0.82)
            border.color: libraryLayer.borderStrong
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "UP PREV   DOWN NEXT   SELECT MENU"
                color: libraryLayer.textMuted
                font.family: gameFont
                font.pixelSize: 8
                font.bold: true
            }
        }
    }
}
