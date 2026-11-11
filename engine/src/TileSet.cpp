#include "plyEngine/TileSet.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace ply {

using json = nlohmann::json;

namespace {

MapError fail(MapErrorCode code, std::string detail) {
    return MapError{code, std::string(toString(code)) + ": " + std::move(detail)};
}

bool parseIntField(const json& doc, const char* key, int& out, MapError& error) {
    const auto it = doc.find(key);
    if (it == doc.end()) return (error = fail(MapErrorCode::MissingField, key), false);
    if (!it->is_number_integer()) return (error = fail(MapErrorCode::WrongType, key), false);
    out = it->get<int>();
    return true;
}

bool parseStringField(const json& doc, const char* key, std::string& out, MapError& error) {
    const auto it = doc.find(key);
    if (it == doc.end()) return (error = fail(MapErrorCode::MissingField, key), false);
    if (!it->is_string()) return (error = fail(MapErrorCode::WrongType, key), false);
    out = it->get<std::string>();
    return true;
}

bool parseDocument(std::string_view document, json& doc, MapError& error) {
    try {
        doc = json::parse(document);
    } catch (const json::exception&) {
        error = fail(MapErrorCode::MalformedJson, "the document does not parse");
        return false;
    }
    if (!doc.is_object()) {
        error = fail(MapErrorCode::WrongType, "document root");
        return false;
    }
    return true;
}

/// One px rect from a {x, y, w, h} object: x/y >= 0 and w/h > 0. Regions
/// live in the ATLAS image and may sit anywhere in it; only tile-local
/// collision rects are additionally bounds-checked against the tile (in
/// parseCollisionRect below).
bool parseRect(const json& node, UIRect& out, MapError& error) {
    if (!node.is_object()) {
        error = fail(MapErrorCode::WrongType, "rect");
        return false;
    }
    if (!parseIntField(node, "x", out.x, error)) return false;
    if (!parseIntField(node, "y", out.y, error)) return false;
    if (!parseIntField(node, "w", out.w, error)) return false;
    if (!parseIntField(node, "h", out.h, error)) return false;
    if (out.x < 0 || out.y < 0 || out.w <= 0 || out.h <= 0) {
        error = fail(MapErrorCode::NegativeRegion, "rect");
        return false;
    }
    return true;
}

/// A collision rect is TILE-LOCAL px: a rect reaching past the tile edge is
/// a schema violation, not something to clamp away.
bool parseCollisionRect(const json& node, UIRect& out, int tileWidth, int tileHeight,
                        MapError& error) {
    if (!parseRect(node, out, error)) return false;
    if (out.x + out.w > tileWidth || out.y + out.h > tileHeight) {
        error = fail(MapErrorCode::NegativeRegion, "collision rect outside the tile");
        return false;
    }
    return true;
}

}  // namespace

// The mask table's shared canonical source (b1.3): both functions are
// binary searches over the ONE table, so the loader's validation and the
// resolver's future lookup cannot drift to two tables.

bool isCanonicalBlobMask(int mask) {
    return std::binary_search(kBlobMasks, kBlobMasks + kBlobMaskCount, mask);
}

int blobMaskIndex(int mask) {
    const int* found =
        std::lower_bound(kBlobMasks, kBlobMasks + kBlobMaskCount, mask);
    if (found == kBlobMasks + kBlobMaskCount || *found != mask) return -1;
    return static_cast<int>(found - kBlobMasks);
}

const TileData* TileSet::findTile(int id) const {
    for (const TileData& tile : tiles) {
        if (tile.id == id) return &tile;
    }
    return nullptr;
}

