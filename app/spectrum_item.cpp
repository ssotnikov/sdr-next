#include "spectrum_item.hpp"
#include <QQuickWindow>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGSimpleTextureNode>
#include <algorithm>
#include <cmath>
#include <cstring>

SpectrumItem::SpectrumItem(QQuickItem *parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    history_.fill(QColor("#101c2c"));
}
void SpectrumItem::setSpan(double v) {
    v = std::clamp(v, 0.02, 1.0);
    if (span_ == v)
        return;
    span_ = v;
    position_ = std::clamp(position_, span_ / 2, 1 - span_ / 2);
    emit viewChanged();
    update();
}
void SpectrumItem::setPosition(double v) {
    v = std::clamp(v, span_ / 2, 1 - span_ / 2);
    if (position_ == v)
        return;
    position_ = v;
    emit viewChanged();
    update();
}
void SpectrumItem::setColorMap(int v) {
    color_map_ = v;
    emit viewChanged();
    update();
}
void SpectrumItem::setFrame(const QList<float> &frame) {
    if (frame.size() != 2048)
        return;
    frame_ = frame;
    std::memmove(history_.bits() + history_.bytesPerLine(), history_.bits(),
                 static_cast<std::size_t>(history_.bytesPerLine()) * (history_.height() - 1));
    auto *row = reinterpret_cast<QRgb *>(history_.scanLine(0));
    for (int i = 0; i < history_.width(); ++i) {
        const double level = std::clamp((frame[i] + 110) / 95.0, 0.0, 1.0);
        QColor color;
        if (color_map_ == 1)
            color = QColor::fromRgbF(level, level * 0.75, level * 0.25);
        else
            color = QColor::fromHsvF(0.68 - 0.55 * level, 0.8, 0.08 + 0.92 * level);
        row[i] = color.rgb();
    }
    dirty_ = true;
    update();
}
QSGNode *SpectrumItem::updatePaintNode(QSGNode *old, UpdatePaintNodeData *) {
    auto *root = old;
    if (!root) {
        root = new QSGNode;
        auto *texture = new QSGSimpleTextureNode;
        texture->setOwnsTexture(true);
        root->appendChildNode(texture);
        auto *line = new QSGGeometryNode;
        auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 2048);
        geometry->setDrawingMode(QSGGeometry::DrawLineStrip);
        line->setGeometry(geometry);
        line->setFlag(QSGNode::OwnsGeometry);
        auto *material = new QSGFlatColorMaterial;
        material->setColor(QColor("#7ee4e0"));
        line->setMaterial(material);
        line->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(line);
        dirty_ = true;
    }
    auto *texture = static_cast<QSGSimpleTextureNode *>(root->firstChild());
    if (dirty_) {
        auto *next = window()->createTextureFromImage(history_);
        if (next) {
            auto *previous = texture->texture();
            // Detach while the old texture is still valid: setTexture inspects it.
            texture->setOwnsTexture(false);
            texture->setTexture(next);
            texture->setOwnsTexture(true);
            delete previous;
            dirty_ = false;
        }
    }
    const qreal spectrumHeight = height() * 0.36;
    texture->setRect(0, spectrumHeight, width(), height() - spectrumHeight);
    texture->setSourceRect((position_ - span_ / 2) * history_.width(), 0, span_ * history_.width(),
                           history_.height());
    auto *line = static_cast<QSGGeometryNode *>(root->lastChild());
    auto *points = line->geometry()->vertexDataAsPoint2D();
    for (int i = 0; i < 2048; ++i) {
        const auto bin = std::clamp(
            static_cast<int>((position_ - span_ / 2 + span_ * i / 2047) * 2047), 0, 2047);
        const double value = frame_.isEmpty() ? -120 : frame_[bin];
        points[i].set(static_cast<float>(width() * i / 2047),
                      static_cast<float>(spectrumHeight * std::clamp(-value / 120, 0.0, 1.0)));
    }
    line->markDirty(QSGNode::DirtyGeometry);
    return root;
}
