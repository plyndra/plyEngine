#include "plyEngine/MapLoader.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace ply {

using json = nlohmann::json;

const char* toString(MapErrorCode code) {
    switch (code) {
        case MapErrorCode::None: return "None";
        case MapErrorCode::FileNotReadable: return "FileNotReadable";
        case MapErrorCode::MalformedJson: return "MalformedJson";
        case MapErrorCode::MissingField: return "MissingField";
        case MapErrorCode::WrongType: return "WrongType";
        case MapErrorCode::NonPositiveSize: return "NonPositiveSize";
        case MapErrorCode::EmptyTilesetName: return "EmptyTilesetName";
        case MapErrorCode::DuplicateTileId: return "DuplicateTileId";
        case MapErrorCode::NegativeRegion: return "NegativeRegion";
        case MapErrorCode::EmptyLayerName: return "EmptyLayerName";
        case MapErrorCode::DuplicateLayer: return "DuplicateLayer";
        case MapErrorCode::LayerSizeMismatch: return "LayerSizeMismatch";
        case MapErrorCode::MissingTileset: return "MissingTileset";
        case MapErrorCode::OutOfRangeTileId: return "OutOfRangeTileId";
        case MapErrorCode::MaskOutOfRange: return "MaskOutOfRange";
    }
    return "None";  // unreachable; every enumerator is handled
}

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

/// Parses one document, mapping syntax errors to MalformedJson and a
/// non-object root to WrongType.
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

}  // namespace

const TileDef* TilesetSlice::findTile(int id) const {
    for (const TileDef& tile : tiles) {
        if (tile.id == id) return &tile;
    }
    return nullptr;
}

// --- tileset slice ---------------------------------------------------------

TilesetLoadResult loadTilesetFromString(std::string_view document) {
    TilesetLoadResult result;
    json doc;
    if (!parseDocument(document, doc, result.error)) return result;

    if (!parseStringField(doc, "name", result.tileset.name, result.error)) return result;
    if (result.tileset.name.empty()) {
        result.error = fail(MapErrorCode::EmptyTilesetName, "name");
        return result;
    }
    if (!parseStringField(doc, "image", result.tileset.image, result.error)) return result;
    if (!parseIntField(doc, "tile_width", result.tileset.tileWidth, result.error)) return result;
    if (!parseIntField(doc, "tile_height", result.tileset.tileHeight, result.error)) return result;
    if (result.tileset.tileWidth <= 0 || result.tileset.tileHeight <= 0) {
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
        TileDef tile;
        if (!parseIntField(entry, "id", tile.id, result.error)) return result;

        const auto region = entry.find("region");
        if (region == entry.end()) {
            result.error = fail(MapErrorCode::MissingField, "region");
            return result;
        }
        if (!region->is_object()) {
            result.error = fail(MapErrorCode::WrongType, "region");
            return result;
        }
        if (!parseIntField(*region, "x", tile.region.x, result.error)) return result;
        if (!parseIntField(*region, "y", tile.region.y, result.error)) return result;
        if (!parseIntField(*region, "w", tile.region.w, result.error)) return result;
        if (!parseIntField(*region, "h", tile.region.h, result.error)) return result;
        if (tile.region.x < 0 || tile.region.y < 0 || tile.region.w <= 0 || tile.region.h <= 0) {
            result.error = fail(MapErrorCode::NegativeRegion,
                                "tile " + std::to_string(tile.id));
            return result;
        }

        // `solid` is optional and defaults to false; a newer document may
        // carry more keys, which are ignored here.
        const auto solid = entry.find("solid");
        if (solid != entry.end()) {
            if (!solid->is_boolean()) {
                result.error = fail(MapErrorCode::WrongType, "solid");
                return result;
            }
            tile.solid = solid->get<bool>();
        }
        result.tileset.tiles.push_back(tile);
    }

    std::sort(result.tileset.tiles.begin(), result.tileset.tiles.end(),
              [](const TileDef& a, const TileDef& b) { return a.id < b.id; });
    for (std::size_t i = 1; i < result.tileset.tiles.size(); ++i) {
        if (result.tileset.tiles[i - 1].id == result.tileset.tiles[i].id) {
            result.error = fail(MapErrorCode::DuplicateTileId,
                                std::to_string(result.tileset.tiles[i].id));
            result.tileset = TilesetSlice{};
            return result;
        }
    }
    return result;
}

TilesetLoadResult loadTilesetFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        TilesetLoadResult result;
        result.error = fail(MapErrorCode::FileNotReadable, path);
        return result;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return loadTilesetFromString(ss.str());
}

// --- map ------------------------------------------------------------------