TileSetLoadResult loadTileSetFromString(std::string_view document) {
    TileSetLoadResult result;
    json doc;
    if (!parseDocument(document, doc, result.error)) return result;

    // Parsed into `staged` and moved into the result only on success, so a
    // failed load is an empty result, not a partial tileset — at any failure
    // point, however late in the document it happens.
    TileSet staged;
    if (!parseStringField(doc, "name", staged.name, result.error)) return result;
    if (staged.name.empty()) {
        result.error = fail(MapErrorCode::EmptyTilesetName, "name");
        return result;
    }
    if (!parseStringField(doc, "image", staged.image, result.error)) return result;
    if (!parseIntField(doc, "tile_width", staged.tileWidth, result.error)) return result;
    if (!parseIntField(doc, "tile_height", staged.tileHeight, result.error)) return result;
    if (staged.tileWidth <= 0 || staged.tileHeight <= 0) {
        result.error = fail(MapErrorCode::NonPositiveSize, "tile_width/tile_height");
        return result;
    }

    const auto tiles = doc.find("tiles");
    if (tiles == doc.end()) {
        result.error = fail(MapErrorCode::MissingField, "tiles");
        return result;
    }
    if (!tiles->is_array()) {
        result.error = fail(MapErrorCode::WrongType, "tiles");
        return result;
    }
    for (std::size_t i = 0; i < tiles->size(); ++i) {
        const json& entry = (*tiles)[i];
        if (!entry.is_object()) {
            result.error = fail(MapErrorCode::WrongType, "tiles[" + std::to_string(i) + "]");
            return result;
        }
        TileData tile;
        if (!parseIntField(entry, "id", tile.id, result.error)) return result;
        // Layer cells use negative ids for empty, so a tileset tile with a
        // negative id could never be indexed by a map (b1.2).
        if (tile.id < 0) {
            result.error = fail(MapErrorCode::OutOfRangeTileId,
                                "tileset tile id " + std::to_string(tile.id));
            return result;
        }

        const auto region = entry.find("region");
        if (region == entry.end()) {
            result.error = fail(MapErrorCode::MissingField, "region");
            return result;
        }
        if (!parseRect(*region, tile.region, result.error)) {
            return result;
        }

        // `solid` is optional and defaults to false (the a9 rule).
        const auto solid = entry.find("solid");
        if (solid != entry.end()) {
            if (!solid->is_boolean()) {
                result.error = fail(MapErrorCode::WrongType, "solid");
                return result;
            }
            tile.solid = solid->get<bool>();
        }

        // --- b1 metadata (all optional, all defaulted) ---
        const auto mask = entry.find("mask");
        if (mask != entry.end()) {
            if (!mask->is_number_integer()) {
                result.error = fail(MapErrorCode::WrongType, "mask");
                return result;
            }
            tile.mask = mask->get<int>();
            // {-1} (no role) or one of the 256 eight-neighbour masks...
            if (tile.mask < kNoTileMask || tile.mask > 255) {
                result.error = fail(MapErrorCode::MaskOutOfRange,
                                    "tile " + std::to_string(tile.id) + " mask " +
                                        std::to_string(tile.mask));
                return result;
            }
            // ...and in-range, one of the 47 canonical blob masks (b1.2,
            // wired through the shared predicate in b1.3): an in-range value
            // outside kBlobMasks sets a diagonal bit without both adjacent
            // edges, which no blob tile represents and the resolver could
            // never resolve.
            if (tile.mask != kNoTileMask && !isCanonicalBlobMask(tile.mask)) {
                result.error = fail(MapErrorCode::UnknownMask,
                                    "tile " + std::to_string(tile.id) + " mask " +
                                        std::to_string(tile.mask));
                return result;
            }
        }

        const auto variant = entry.find("variant");
        if (variant != entry.end()) {
            if (!variant->is_string()) {
                result.error = fail(MapErrorCode::WrongType, "variant");
                return result;
            }
            tile.variant = variant->get<std::string>();
        }

        const auto palette = entry.find("palette");
        if (palette != entry.end()) {
            if (!palette->is_string()) {
                result.error = fail(MapErrorCode::WrongType, "palette");
                return result;
            }
            tile.palette = palette->get<std::string>();
        }

        const auto phase = entry.find("phase");
        if (phase != entry.end()) {
            if (!phase->is_number_integer()) {
                result.error = fail(MapErrorCode::WrongType, "phase");
                return result;
            }
            tile.phase = phase->get<int>();
        }

        const auto animation = entry.find("animation");
        if (animation != entry.end() && !animation->is_null()) {
            if (!animation->is_array()) {
                result.error = fail(MapErrorCode::WrongType, "animation");
                return result;
            }
            for (std::size_t f = 0; f < animation->size(); ++f) {
                const json& frame = (*animation)[f];
                if (!frame.is_object()) {
                    result.error = fail(MapErrorCode::WrongType,
                                        "animation[" + std::to_string(f) + "]");
                    return result;
                }
                TileAnimFrame parsed;
                const auto frameRegion = frame.find("region");
                if (frameRegion == frame.end()) {
                    result.error = fail(MapErrorCode::MissingField, "animation region");
                    return result;
                }
                if (!parseRect(*frameRegion, parsed.region, result.error)) {
                    return result;
                }
                if (!parseIntField(frame, "steps", parsed.steps, result.error)) return result;
                if (parsed.steps <= 0) {
                    result.error = fail(MapErrorCode::NonPositiveSize,
                                        "animation[" + std::to_string(f) + "] steps");
                    return result;
                }
                tile.animation.push_back(parsed);
            }
        }

        const auto collision = entry.find("collision");
        if (collision != entry.end() && !collision->is_null()) {
            if (!collision->is_array()) {
                result.error = fail(MapErrorCode::WrongType, "collision");
                return result;
            }
            for (std::size_t c = 0; c < collision->size(); ++c) {
                UIRect rect;
                if (!parseCollisionRect((*collision)[c], rect, staged.tileWidth,
                                        staged.tileHeight, result.error)) {
                    return result;
                }
                tile.collision.push_back(rect);
            }
        }

        staged.tiles.push_back(std::move(tile));
    }

    std::sort(staged.tiles.begin(), staged.tiles.end(),
              [](const TileData& a, const TileData& b) { return a.id < b.id; });
    for (std::size_t i = 1; i < staged.tiles.size(); ++i) {
        if (staged.tiles[i - 1].id == staged.tiles[i].id) {
            result.error = fail(MapErrorCode::DuplicateTileId,
                                std::to_string(staged.tiles[i].id));
            return result;
        }
    }
    result.tileset = std::move(staged);
    return result;
}

