#pragma once
#include <QImage>
#include <QQuickItem>
#include <QVariantList>

class SpectrumItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(double span READ span WRITE setSpan NOTIFY viewChanged)
    Q_PROPERTY(double position READ position WRITE setPosition NOTIFY viewChanged)
    Q_PROPERTY(int colorMap READ colorMap WRITE setColorMap NOTIFY viewChanged)
    Q_PROPERTY(QColor traceColor READ traceColor WRITE setTraceColor NOTIFY viewChanged)
    Q_PROPERTY(
        QVariantList waterfallStops READ waterfallStops WRITE setWaterfallStops NOTIFY viewChanged)
  public:
    explicit SpectrumItem(QQuickItem *parent = nullptr);
    double span() const { return span_; }
    void setSpan(double);
    double position() const { return position_; }
    void setPosition(double);
    int colorMap() const { return color_map_; }
    void setColorMap(int);
    QColor traceColor() const { return trace_color_; }
    void setTraceColor(const QColor &);
    QVariantList waterfallStops() const { return waterfall_stops_; }
    void setWaterfallStops(const QVariantList &);
    Q_INVOKABLE QColor waterfallColor(double level) const;
    Q_INVOKABLE void setFrame(const QList<float> &);
  signals:
    void viewChanged();

  protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

  private:
    double span_ = 1, position_ = 0.5;
    int color_map_ = 0;
    QColor trace_color_{"#8fd8ff"};
    QVariantList waterfall_stops_;
    QList<QRgb> waterfall_lut_;
    bool dirty_ = true;
    QList<float> frame_;
    QImage history_{2048, 256, QImage::Format_RGB32};
};
