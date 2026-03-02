import QtQuick

Item {
    id: host

    property bool fullUiMode: true
    property bool screenOnlyUiMode: false
    property bool shellOnlyUiMode: false
    property int shellBaseWidth: 350
    property int shellBaseHeight: 620
    property int screenBaseWidth: 240
    property int screenBaseHeight: 216
    property var commandController
    property var inputController
    property var themeViewModel
    property var audioSettingsViewModel
    property var uiRuntimeState
    property color p0
    property color p1
    property color p2
    property color p3
    property string gameFont: ""
    property real elapsed: 0.0

    readonly property var bridge: shellBridge
    readonly property var screenItem: fullUiMode
                                     ? (fullShellLoader.item ? fullShellLoader.item.screenItem : null)
                                     : (screenOnlyUiMode ? screenOnlyLoader.item : null)

    ShellBridge {
        id: shellBridge
    }

    Component {
        id: screenComponent

        ScreenView {
            commandController: host.commandController
            themeViewModel: host.themeViewModel
            p0: host.p0
            p1: host.p1
            p2: host.p2
            p3: host.p3
            gameFont: host.gameFont
            elapsed: host.elapsed
            iconDebugMode: host.uiRuntimeState.iconDebugMode
            botDebugPanelVisible: host.uiRuntimeState.botDebugPanelVisible
            staticDebugScene: host.uiRuntimeState.staticDebugScene
            staticDebugOptions: host.uiRuntimeState.staticDebugOptions
        }
    }

    Component {
        id: shellOnlyScreenPlaceholder

        Rectangle {
            color: Qt.rgba(host.p0.r, host.p0.g, host.p0.b, 0.96)
            border.color: Qt.rgba(host.p1.r, host.p1.g, host.p1.b, 0.42)
            border.width: 1
        }
    }

    Component {
        id: fullShellComponent

        Shell {
            bridge: shellBridge
            commandController: host.commandController
            inputController: host.inputController
            shellColor: host.themeViewModel.shellColor
            shellThemeName: host.themeViewModel.shellName
            volume: host.audioSettingsViewModel.volume
            screenContentComponent: screenComponent
        }
    }

    Component {
        id: shellOnlyComponent

        Shell {
            bridge: shellBridge
            commandController: host.commandController
            inputController: host.inputController
            shellColor: host.themeViewModel.shellColor
            shellThemeName: host.themeViewModel.shellName
            volume: host.audioSettingsViewModel.volume
            screenContentComponent: shellOnlyScreenPlaceholder
        }
    }

    Item {
        id: shellHost
        visible: !host.screenOnlyUiMode
        width: host.shellBaseWidth
        height: host.shellBaseHeight
        anchors.centerIn: parent
        scale: Math.min(host.width / host.shellBaseWidth,
                        host.height / host.shellBaseHeight)

        Loader {
            id: fullShellLoader
            anchors.fill: parent
            active: host.fullUiMode
            sourceComponent: fullShellComponent
        }

        Loader {
            id: shellOnlyLoader
            anchors.fill: parent
            active: host.shellOnlyUiMode
            sourceComponent: shellOnlyComponent
        }
    }

    Item {
        id: screenOnlyHost
        visible: host.screenOnlyUiMode
        width: host.screenBaseWidth
        height: host.screenBaseHeight
        anchors.centerIn: parent
        scale: Math.min(host.width / host.screenBaseWidth,
                        host.height / host.screenBaseHeight)

        Loader {
            id: screenOnlyLoader
            anchors.fill: parent
            active: host.screenOnlyUiMode
            sourceComponent: screenComponent
        }
    }
}
