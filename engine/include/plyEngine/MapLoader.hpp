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

// --- layer flags (b3a, ARCHITECTURE_TARGET.md §3) --------------------------
//
// "TileLayer: Int32 tile ids per cell; per-layer flags (below/above entities,
// animated, opaque)". The roadmap calls this module `TileLayer`; it is the
// struct below (aliased at the end of this block), because a9 already shipped
// the ids half of it as MapLayer and a second type would be the same data
// spelled twice.

/// Draws under the entity layer — the ground. The default: a layer that says
/// nothing about itself is ground.
inline constexpr unsigned kLayerBelowEntities = 1u << 0;
/// Draws over the entity layer (tree canopies, roofs, awnings).
inline constexpr unsigned kLayerAboveEntities = 1u << 1;
/// Contains animated tiles; TileAnimator (b4) advances it from the sim clock.
inline constexpr unsigned kLayerAnimated = 1u << 2;
/// Fully opaque: nothing below it needs drawing where it covers (a culling
/// hint for b3b, not a blend mode).
inline constexpr unsigned kLayerOpaque = 1u << 3;

/// Every flag this build knows. An unknown token in a document is a named
/// error (UnknownLayerFlag), not a silent drop: a typo in an authoring tool
/// must not quietly turn a canopy into ground.
inline constexpr unsigned kLayerAllFlags = kLayerBelowEntities | kLayerAboveEntities |
                                           kLayerAnimated | kLayerOpaque;

/// One map layer. `tiles` has width*height entries, row-major, row 0 being
/// the map's TOP row (maps are authored as drawn, top-down). A negative id
/// is an empty cell; loading normalises every negative id to -1.
///
/// `flags` is a bitmask over the kLayer* constants. It defaults to
/// kLayerBelowEntities, and emit writes the "flags" key only when the value
/// differs from that default, so a document carrying no flags emits back as
/// exactly the a9 grammar (the a9 compatibility pin).
///
/// Precondition for a hand-built layer (never reachable from a document): set
/// only bits in kLayerAllFlags, and never both below- and above-entities.
/// emit DROPS unknown bits (the a9 negative-id normalisation precedent) and
/// treats 0 as the default; a layer claiming both draw orders is emitted as
/// written and rejected loudly on reload, because that is an impossible draw
/// order rather than something to repair silently.
struct MapLayer {
    std::string name;
    std::vector<int> tiles;
    unsigned flags = kLayerBelowEntities;

    bool operator==(const MapLayer&) const = default;

    bool hasFlag(unsigned flag) const { return (flags & flag) != 0; }
    /// True when the layer draws above entities rather than below them.
    bool aboveEntities() const { return hasFlag(kLayerAboveEntities); }
};

/// The roadmap's name for the module above (ARCHITECTURE_TARGET.md §3).
using TileLayer = MapLayer;

/// Stable token for a layer flag, e.g. "above-entities"; nullptr when the
/// value does not name exactly one recognised flag. The tokens are the file
/// format's contract.
const char* toString(unsigned layerFlag);
/// Parses a flag token; 0 when unknown. Exact match only.
unsigned parseLayerFlag(std::string_view token);

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
    /// b1.2 (TileSet.hpp): an in-range autotile mask that is not one of the
    /// 47 canonical blob masks (kBlobMasks) - no blob tile can represent it.
    UnknownMask,
    /// b3a: a layer flag token this build does not know.
    UnknownLayerFlag,
    /// b3a: a layer claiming both below-entities and above-entities, which
    /// cannot both be true of one draw order.
    ConflictingLayerFlags,
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