TileSetLoadResult loadTileSetFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        TileSetLoadResult result;
        result.error = fail(MapErrorCode::FileNotReadable, path);
        return result;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return loadTileSetFromString(ss.str());
}

std::string emitTileSet(const TileSet& tileset) {
    // nlohmann's `json` is a std::map, so keys come out in sorted order: the
    // canonical form is structural, not hand-ordered. Tiles keep id order
    // (the loaded order). A metadata field at its default is OMITTED, so a
    // tileset carrying no b1 metadata emits as exactly the a9 grammar.
    json doc;
    doc["name"] = tileset.name;
    doc["image"] = tileset.image;
    doc["tile_width"] = tileset.tileWidth;
    doc["tile_height"] = tileset.tileHeight;
    json tiles = json::array();
    for (const TileData& tile : tileset.tiles) {
        json entry;
        entry["id"] = tile.id;
        entry["region"] = json{{"x", tile.region.x},
                               {"y", tile.region.y},
                               {"w", tile.region.w},
                               {"h", tile.region.h}};
        if (tile.solid) {
            entry["solid"] = true;
        }
        if (tile.mask != kNoTileMask) {
            entry["mask"] = tile.mask;
        }
        if (!tile.variant.empty()) {
            entry["variant"] = tile.variant;
        }
        if (!tile.animation.empty()) {
            json frames = json::array();
            for (const TileAnimFrame& frame : tile.animation) {
                frames.push_back(json{
                    {"region", json{{"x", frame.region.x},
                                    {"y", frame.region.y},
                                    {"w", frame.region.w},
                                    {"h", frame.region.h}}},
                    {"steps", frame.steps}});
            }
            entry["animation"] = std::move(frames);
        }
        if (tile.phase != 0) {
            entry["phase"] = tile.phase;
        }
        if (!tile.collision.empty()) {
            json rects = json::array();
            for (const UIRect& rect : tile.collision) {
                rects.push_back(json{{"x", rect.x},
                                     {"y", rect.y},
                                     {"w", rect.w},
                                     {"h", rect.h}});
            }
            entry["collision"] = std::move(rects);
        }
        if (!tile.palette.empty()) {
            entry["palette"] = tile.palette;
        }
        tiles.push_back(std::move(entry));
    }
    doc["tiles"] = std::move(tiles);
    return doc.dump(2);
}

}  // namespace ply
