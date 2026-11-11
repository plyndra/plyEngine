#pragma once

#include "plyEngine/UILayout.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ply {

// Map and static-tileset loading (M1a a9, ADR 0003 + ARCHITECTURE_TARGET.md
// §4): the native JSON map format's a9 slice. Pure data — no texture, no GL,
// no window, no clock — so the whole unit is gradable in ctest. The roadmap
// calls this unit `MapLoader`; it is the load/emit/validate function trio
// below rather than a class, matching UIAtlasLoader (a1).
//
// Scope boundary (accepted 2026-11-10, before this unit's code existed): a9
// is the TILES slice — map layers/tiles/metadata and the tileset's regions +
// solid flags. Prop placements and spawns are map-format fields that arrive
// with b5.1 (PropLayer) and are unknown keys here, ignored at load, so a
// newer document still loads in an a9 build. The full tileset metadata
// schema (mask table, variants, animation) is b1.

// --- static tileset slice --------------------------------------------------

/// One tile's art region in the tileset image and its collision flag.
struct TileDef {
    int id = 0;
    UIRect region;
    bool solid = false;

    bool operator==(const TileDef&) const = default;
};

/// The a9 tileset slice (ARCHITECTURE_TARGET.md §4: "regions, mask table,
/// variants, animation, collision" — only regions and collision land here).
struct TilesetSlice {
    std::string name;
    std::string image;
    int tileWidth = 0, tileHeight = 0;
    /// Sorted by id at load.
    std::vector<TileDef> tiles;

    bool operator==(const TilesetSlice&) const = default;

    /// Tile lookup by id; nullptr when absent.
    const TileDef* findTile(int id) const;
};

// --- map ------------------------------------------------------------------

/// One map layer. `tiles` has width*height entries, row-major, row 0 being
/// the map's TOP row (maps are authored as drawn, top-down). A negative id
/// is an empty cell; loading normalises every negative id to -1.
struct MapLayer {
    std::string name;
    std::vector<int> tiles;

    bool operator==(const MapLayer&) const = default;
};

/// A map document (ARCHITECTURE_TARGET.md §4: "layers, tiles, props, spawns,
/// map metadata" — the a9 slice is layers, tiles and metadata). Layer order
/// is semantic (draw order) and preserved; everything else canonicalises at
/// load, so documents that differ only in JSON key order compare equal.
struct MapData {
    std::string name;
    /// The name of the tileset every layer's non-negative ids index into.
    std::string tileset;
    int width = 0, height = 0;
    /// Authored order preserved.
    std::vector<MapLayer> layers;

    bool operator==(const MapData&) const = default;
};

// --- named errors ----------------------------------------------------------

/// Every way a map or static tileset can be rejected. The names are part of
/// the contract: tests and the map lint match on the token, not on the
/// message prose (a1's discipline).
enum class MapErrorCode {
    None,
    FileNotReadable,
    MalformedJson,
    MissingField,
    WrongType,
    NonPositiveSize,
    EmptyTilesetName,
    DuplicateTileId,
    NegativeRegion,
    EmptyLayerName,
    DuplicateLayer,
    LayerSizeMismatch,
    MissingTileset,
    OutOfRangeTileId,
    /// b1 (TileSet.hpp): an autotile mask outside {-1} ∪ [0, 255]. Appended
    /// after the a9 tokens so existing values stay stable.
    MaskOutOfRange,
};

/// Stable token for the code, e.g. "OutOfRangeTileId". Never renamed.
const char* toString(MapErrorCode code);

struct MapError {
    MapErrorCode code = MapErrorCode::None;
    /// "<Token>: <detail>", where the detail names the offending
    /// tile/layer/field. Empty when the code is None.
    std::string message;

    bool ok() const { return code == MapErrorCode::None; }
    const char* name() const { return toString(code); }
};

struct MapLoadResult {
    MapData map;
    MapError error;

    bool ok() const { return error.ok(); }
    explicit operator bool() const { return ok(); }
};

struct TilesetLoadResult {
    TilesetSlice tileset;
    MapError error;

    bool ok() const { return error.ok(); }
    explicit operator bool() const { return ok(); }
};

// --- the function trio -----------------------------------------------------

/// Parses and validates one document. Returns a named error and an empty
/// result on failure. Unknown keys are ignored, so a newer document still
/// loads in an older build; emit writes the canonical form and drops what it
/// does not know.
TilesetLoadResult loadTilesetFromString(std::string_view document);
TilesetLoadResult loadTilesetFile(const std::string& path);
MapLoadResult loadMapFromString(std::string_view document);
MapLoadResult loadMapFile(const std::string& path);

/// Canonical JSON: keys in sorted order, two-space indent, layers in authored
/// order, every empty cell as -1. load → emit → load is byte-identical and
/// emit is a fixed point (E-a9.1's "load → save → load identical").
std::string emitMap(const MapData& map);

/// Cross-checks a map against registered tilesets (ARCHITECTURE_TARGET.md §4:
/// the validator "rejects (and names)" bad references and ids). `map.tileset`
/// must resolve to a registered tileset of that name (else MissingTileset)
/// and every non-negative tile id in every layer must exist in it (else
/// OutOfRangeTileId). The prop-bounds half of §4's validator list lands with
/// b5.1 (E-b5.x): a9's maps carry no props.
MapError validateMap(const MapData& map, const std::vector<TilesetSlice>& registered);

/// The world-px quad each non-empty cell covers, in file order (row-major,
/// top row first): cell (cx, cy) covers
/// {cx*tileWidth, (height-1-cy)*tileHeight, tileWidth, tileHeight}.
///
/// The flip is not decoration: maps are authored top-down while the world is
/// y-up (OrthoCamera.hpp), so the top row lands at the highest world y —
/// which is what "static tiles render behind the HUD" means geometrically
/// before a7's GroundTiles layer draws them under the UI (which is above the
/// upscale and outside this scene list). Empty cells emit nothing.
std::vector<UIRect> mapTileQuads(const MapData& map, const TilesetSlice& tileset);

}  // namespace ply
