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
        case MapErrorCode::UnknownMask: return "UnknownMask";
        case MapErrorCode::UnknownLayerFlag: return "UnknownLayerFlag";
        case MapErrorCode::ConflictingLayerFlags: return "ConflictingLayerFlags";
        case MapErrorCode::EmptyPropName: return "EmptyPropName";
        case MapErrorCode::DuplicateProp: return "DuplicateProp";
        case MapErrorCode::PropOutOfBounds: return "PropOutOfBounds";
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

// --- layer flag tokens (b3a) ----------------------------------------------

const char* toString(unsigned layerFlag) {
    switch (layerFlag) {
        case kLayerBelowEntities: return "below-entities";
        case kLayerAboveEntities: return "above-entities";
        case kLayerAnimated: return "animated";
        case kLayerOpaque: return "opaque";
        default: return nullptr;  // 0, a combination, or a future flag
    }
}

unsigned parseLayerFlag(std::string_view token) {
    // Exact match only: a prefix or a different case is not a token, so a
    // typo surfaces as UnknownLayerFlag instead of a silently different
    // layer.
    for (const unsigned flag : {kLayerBelowEntities, kLayerAboveEntities,
                                kLayerAnimated, kLayerOpaque}) {
        const char* name = toString(flag);
        if (name != nullptr && token == name) return flag;
    }
    return 0;
}

const TileDef* TilesetSlice::findTile(int id) const {
    for (const TileDef& tile : tiles) {
        if (tile.id == id) return &tile;
    }
    return nullptr;
}

// --- layer-flag grammar, shared by layers (b3a) and props (b5.1) ------------
// One parse and one emit so the two spellings cannot drift: the same tokens,
// the same default (an absent key or empty array is below-entities), the same
// loud failures (UnknownLayerFlag / ConflictingLayerFlags).

bool parseFlagsArray(const json& entry, const char* key, unsigned& out, MapError& error,
                     const std::string& who) {
    const auto flags = entry.find(key);
    if (flags == entry.end()) return true;  // absent = the ground default
    if (!flags->is_array()) {
        error = fail(MapErrorCode::WrongType, key);
        return false;
    }
    unsigned parsed = 0;
    for (std::size_t f = 0; f < flags->size(); ++f) {
        if (!(*flags)[f].is_string()) {
            error = fail(MapErrorCode::WrongType,
                         std::string(key) + "[" + std::to_string(f) + "]");
            return false;
        }
        const std::string token = (*flags)[f].get<std::string>();
        const unsigned flag = parseLayerFlag(token);
        if (flag == 0) {
            error = fail(MapErrorCode::UnknownLayerFlag, who + " flag '" + token + "'");
            return false;
        }
        parsed |= flag;
    }
    out = parsed == 0 ? kLayerBelowEntities : parsed;
    if ((out & kLayerBelowEntities) && (out & kLayerAboveEntities)) {
        error = fail(MapErrorCode::ConflictingLayerFlags,
                     who + " claims below-entities and above-entities");
        return false;
    }
    return true;
}

