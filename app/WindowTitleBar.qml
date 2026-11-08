pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Rectangle {
    id: bar
    required property var hostWindow
    required property var theme
    property bool menuEnabled: true
    property bool menuOpen: false
    property string testCaption: ""
    property bool russian: true
    readonly property bool maximized: hostWindow.visibility === Window.Maximized
    signal menuRequested()
    implicitHeight: 44
    color: theme.colors.bg1

    function toggleMaximized() {
        if (maximized) hostWindow.showNormal()
        else hostWindow.showMaximized()
    }

    RowLayout {
        id: leftGroup
        anchors.left: parent.left; anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12
        Button {
            id: burger
            objectName: "mainMenuButton"
            implicitWidth: 34; implicitHeight: 34
            enabled: bar.menuEnabled
            Accessible.name: bar.russian ? "Главное меню" : "Main menu"
            onClicked: bar.menuRequested()
            background: Rectangle {
                radius: 6
                color: burger.hovered || burger.down || bar.menuOpen ? bar.theme.colors.bg3 : "transparent"
                border.width: burger.visualFocus ? 2 : 0
                border.color: bar.theme.colors.focus
            }
            contentItem: Item {
                opacity: burger.enabled ? 1 : bar.theme.tokens.opacity.disabled
                Repeater {
                    model: 3
                    Rectangle {
                        required property int index
                        width: 14; height: 1
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: (parent.height - 1) / 2 + (index - 1) * 5
                        color: bar.theme.colors.inkMuted
                    }
                }
            }
        }
        Row {
            objectName: "titleWordmark"
            spacing: 4
            Accessible.role: Accessible.StaticText
            Accessible.name: "SDR Next"
            Label {
                objectName: "wordmarkSdr"
                text: "SDR"
                color: bar.theme.colors.ink
                font.family: bar.theme.sansSemibold
                font.pixelSize: 16
            }
            Label {
                objectName: "wordmarkNext"
                text: "Next"
                color: bar.theme.colors.dialInk
                font.family: bar.theme.sansSemibold
                font.pixelSize: 16
            }
        }
        Label {
            text: "0.1"; color: bar.theme.colors.inkMuted
            font.family: bar.theme.sans; font.pixelSize: bar.theme.text.caption.size
        }
    }
    Item {
        id: dragRegion
        objectName: "titleDragRegion"
        anchors.left: leftGroup.right; anchors.leftMargin: 12
        anchors.right: captionButtons.left; anchors.top: parent.top; anchors.bottom: parent.bottom
        Label {
            anchors.fill: parent; anchors.rightMargin: 12
            text: bar.testCaption
            color: bar.theme.colors.inkMuted
            font.family: bar.theme.sans; font.pixelSize: bar.theme.text.caption.size
            verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
        }
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onPressed: bar.hostWindow.startSystemMove()
            onDoubleClicked: bar.toggleMaximized()
        }
    }
    component CaptionButton: Button {
        id: control
        property int kind: 0
        readonly property color glyphColor: kind === 2 && hovered ? bar.theme.colors.bg0 : bar.theme.colors.inkMuted
        implicitWidth: 46; implicitHeight: 44
        background: Rectangle {
            anchors.centerIn: parent; width: 30; height: 28; radius: 5
            color: control.hovered || control.down ? (control.kind === 2 ? bar.theme.colors.danger : bar.theme.colors.bg3) : "transparent"
            border.width: control.visualFocus ? 2 : 0
            border.color: bar.theme.colors.focus
        }
        contentItem: Item {
            Rectangle { visible: control.kind === 0; anchors.centerIn: parent; width: 12; height: 1; color: control.glyphColor }
            Rectangle {
                visible: control.kind === 1
                anchors.centerIn: parent
                width: 11; height: 10; color: "transparent"; border.width: 1; border.color: control.glyphColor
                Rectangle {
                    visible: bar.maximized
                    x: 3; y: -3; width: 10; height: 9; color: "transparent"
                    border.width: 1; border.color: control.glyphColor
                }
            }
            Rectangle { visible: control.kind === 2; anchors.centerIn: parent; width: 14; height: 1; rotation: 45; color: control.glyphColor }
            Rectangle { visible: control.kind === 2; anchors.centerIn: parent; width: 14; height: 1; rotation: -45; color: control.glyphColor }
        }
    }
    Row {
        id: captionButtons
        anchors.right: parent.right; anchors.top: parent.top
        CaptionButton {
            objectName: "minimizeButton"; kind: 0
            Accessible.name: bar.russian ? "Свернуть" : "Minimize"
            onClicked: bar.hostWindow.showMinimized()
        }
        CaptionButton {
            objectName: "maximizeButton"; kind: 1
            Accessible.name: bar.maximized ? (bar.russian ? "Восстановить" : "Restore") : (bar.russian ? "Развернуть" : "Maximize")
            onClicked: bar.toggleMaximized()
        }
        CaptionButton {
            objectName: "closeWindowButton"; kind: 2
            Accessible.name: bar.russian ? "Закрыть" : "Close"
            onClicked: bar.hostWindow.close()
        }
    }
}
