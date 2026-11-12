// Camera2D (M1b b7): follow with a deadzone, clamp to the map at all four
// edges, integer snap. The contract — the three stages in fixed order, the
// floor snap, the minimum-move deadzone, clamp-wins — is documented in
// Camera2D.hpp; this file is that contract, implemented in integer math
// after the one sanctioned floor.
#include "plyEngine/Camera2D.hpp"

#include <cmath>

namespace ply {

Camera2D::Camera2D(const Config& config) : config_(config) {
    // The documented initial state: the map centre.
    x_ = config_.mapWidth / 2;
    y_ = config_.mapHeight / 2;
    clampToMap();
}

int Camera2D::x() const { return x_; }
int Camera2D::y() const { return y_; }

UIRect Camera2D::viewRect() const {
    return {x_ - config_.viewWidth / 2, y_ - config_.viewHeight / 2, config_.viewWidth,
            config_.viewHeight};
}

void Camera2D::follow(double targetX, double targetY) {
    // 1. SNAP: the float stops at this floor — 10.4 and 10.6 are one
    //    target. Long long through the arithmetic (the F1 lesson: a
    //    position value must not wrap at a reachable width).
    const long long tx = static_cast<long long>(std::floor(targetX));
    const long long ty = static_cast<long long>(std::floor(targetY));
    // 2. DEADZONE: the MINIMUM integer move that brings the target back
    //    inside the rect centred on the camera — inside means no move at
    //    all (the no-jitter rule), crossing an edge moves to that edge.
    const long long halfW = config_.deadzoneWidth / 2;
    const long long halfH = config_.deadzoneHeight / 2;
    if (tx < static_cast<long long>(x_) - halfW) {
        x_ = static_cast<int>(tx + halfW);
    } else if (tx > static_cast<long long>(x_) + halfW) {
        x_ = static_cast<int>(tx - halfW);
    }
    if (ty < static_cast<long long>(y_) - halfH) {
        y_ = static_cast<int>(ty + halfH);
    } else if (ty > static_cast<long long>(y_) + halfH) {
        y_ = static_cast<int>(ty - halfH);
    }
    // 3. CLAMP: the view never leaves the map — the clamp wins over the
    //    deadzone at the edges.
    clampToMap();
}

void Camera2D::snapTo(double targetX, double targetY) {
    x_ = static_cast<int>(std::floor(targetX));
    y_ = static_cast<int>(std::floor(targetY));
    clampToMap();
}

void Camera2D::clampToMap() {
    // A map smaller than the view on an axis centres on that axis: there is
    // no view rect inside the map that fits.
    if (config_.mapWidth <= config_.viewWidth) {
        x_ = config_.mapWidth / 2;
    } else {
        const int minX = config_.viewWidth / 2;
        const int maxX = config_.mapWidth - config_.viewWidth + config_.viewWidth / 2;
        if (x_ < minX) x_ = minX;
        if (x_ > maxX) x_ = maxX;
    }
    if (config_.mapHeight <= config_.viewHeight) {
        y_ = config_.mapHeight / 2;
    } else {
        const int minY = config_.viewHeight / 2;
        const int maxY = config_.mapHeight - config_.viewHeight + config_.viewHeight / 2;
        if (y_ < minY) y_ = minY;
        if (y_ > maxY) y_ = maxY;
    }
}

}  // namespace ply
