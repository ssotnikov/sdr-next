#include "window_chrome.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        const QSizeF size(1000, 650);
        const auto check = [&](QPointF point, Qt::Edges expected) {
            if (windowResizeEdges(point, size, 5, true) != expected)
                throw std::runtime_error("Window border hit-test regression");
        };
        check({2, 200}, Qt::LeftEdge);
        check({998, 200}, Qt::RightEdge);
        check({200, 2}, Qt::TopEdge);
        check({200, 648}, Qt::BottomEdge);
        check({2, 2}, Qt::TopEdge | Qt::LeftEdge);
        check({998, 2}, Qt::TopEdge | Qt::RightEdge);
        check({2, 648}, Qt::BottomEdge | Qt::LeftEdge);
        check({998, 648}, Qt::BottomEdge | Qt::RightEdge);
        check({5, 5}, {});
        check({500, 22}, {});
        check({-1, 100}, {});
        check({1001, 100}, {});
        check({995.25, 100}, Qt::RightEdge);
        if (windowResizeEdges({1, 1}, size, 5, false) || windowResizeEdges({1, 1}, size, 0, true))
            throw std::runtime_error("Disabled border permits resizing");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
