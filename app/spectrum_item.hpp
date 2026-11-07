#pragma once
#include <QImage>
#include <QQuickItem>

class SpectrumItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(double span READ span WRITE setSpan NOTIFY viewChanged)
    Q_PROPERTY(double position READ position WRITE setPosition NOTIFY viewChanged)
    Q_PROPERTY(int colorMap READ colorMap WRITE setColorMap NOTIFY viewChanged)
  public:
    explicit SpectrumItem(QQuickItem *parent = nullptr);
    double span() const { return span_; }
    void setSpan(double);
    double position() const { return position_; }
    void setPosition(double);
    int colorMap() const { return color_map_; }
    void setColorMap(int);
    Q_INVOKABLE void setFrame(const QList<float> &);
  signals:
    void viewChanged();

  protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

  private:
    double span_ = 1, position_ = 0.5;
    int color_map_ = 0;
    bool dirty_ = true;
    QList<float> frame_;
    QImage history_{2048, 256, QImage::Format_RGB32};
};
