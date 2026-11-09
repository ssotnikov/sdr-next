#include "spectrum_item.hpp"
#include <QQuickWindow>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGSimpleTextureNode>
#include <algorithm>
#include <cmath>
#include <memory>

class SpectrumItem::Scene : public QSGNode {
  public:
    struct Tile {
        std::unique_ptr<QSGTexture> texture;
        qint64 image_key = 0;
    };
    std::array<Tile, tile_count> tiles;
    std::array<QSGSimpleTextureNode *, tile_count + 1> strips{};
    QSGGeometryNode *line = nullptr;

    ~Scene() override {
        while (auto *node = firstChild()) {
            removeChildNode(node);
            delete node;
        }
    }
};

SpectrumItem::SpectrumItem(QQuickItem *parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    for (auto &tile : history_) {
        tile = QImage(bins, rows_per_tile, QImage::Format_RGB32);
        tile.fill(QColor("#0a0d10"));
    }
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
    v = std::clamp(v, 0, 2);
    if (color_map_ == v)
        return;
    color_map_ = v;
    for (auto &tile : history_)
        tile.fill(QColor("#0a0d10"));
    emit viewChanged();
    update();
}
void SpectrumItem::setTraceColor(const QColor &color) {
    if (!color.isValid() || trace_color_ == color)
        return;
    trace_color_ = color;
    emit viewChanged();
    update();
}
void SpectrumItem::setWaterfallStops(const QVariantList &stops) {
    if (stops == waterfall_stops_ || stops.size() < 2 || stops.size() > 64)
        return;
    QList<std::pair<double, QColor>> parsed;
    for (const auto &stop : stops) {
        const auto map = stop.toMap();
        bool ok = false;
        const double pos = map.value("pos").toDouble(&ok);
        const QColor color(map.value("color").toString());
        if (!ok || !std::isfinite(pos) || pos < 0 || pos > 1 || !color.isValid() ||
            (!parsed.isEmpty() && pos <= parsed.back().first))
            return;
        parsed.append({pos, color});
    }
    if (parsed.front().first != 0 || parsed.back().first != 1)
        return;
    waterfall_stops_ = stops;
    waterfall_lut_.resize(256);
    int segment = 1;
    for (int i = 0; i < 256; ++i) {
        const double level = i / 255.0;
        while (segment < parsed.size() - 1 && level > parsed[segment].first)
            ++segment;
        const auto &[lo, a] = parsed[segment - 1];
        const auto &[hi, b] = parsed[segment];
        const double f = (level - lo) / (hi - lo);
        const auto channel = [f](int x, int y) { return qRound(x + (y - x) * f); };
        waterfall_lut_[i] = qRgb(channel(a.red(), b.red()), channel(a.green(), b.green()),
                                 channel(a.blue(), b.blue()));
    }
    emit viewChanged();
    update();
}
QColor SpectrumItem::waterfallColor(double level) const {
    level = std::isfinite(level) ? std::clamp(level, 0.0, 1.0) : 0.0;
    if (color_map_ == 2 && !waterfall_lut_.isEmpty())
        return QColor::fromRgb(waterfall_lut_[qRound(level * 255)]);
    if (color_map_ == 1)
        return QColor::fromRgbF(level, level * 0.75, level * 0.25);
    return QColor::fromHsvF(0.68 - 0.55 * level, 0.8, 0.08 + 0.92 * level);
}
void SpectrumItem::setFrame(const QList<float> &frame) {
    if (frame.size() != bins)
        return;
    frame_ = frame;
    newest_row_ = (newest_row_ + history_rows - 1) % history_rows;
    auto &tile = history_[newest_row_ / rows_per_tile];
    auto *row = reinterpret_cast<QRgb *>(tile.scanLine(newest_row_ % rows_per_tile));
    for (int i = 0; i < bins; ++i) {
        const double level = std::clamp((frame[i] + 110) / 95.0, 0.0, 1.0);
        row[i] = waterfallColor(level).rgb();
    }
    update();
}
QSGNode *SpectrumItem::updatePaintNode(QSGNode *old, UpdatePaintNodeData *) {
    auto *root = static_cast<Scene *>(old);
    if (!root) {
        root = new Scene;
        auto *line = new QSGGeometryNode;
        auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), bins);
        geometry->setDrawingMode(QSGGeometry::DrawLineStrip);
        line->setGeometry(geometry);
        line->setFlag(QSGNode::OwnsGeometry);
        auto *material = new QSGFlatColorMaterial;
        material->setColor(trace_color_);
        line->setMaterial(material);
        line->setFlag(QSGNode::OwnsMaterial);
        root->line = line;
        root->appendChildNode(line);
    }
    // Keep the old textures alive until every strip has moved to its new texture.
    std::array<std::unique_ptr<QSGTexture>, tile_count> previous;
    for (int i = 0; i < tile_count; ++i) {
        auto &tile = root->tiles[i];
        const auto key = history_[i].cacheKey();
        if (tile.image_key != key) {
            if (auto *next = window()->createTextureFromImage(history_[i])) {
                previous[i] = std::move(tile.texture);
                tile.texture.reset(next);
                tile.image_key = key;
            }
        }
    }
    const qreal spectrumHeight = height() * 0.36;
    const qreal row_height = (height() - spectrumHeight) / history_rows;
    int drawn = 0;
    int row = newest_row_;
    for (auto *&strip : root->strips) {
        if (drawn == history_rows) {
            if (strip) {
                root->removeChildNode(strip);
                delete strip;
                strip = nullptr;
            }
            continue;
        }
        const int offset = row % rows_per_tile;
        const int count = std::min(rows_per_tile - offset, history_rows - drawn);
        auto *texture = root->tiles[row / rows_per_tile].texture.get();
        if (texture) {
            if (!strip) {
                strip = new QSGSimpleTextureNode;
                strip->setTexture(texture);
                root->insertChildNodeBefore(strip, root->line);
            } else if (strip->texture() != texture) {
                strip->setTexture(texture);
            }
            strip->setRect(0, spectrumHeight + drawn * row_height, width(), count * row_height);
            strip->setSourceRect((position_ - span_ / 2) * bins, offset, span_ * bins, count);
        } else if (strip) {
            root->removeChildNode(strip);
            delete strip;
            strip = nullptr;
        }
        drawn += count;
        row = (row + count) % history_rows;
    }
    auto *line = root->line;
    static_cast<QSGFlatColorMaterial *>(line->material())->setColor(trace_color_);
    line->markDirty(QSGNode::DirtyMaterial);
    auto *points = line->geometry()->vertexDataAsPoint2D();
    for (int i = 0; i < bins; ++i) {
        const auto bin = std::clamp(
            static_cast<int>((position_ - span_ / 2 + span_ * i / (bins - 1)) * (bins - 1)),
            0, bins - 1);
        const double value = frame_.isEmpty() ? -120 : frame_[bin];
        points[i].set(static_cast<float>(width() * i / (bins - 1)),
                      static_cast<float>(spectrumHeight * std::clamp(-value / 120, 0.0, 1.0)));
    }
    line->markDirty(QSGNode::DirtyGeometry);
    return root;
}