// Flags are written only when they differ from the ground default, so a
// flags-less a9 document emits back as exactly the a9 grammar.
//
// Normalisation on emit (b3a follow-up, O2): unknown bits are DROPPED, the
// same way a9 normalises a negative tile id to -1 at load, so a hand-built
// flags=0 or a future flag bit cannot make emit produce a document this
// build would reload differently. A value claiming BOTH below and above is
// still emitted as written and will be rejected loudly on reload: that is an
// impossible draw order, not something to silently repair.
void emitFlagsEntry(json& entry, const char* key, unsigned flags) {
    const unsigned emitted = flags & kLayerAllFlags;
    if (emitted == 0 || emitted == kLayerBelowEntities) return;
    json tokens = json::array();
    for (const unsigned flag : {kLayerBelowEntities, kLayerAboveEntities, kLayerAnimated,
                                kLayerOpaque}) {
        if ((emitted & flag) == 0) continue;
        const char* token = toString(flag);
        if (token != nullptr) tokens.push_back(token);
    }
    entry[key] = std::move(tokens);
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

        // b3a: layer flags. Absent means the default (ground), which is the
        // struct initialiser; an empty array is the same statement.
        if (!parseFlagsArray(entry, "flags", layer.flags, result.error, layer.name)) {
            result.map = MapData{};  // O4: every rejection clears the map
            return result;
        }

        const auto tiles = entry.find("tiles");
        if (tiles == entry.end()) {
            result.error = fail(MapErrorCode::MissingField, "tiles");
            result.map = MapData{};  // O4: every rejection clears the map
            return result;
        }
        if (!tiles->is_array()) {
            result.error = fail(MapErrorCode::WrongType, "tiles");
            result.map = MapData{};
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
                result.map = MapData{};
                return result;
            }
            const int id = (*tiles)[c].get<int>();
            layer.tiles.push_back(id < 0 ? -1 : id);  // every empty cell normalises to -1
        }
        result.map.layers.push_back(std::move(layer));
    }

    // b5.1 props (optional — a9 documents carry none; unknown keys were
    // ignored at load before this existed, so old documents still load).
    const auto props = doc.find("props");
    if (props != doc.end()) {
        if (!props->is_array()) {
            result.error = fail(MapErrorCode::WrongType, "props");
            result.map = MapData{};
            return result;
        }
        for (std::size_t i = 0; i < props->size(); ++i) {
            const json& entry = (*props)[i];
            const std::string where = "props[" + std::to_string(i) + "]";
            if (!entry.is_object()) {
                result.error = fail(MapErrorCode::WrongType, where);
                result.map = MapData{};
                return result;
            }
            Prop prop;
            if (!parseStringField(entry, "name", prop.name, result.error)) {
                result.map = MapData{};
                return result;
            }
            if (prop.name.empty()) {
                result.error = fail(MapErrorCode::EmptyPropName, where);
                result.map = MapData{};
                return result;
            }
            for (const Prop& existing : result.map.props) {
                if (existing.name == prop.name) {
                    result.error = fail(MapErrorCode::DuplicateProp, prop.name);
                    result.map = MapData{};
                    return result;
                }
            }
            if (!parseIntField(entry, "tile", prop.tile, result.error)) {
                result.map = MapData{};
                return result;
            }
            if (prop.tile < 0) {
                result.error = fail(MapErrorCode::OutOfRangeTileId,
                                    prop.name + " tile " + std::to_string(prop.tile));
                result.map = MapData{};
                return result;
            }
            if (!parseIntField(entry, "x", prop.x, result.error) ||
                !parseIntField(entry, "y", prop.y, result.error)) {
                result.map = MapData{};
                return result;
            }
            // width/height are the art footprint in TILES, optional (1x1).
            for (const char* key : {"width", "height"}) {
                const auto size = entry.find(key);
                if (size == entry.end()) continue;
                int value = 1;
                if (!parseIntField(entry, key, value, result.error)) {
                    result.map = MapData{};
                    return result;
                }
                if (value <= 0) {
                    result.error = fail(MapErrorCode::NonPositiveSize,
                                        prop.name + " " + key);
                    result.map = MapData{};
                    return result;
                }
                if (std::string(key) == "width")
                    prop.widthTiles = value;
                else
                    prop.heightTiles = value;
            }
            // The b3a grammar under the prop spelling ("layer").
            if (!parseFlagsArray(entry, "layer", prop.flags, result.error, prop.name)) {
                result.map = MapData{};
                return result;
            }
            const auto occlusion = entry.find("occlusion");
            if (occlusion != entry.end()) {
                if (!occlusion->is_boolean()) {
                    result.error = fail(MapErrorCode::WrongType, prop.name + " occlusion");
                    result.map = MapData{};
                    return result;
                }
                prop.occlusion = occlusion->get<bool>();
            }
            const auto collision = entry.find("collision");
            if (collision != entry.end()) {
                if (!collision->is_array()) {
                    result.error = fail(MapErrorCode::WrongType, prop.name + " collision");
                    result.map = MapData{};
                    return result;
                }
                for (std::size_t c = 0; c < collision->size(); ++c) {
                    const json& rect = (*collision)[c];
                    if (!rect.is_object()) {
                        result.error = fail(MapErrorCode::WrongType,
                                            prop.name + " collision[" + std::to_string(c) + "]");
                        result.map = MapData{};
                        return result;
                    }
                    UIRect r;
                    if (!parseIntField(rect, "x", r.x, result.error) ||
                        !parseIntField(rect, "y", r.y, result.error) ||
                        !parseIntField(rect, "w", r.w, result.error) ||
                        !parseIntField(rect, "h", r.h, result.error)) {
                        result.map = MapData{};
                        return result;
                    }
                    // Intrinsic geometry is checked here (the b1 tile-
                    // collision precedent); the ART-FOOTPRINT half needs the
                    // tile size and lands in validateMap.
                    if (r.x < 0 || r.y < 0 || r.w <= 0 || r.h <= 0) {
                        result.error = fail(MapErrorCode::NegativeRegion,
                                            prop.name + " collision " + std::to_string(c));
                        result.map = MapData{};
                        return result;
                    }
                    prop.collision.push_back(r);
                }
            }
            const auto interaction = entry.find("interaction");
            if (interaction != entry.end()) {
                if (!interaction->is_string()) {
                    result.error = fail(MapErrorCode::WrongType, prop.name + " interaction");
                    result.map = MapData{};
                    return result;
                }
                prop.interaction = interaction->get<std::string>();
            }
            result.map.props.push_back(std::move(prop));
        }
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
        emitFlagsEntry(entry, "flags", layer.flags);
        layers.push_back(std::move(entry));
    }
    doc["layers"] = std::move(layers);
    // b5.1: the props key exists only when there are props (the a9 byte
    // shape); authored order preserved; every default omitted.
    if (!map.props.empty()) {
        json props = json::array();
        for (const Prop& prop : map.props) {
            json entry;
            entry["name"] = prop.name;
            entry["tile"] = prop.tile;
            entry["x"] = prop.x;
            entry["y"] = prop.y;
            if (prop.widthTiles != 1) entry["width"] = prop.widthTiles;
            if (prop.heightTiles != 1) entry["height"] = prop.heightTiles;
            emitFlagsEntry(entry, "layer", prop.flags);
            if (prop.occlusion) entry["occlusion"] = prop.occlusion;
            if (!prop.collision.empty()) {
                json collision = json::array();
                for (const UIRect& r : prop.collision) {
                    json rect;
                    rect["x"] = r.x;
                    rect["y"] = r.y;
                    rect["w"] = r.w;
                    rect["h"] = r.h;
                    collision.push_back(std::move(rect));
                }
                entry["collision"] = std::move(collision);
            }
            if (!prop.interaction.empty()) entry["interaction"] = prop.interaction;
            props.push_back(std::move(entry));
        }
        doc["props"] = std::move(props);
    }
    return doc.dump(2);
}

