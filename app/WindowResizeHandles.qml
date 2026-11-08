pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window

Item {
    id: handles
    required property var hostWindow
    readonly property int grip: 5
    enabled: hostWindow.visibility !== Window.Maximized && hostWindow.visibility !== Window.FullScreen
    z: 1000
    Repeater {
        model: [
            { edge: Qt.LeftEdge, x: 0, y: handles.grip, w: handles.grip, h: handles.height - 2*handles.grip, cursor: Qt.SizeHorCursor },
            { edge: Qt.RightEdge, x: handles.width - handles.grip, y: handles.grip, w: handles.grip, h: handles.height - 2*handles.grip, cursor: Qt.SizeHorCursor },
            { edge: Qt.TopEdge, x: handles.grip, y: 0, w: handles.width - 2*handles.grip, h: handles.grip, cursor: Qt.SizeVerCursor },
            { edge: Qt.BottomEdge, x: handles.grip, y: handles.height - handles.grip, w: handles.width - 2*handles.grip, h: handles.grip, cursor: Qt.SizeVerCursor },
            { edge: Qt.TopEdge | Qt.LeftEdge, x: 0, y: 0, w: handles.grip, h: handles.grip, cursor: Qt.SizeFDiagCursor },
            { edge: Qt.TopEdge | Qt.RightEdge, x: handles.width - handles.grip, y: 0, w: handles.grip, h: handles.grip, cursor: Qt.SizeBDiagCursor },
            { edge: Qt.BottomEdge | Qt.LeftEdge, x: 0, y: handles.height - handles.grip, w: handles.grip, h: handles.grip, cursor: Qt.SizeBDiagCursor },
            { edge: Qt.BottomEdge | Qt.RightEdge, x: handles.width - handles.grip, y: handles.height - handles.grip, w: handles.grip, h: handles.grip, cursor: Qt.SizeFDiagCursor }
        ]
        delegate: MouseArea {
            required property var modelData
            x: modelData.x; y: modelData.y; width: modelData.w; height: modelData.h
            cursorShape: modelData.cursor
            acceptedButtons: Qt.LeftButton
            onPressed: handles.hostWindow.startSystemResize(modelData.edge)
        }
    }
}
