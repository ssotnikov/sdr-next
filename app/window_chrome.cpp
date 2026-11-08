#include "window_chrome.hpp"
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

Qt::Edges windowResizeEdges(QPointF point, QSizeF size, qreal border, bool enabled) {
    if (!enabled || border <= 0 || !QRectF(QPointF(), size).contains(point))
        return {};
    Qt::Edges edges;
    if (point.x() < border)
        edges |= Qt::LeftEdge;
    else if (point.x() >= size.width() - border)
        edges |= Qt::RightEdge;
    if (point.y() < border)
        edges |= Qt::TopEdge;
    else if (point.y() >= size.height() - border)
        edges |= Qt::BottomEdge;
    return edges;
}

WindowChrome::WindowChrome(QQuickWindow *window) : window_(window) {
    if (window_) {
        drag_region_ = window_->findChild<QQuickItem *>("titleDragRegion");
        resize_handles_ = window_->findChild<QQuickItem *>("windowResizeHandles");
    }
#ifdef Q_OS_WIN
    QCoreApplication::instance()->installNativeEventFilter(this);
#endif
}
WindowChrome::~WindowChrome() {
#ifdef Q_OS_WIN
    QCoreApplication::instance()->removeNativeEventFilter(this);
#endif
}
bool WindowChrome::nativeEventFilter(const QByteArray &, void *message, qintptr *result) {
#ifdef Q_OS_WIN
    const auto *msg = static_cast<MSG *>(message);
    if (!window_ || msg->message != WM_NCHITTEST ||
        msg->hwnd != reinterpret_cast<HWND>(window_->winId()))
        return false;
    POINT client{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
    if (!ScreenToClient(msg->hwnd, &client))
        return false;
    const qreal scale = window_->devicePixelRatio();
    const QPointF point(client.x / scale, client.y / scale);
    const auto edges = windowResizeEdges(
        point, window_->size(), resize_handles_ ? resize_handles_->property("grip").toReal() : 5,
        window_->visibility() == QWindow::Windowed);
    if (edges) {
        if (edges == (Qt::TopEdge | Qt::LeftEdge))
            *result = HTTOPLEFT;
        else if (edges == (Qt::TopEdge | Qt::RightEdge))
            *result = HTTOPRIGHT;
        else if (edges == (Qt::BottomEdge | Qt::LeftEdge))
            *result = HTBOTTOMLEFT;
        else if (edges == (Qt::BottomEdge | Qt::RightEdge))
            *result = HTBOTTOMRIGHT;
        else if (edges == Qt::LeftEdge)
            *result = HTLEFT;
        else if (edges == Qt::RightEdge)
            *result = HTRIGHT;
        else if (edges == Qt::TopEdge)
            *result = HTTOP;
        else
            *result = HTBOTTOM;
        return true;
    }
    if (drag_region_ && drag_region_->isVisible() &&
        drag_region_->contains(drag_region_->mapFromScene(point))) {
        *result = HTCAPTION;
        return true;
    }
#else
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return false;
}