MapLoadResult loadMapFromString(std::string_view document) {
    MapLoadResult result;
    json doc;
    if (!parseDocument(document, doc, result.error)) return result;

    if (!parseStringField(doc, "name", result.map.name, result.error)) return result;
    if (!parseStringField(doc, "tileset", result.map.tileset, result.error)) return result;
    if (result.map.tileset.empty()) {
        result.error = fail(MapErrorCode::EmptyTilesetName, "tileset");
        return result;
    }
    if (!parseIntField(doc, "width", result.map.width, result.error)) return result;
    if (!parseIntField(doc, "height", result.map.height, result.error)) return result;
    if (result.map.width <= 0 || result.map.height <= 0) {
        result.error = fail(MapErrorCode::NonPositiveSize, "width/height");
        return result;
    }

    const auto layers = doc.find("layers");
    if (layers == doc.end()) {
        result.error = fail(MapErrorCode::MissingField, "layers");
        return result;
    }
    if (!layers->is_array()) {
        result.error = fail(MapErrorCode::WrongType, "layers");
        return result;
    }
    for (std::size_t i = 0; i < layers->size(); ++i) {
        const json& entry = (*layers)[i];
        if (!entry.is_object()) {
            result.error = fail(MapErrorCode::WrongType, "layers[" + std::to_string(i) + "]");
            return result;
        }
        MapLayer layer;
        if (!parseStringField(entry, "name", layer.name, result.error)) return result;
        if (layer.name.empty()) {
            result.error = fail(MapErrorCode::EmptyLayerName,
                                "layers[" + std::to_string(i) + "]");
            return result;
        }
        for (const MapLayer& existing : result.map.layers) {
            if (existing.name == layer.name) {
                result.error = fail(MapErrorCode::DuplicateLayer, layer.name);
                result.map = MapData{};
                return result;
            }
        }

        const auto tiles = entry.find("tiles");
        if (tiles == entry.end()) {
            result.error = fail(MapErrorCode::MissingField, "tiles");
            return result;
        }
        if (!tiles->is_array()) {
            result.error = fail(MapErrorCode::WrongType, "tiles");
            return result;
        }
        const std::size_t cells = static_cast<std::size_t>(result.map.width) *
                                  static_cast<std::size_t>(result.map.height);
        if (tiles->size() != cells) {
            result.error = fail(MapErrorCode::LayerSizeMismatch, layer.name + " holds " +
                                                                     std::to_string(tiles->size()) +
                                                                     " of " +
                                                                     std::to_string(cells));
            result.map = MapData{};
            return result;
        }
        layer.tiles.reserve(cells);
        for (std::size_t c = 0; c < tiles->size(); ++c) {
            if (!(*tiles)[c].is_number_integer()) {
                result.error = fail(MapErrorCode::WrongType, "tiles[" + std::to_string(c) + "]");
                return result;
            }
            const int id = (*tiles)[c].get<int>();
            layer.tiles.push_back(id < 0 ? -1 : id);  // every empty cell normalises to -1
        }
        result.map.layers.push_back(std::move(layer));
    }
    return result;
}

MapLoadResult loadMapFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        MapLoadResult result;
        result.error = fail(MapErrorCode::FileNotReadable, path);
        return result;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return loadMapFromString(ss.str());
}

std::string emitMap(const MapData& map) {
    // nlohmann's `json` is a std::map, so keys come out in sorted order: the
    // canonical form is structural, not hand-ordered. Layers keep authored
    // order (draw order is semantic); every empty cell is already -1.
    json doc;
    doc["name"] = map.name;
    doc["tileset"] = map.tileset;
    doc["width"] = map.width;
    doc["height"] = map.height;
    json layers = json::array();
    for (const MapLayer& layer : map.layers) {
        json entry;
        entry["name"] = layer.name;
        entry["tiles"] = layer.tiles;
        layers.push_back(std::move(entry));
    }
    doc["layers"] = std::move(layers);
    return doc.dump(2);
}

MapError validateMap(const MapData& map, const std::vector<TilesetSlice>& registered) {
    const TilesetSlice* tileset = nullptr;
    for (const TilesetSlice& candidate : registered) {
        if (candidate.name == map.tileset) {
            tileset = &candidate;
            break;
        }
    }
    if (tileset == nullptr) {
        return fail(MapErrorCode::MissingTileset, map.tileset.empty() ? "(empty)" : map.tileset);
    }
    for (const MapLayer& layer : map.layers) {
        for (std::size_t c = 0; c < layer.tiles.size(); ++c) {
            const int id = layer.tiles[c];
            if (id < 0) continue;  // empty cells index nothing
            if (tileset->findTile(id) == nullptr) {
                return fail(MapErrorCode::OutOfRangeTileId,
                            layer.name + " cell " + std::to_string(c) + " id " +
                                std::to_string(id));
            }
        }
    }
    return MapError{};
}

std::vector<UIRect> mapTileQuads(const MapData& map, const TilesetSlice& tileset) {
    std::vector<UIRect> out;
    for (const MapLayer& layer : map.layers) {
        for (int cy = 0; cy < map.height; ++cy) {
            for (int cx = 0; cx < map.width; ++cx) {
                const std::size_t index = static_cast<std::size_t>(cy) *
                                          static_cast<std::size_t>(map.width) +
                                          static_cast<std::size_t>(cx);
                if (index >= layer.tiles.size() || layer.tiles[index] < 0) continue;
                // Row 0 is the map's TOP row (authored top-down); the world is
                // y-up (OrthoCamera.hpp), so the top row lands at the highest
                // world y. This flip is pinned at the corners by E-a9.1.
                out.push_back(UIRect{cx * tileset.tileWidth,
                                     (map.height - 1 - cy) * tileset.tileHeight,
                                     tileset.tileWidth, tileset.tileHeight});
            }
        }
    }
    return out;
}

}  // namespace ply
