#include "plyEngine/UIMargins.hpp"

#include "plyEngine/Image.hpp"

#include <cstddef>
#include <string>

namespace ply {

const char* toString(MarginErrorCode code) {
    switch (code) {
        case MarginErrorCode::None: return "None";
        case MarginErrorCode::RegionOutOfBounds: return "RegionOutOfBounds";
        case MarginErrorCode::NoCentre: return "NoCentre";
    }
    return "None";  // unreachable; every enumerator is handled
}

namespace {

MarginError fail(MarginErrorCode code, std::string detail) {
    return MarginError{code, std::string(toString(code)) + ": " + std::move(detail)};
}

/// Alpha at (x, y) of an RGBA8 image whose row 0 is the top of the image.
int alphaAt(const Image& image, int x, int y) {
    const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width()) *
                                 4u +
                              static_cast<std::size_t>(x) * 4u + 3u;
    return image.pixels()[index];
}

/// True when every pixel of column `x` in rows [y0, y0+h) has alpha `value`.
bool columnIs(const Image& image, int x, int y0, int h, int value) {
    for (int y = y0; y < y0 + h; ++y) {
        if (alphaAt(image, x, y) != value) return false;
    }
    return true;
}

/// True when every pixel of row `y` in columns [x0, x0+w) has alpha `value`.
bool rowIs(const Image& image, int y, int x0, int w, int value) {
    for (int x = x0; x < x0 + w; ++x) {
        if (alphaAt(image, x, y) != value) return false;
    }
    return true;
}

}  // namespace

MarginResult deriveMargins(const Image& image, const UIRect& region) {
    MarginResult result;
    if (!image.valid()) {
        result.error = fail(MarginErrorCode::RegionOutOfBounds, "the image is not loaded");
        return result;
    }
    if (region.w <= 0 || region.h <= 0 || region.x < 0 || region.y < 0 ||
        region.x + region.w > image.width() || region.y + region.h > image.height()) {
        result.error = fail(MarginErrorCode::RegionOutOfBounds,
                            std::to_string(region.x) + "," + std::to_string(region.y) + " " +
                                std::to_string(region.w) + "x" + std::to_string(region.h));
        return result;
    }

    // The contour is read from the alpha channel, so the rule is independent of
    // palette. Each side is measured against its own corner's alpha: top/left
    // from the top-left pixel, bottom/right from the bottom-right one, because
    // art may be padded asymmetrically.
    const int topLeftAlpha = alphaAt(image, region.x, region.y);
    const int bottomRightAlpha = alphaAt(image, region.x + region.w - 1, region.y + region.h - 1);

    int left = 0;
    while (left < region.w && columnIs(image, region.x + left, region.y, region.h, topLeftAlpha)) {
        ++left;
    }
    int right = 0;
    while (right < region.w &&
           columnIs(image, region.x + region.w - 1 - right, region.y, region.h, bottomRightAlpha)) {
        ++right;
    }
    int top = 0;
    while (top < region.h && rowIs(image, region.y + top, region.x, region.w, topLeftAlpha)) ++top;
    int bottom = 0;
    while (bottom < region.h &&
           rowIs(image, region.y + region.h - 1 - bottom, region.x, region.w, bottomRightAlpha)) {
        ++bottom;
    }

    // §3: "a source region must keep a centre" — caps that consume an axis
    // would leave nothing to slice. An opaque, margin-less region lands here:
    // its alpha contour says the whole region is border, which is the signal
    // that this art must declare its margins instead.
    if (left + right >= region.w || top + bottom >= region.h) {
        result.error = fail(MarginErrorCode::NoCentre,
                            "derived [" + std::to_string(left) + "," + std::to_string(top) + "," +
                                std::to_string(right) + "," + std::to_string(bottom) +
                                "] leaves no centre in " + std::to_string(region.w) + "x" +
                                std::to_string(region.h));
        return result;
    }

    result.margins = UIMargins{left, top, right, bottom};
    return result;
}

std::vector<DerivedMarginCheck> checkDerivedMargins(const UIAtlas& atlas, const Image& image,
                                                    int tolerancePx) {
    std::vector<DerivedMarginCheck> report;
    for (const UIAtlasRegion& region : atlas.regions) {
        if (region.marginSource != MarginSource::Derived) continue;

        DerivedMarginCheck entry;
        entry.region = region.name;
        entry.recorded = region.margins;

        const MarginResult derived =
            deriveMargins(image, UIRect{region.x, region.y, region.w, region.h});
        if (!derived.ok()) {
            entry.error = derived.error;
            entry.withinTolerance = false;
            report.push_back(std::move(entry));
            continue;
        }

        entry.derived = derived.margins;
        // Drift is recorded as derived-minus-recorded, so a positive number
        // means the art's padding grew.
        entry.dl = entry.derived.left - entry.recorded.left;
        entry.dt = entry.derived.top - entry.recorded.top;
        entry.dr = entry.derived.right - entry.recorded.right;
        entry.db = entry.derived.bottom - entry.recorded.bottom;

        const int tolerance = tolerancePx < 0 ? 0 : tolerancePx;
        const auto within = [tolerance](int delta) {
            return delta >= -tolerance && delta <= tolerance;
        };
        entry.withinTolerance =
            within(entry.dl) && within(entry.dt) && within(entry.dr) && within(entry.db);
        report.push_back(std::move(entry));
    }
    return report;
}

}  // namespace ply