// --- PropLayer (b5) --------------------------------------------------------

PropLayer PropLayer::build(const MapData& map, const TilesetSlice& tileset) {
    PropLayer layer;
    const int mapHeightPx = map.height * tileset.tileHeight;
    for (const Prop& prop : map.props) {
        Entry entry;
        entry.prop = prop;
        // Defensive clamp (hand-built props bypass the loader's NonPositiveSize).
        const int tilesW = prop.widthTiles > 0 ? prop.widthTiles : 1;
        const int tilesH = prop.heightTiles > 0 ? prop.heightTiles : 1;
        const int artW = tilesW * tileset.tileWidth;
        const int artH = tilesH * tileset.tileHeight;
        const int feetX = prop.x;
        const int feetY = mapHeightPx - prop.y;  // the one documented flip
        entry.pivotY = feetY;
        // The art sits on the feet point at bottom-centre (y-up: the rect's y
        // IS its bottom edge).
        entry.art = {feetX - artW / 2, feetY, artW, artH};
        // Generated shadow (never authored): an ellipse's bounding rect
        // centred on the FEET, width = art width, height = tileHeight/2.
        const int shadowH = tileset.tileHeight > 1 ? tileset.tileHeight / 2 : 1;
        entry.shadow = {feetX - artW / 2, feetY - shadowH / 2, artW, shadowH};
        // Collision onto the art: prop-local art px, y-down mirrored into
        // y-up within the art rect. Absent = the whole footprint.
        if (prop.collision.empty()) {
            entry.collision.push_back(entry.art);
        } else {
            for (const UIRect& rect : prop.collision) {
                entry.collision.push_back(
                    {entry.art.x + rect.x, entry.art.y + (artH - rect.y - rect.h), rect.w,
                     rect.h});
            }
        }
        layer.entries_.push_back(std::move(entry));
    }
    return layer;
}

