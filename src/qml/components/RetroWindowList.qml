import QtQuick

Item {
    id: retroList

    property var model: []
    property int selectedIndex: 0
    property int visibleCount: 3
    property int itemHeight: 56
    property int spacing: 6
    property color scrollbarHandle: "white"
    property color scrollbarTrack: "black"
    property color flashColor: "white"
    property int windowStartIndex: 0
    readonly property int modelCount: model ? model.length : 0
    readonly property int maxWindowStart: Math.max(0, modelCount - visibleCount)
    readonly property real contentHeight: (visibleCount * itemHeight) + (Math.max(0, visibleCount - 1) * spacing)

    signal requestIndexChange(int nextIndex)

    default property alias contentData: contentRoot.data

    function clampIndex(index) {
        return Math.max(0, Math.min(modelCount - 1, index))
    }

    function entryAt(slotIndex) {
        const modelIndex = windowStartIndex + slotIndex
        if (modelIndex < 0 || modelIndex >= modelCount) {
            return null
        }
        return model[modelIndex]
    }

    function modelIndexAt(slotIndex) {
        const modelIndex = windowStartIndex + slotIndex
        if (modelIndex < 0 || modelIndex >= modelCount) {
            return -1
        }
        return modelIndex
    }

    function syncWindowToSelection() {
        if (modelCount <= 0) {
            windowStartIndex = 0
            return
        }

        const clampedSelection = clampIndex(selectedIndex)
        let nextWindowStart = windowStartIndex
        if (clampedSelection < nextWindowStart) {
            nextWindowStart = clampedSelection
        } else if (clampedSelection >= nextWindowStart + visibleCount) {
            nextWindowStart = clampedSelection - visibleCount + 1
        }
        nextWindowStart = Math.max(0, Math.min(maxWindowStart, nextWindowStart))
        if (nextWindowStart !== windowStartIndex) {
            windowStartIndex = nextWindowStart
            flash.restart()
        }
    }

    width: parent ? parent.width : 0
    height: contentHeight

    onSelectedIndexChanged: syncWindowToSelection()
    onModelChanged: syncWindowToSelection()
    onVisibleCountChanged: syncWindowToSelection()

    Component.onCompleted: syncWindowToSelection()

    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event) => {
            if (retroList.modelCount <= 0) {
                return
            }
            const delta = event.angleDelta.y < 0 ? 1 : -1
            const nextIndex = retroList.clampIndex(retroList.selectedIndex + delta)
            if (nextIndex !== retroList.selectedIndex) {
                retroList.requestIndexChange(nextIndex)
            }
            event.accepted = true
        }
    }

    Item {
        id: contentRoot
        anchors.fill: parent
    }

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 6
        radius: 3
        color: retroList.scrollbarTrack
        opacity: 0.35
    }

    Rectangle {
        readonly property real handleHeight: retroList.modelCount <= retroList.visibleCount
            ? retroList.height
            : Math.max(16, (retroList.visibleCount / Math.max(1, retroList.modelCount)) * retroList.height)
        readonly property real travel: Math.max(0, retroList.height - handleHeight)
        readonly property real progress: retroList.maxWindowStart <= 0
            ? 0
            : retroList.windowStartIndex / retroList.maxWindowStart

        anchors.right: parent.right
        width: 6
        height: handleHeight
        y: travel * progress
        radius: 3
        color: retroList.scrollbarHandle
    }

    SequentialAnimation {
        id: flash
        running: false

        NumberAnimation {
            target: flashOverlay
            property: "opacity"
            from: 0.0
            to: 0.12
            duration: 45
        }
        NumberAnimation {
            target: flashOverlay
            property: "opacity"
            from: 0.12
            to: 0.0
            duration: 80
        }
    }

    Rectangle {
        id: flashOverlay
        anchors.fill: parent
        color: retroList.flashColor
        opacity: 0
    }
}
