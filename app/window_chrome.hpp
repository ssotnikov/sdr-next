#pragma once
#include <QAbstractNativeEventFilter>
#include <QPointF>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>

// Coordinates and border width are logical pixels, independent of monitor DPI.
Qt::Edges windowResizeEdges(QPointF point, QSizeF size, qreal border, bool enabled);

class WindowChrome final : public QAbstractNativeEventFilter {
  public:
    explicit WindowChrome(QQuickWindow *window);
    ~WindowChrome() override;
    bool nativeEventFilter(const QByteArray &, void *, qintptr *) override;

  private:
    QPointer<QQuickWindow> window_;
    QPointer<QQuickItem> drag_region_;
    QPointer<QQuickItem> resize_handles_;
};