std::size_t PropLayer::size() const { return entries_.size(); }

const Prop& PropLayer::prop(std::size_t index) const {
    static const Prop kEmpty;
    return index < entries_.size() ? entries_[index].prop : kEmpty;
}

UIRect PropLayer::artRect(std::size_t index) const {
    return index < entries_.size() ? entries_[index].art : UIRect{};
}

UIRect PropLayer::shadowRect(std::size_t index) const {
    return index < entries_.size() ? entries_[index].shadow : UIRect{};
}

const std::vector<UIRect>& PropLayer::collisionRects(std::size_t index) const {
    static const std::vector<UIRect> kEmpty;
    return index < entries_.size() ? entries_[index].collision : kEmpty;
}

int PropLayer::pivotY(std::size_t index) const {
    return index < entries_.size() ? entries_[index].pivotY : 0;
}

bool PropLayer::occludes(std::size_t index, int playerFeetY) const {
    if (index >= entries_.size()) return false;
    // The flip lands EXACTLY on the player's y: at or behind the feet line.
    return entries_[index].prop.occlusion && playerFeetY >= entries_[index].pivotY;
}

bool PropLayer::drawsAboveEntities(std::size_t index) const {
    return index < entries_.size() ? entries_[index].prop.aboveEntities() : false;
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
    // b5.1 (E-b5.x, ARCHITECTURE_TARGET.md §4): prop art must exist, and
    // prop collisions must fit their art footprint AND the map's bounds.
    const int mapWidthPx = map.width * tileset->tileWidth;
    const int mapHeightPx = map.height * tileset->tileHeight;
    for (const Prop& prop : map.props) {
        if (prop.tile < 0 || tileset->findTile(prop.tile) == nullptr) {
            return fail(MapErrorCode::OutOfRangeTileId,
                        "prop " + prop.name + " tile " + std::to_string(prop.tile));
        }
        const int artWidth = prop.widthTiles * tileset->tileWidth;
        const int artHeight = prop.heightTiles * tileset->tileHeight;
        // The EFFECTIVE collision: authored rects, or the whole art
        // footprint when none are authored (Prop::collision's contract), so
        // an out-of-bounds prop is caught even without authored rects.
        std::vector<UIRect> effective = prop.collision;
        if (effective.empty()) effective.push_back(UIRect{0, 0, artWidth, artHeight});
        for (std::size_t c = 0; c < effective.size(); ++c) {
            const UIRect& rect = effective[c];
            if (rect.x < 0 || rect.y < 0 || rect.w <= 0 || rect.h <= 0 ||
                rect.x + rect.w > artWidth || rect.y + rect.h > artHeight) {
                return fail(MapErrorCode::NegativeRegion,
                            "prop " + prop.name + " collision " + std::to_string(c) +
                                " outside its art");
            }
            // World px (the one documented flip): the art's origin is
            // (x - artW/2, mapHeightPx - y); the rect mirrors y within it.
            const int worldX = prop.x - artWidth / 2 + rect.x;
            const int worldY = (mapHeightPx - prop.y) + (artHeight - rect.y - rect.h);
            if (worldX < 0 || worldY < 0 || worldX + rect.w > mapWidthPx ||
                worldY + rect.h > mapHeightPx) {
                return fail(MapErrorCode::PropOutOfBounds,
                            "prop " + prop.name + " collision " + std::to_string(c) +
                                " outside the map");
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
