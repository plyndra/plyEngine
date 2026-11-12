// WorldMap (M1b b3b, ARCHITECTURE_TARGET.md §3): chunked 16×16 cell storage
// with per-chunk dirty flags and view-frustum culling at an exact count.
// The contract — coordinate model, strict positive-area intersection, opaque
// coverage, pass selection and submission order — is documented in
// WorldMap.hpp; this file is that contract, implemented.
#include "plyEngine/WorldMap.hpp"

#include "plyEngine/StructuralHash.hpp"

#include <algorithm>

namespace ply {

namespace {
void hashQuad(StructuralHasher& h, const WorldMapQuad& q) {
    h.i32(q.tileId);
    h.i32(q.rect.x);
    h.i32(q.rect.y);
    h.i32(q.rect.w);
    h.i32(q.rect.h);
}

/// Positive-area overlap: edge-touching is not intersection (a zero-area
/// sliver draws no pixels). The one overlap rule in the unit.
bool overlaps(const UIRect& a, const UIRect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

/// Draw order key: the below pass draws before the above pass; authored
/// order within a pass. A layer with no above bit draws below.
unsigned passOf(unsigned flags) {
    return (flags & kLayerAboveEntities) ? 1u : 0u;
}
}  // namespace

std::uint64_t WorldMapSubmission::hash() const {
    StructuralHasher h;
    h.byte(0);
    h.i32(static_cast<std::int32_t>(below.size()));
    for (const WorldMapQuad& q : below) hashQuad(h, q);
    h.byte(1);
    h.i32(static_cast<std::int32_t>(above.size()));
    for (const WorldMapQuad& q : above) hashQuad(h, q);
    return h.value();
}

WorldMap WorldMap::build(const MapData& map, const TilesetSlice& tileset) {
    WorldMap m;
    m.width_ = map.width > 0 ? map.width : 0;
    m.height_ = map.height > 0 ? map.height : 0;
    m.tileWidth_ = tileset.tileWidth;
    m.tileHeight_ = tileset.tileHeight;
    m.layerFlags_.clear();
    for (const MapLayer& layer : map.layers) m.layerFlags_.push_back(layer.flags);

    m.chunkColumns_ = (m.width_ + kChunkSize - 1) / kChunkSize;
    m.chunkRows_ = (m.height_ + kChunkSize - 1) / kChunkSize;
    m.chunks_.assign(static_cast<std::size_t>(m.chunkColumns_) *
                         static_cast<std::size_t>(m.chunkRows_),
                     Chunk{});
    for (Chunk& chunk : m.chunks_) {
        chunk.cells.assign(m.layerFlags_.size(),
                           std::vector<int>(static_cast<std::size_t>(kChunkSize) *
                                                static_cast<std::size_t>(kChunkSize),
                                            -1));
        chunk.quads.assign(m.layerFlags_.size(), {});
        chunk.dirty = true;  // derived lists not built until first needed
    }

    // Copy the document's cells into their chunks (the chunked storage).
    for (std::size_t l = 0; l < m.layerFlags_.size(); ++l) {
        for (int cy = 0; cy < m.height_; ++cy) {
            for (int cx = 0; cx < m.width_; ++cx) {
                const std::size_t index =
                    static_cast<std::size_t>(cy) * static_cast<std::size_t>(m.width_) +
                    static_cast<std::size_t>(cx);
                if (index >= map.layers[l].tiles.size()) continue;
                Chunk* chunk = m.chunkAt(cx / kChunkSize, cy / kChunkSize);
                if (chunk == nullptr) continue;
                const int lx = cx % kChunkSize;
                const int ly = cy % kChunkSize;
                chunk->cells[l][static_cast<std::size_t>(ly) * kChunkSize +
                                static_cast<std::size_t>(lx)] = map.layers[l].tiles[index];
            }
        }
    }
    return m;
}

int WorldMap::width() const { return width_; }
int WorldMap::height() const { return height_; }
int WorldMap::tileWidth() const { return tileWidth_; }
int WorldMap::tileHeight() const { return tileHeight_; }
std::size_t WorldMap::layerCount() const { return layerFlags_.size(); }
unsigned WorldMap::layerFlags(std::size_t layer) const {
    return layer < layerFlags_.size() ? layerFlags_[layer] : 0u;
}

int WorldMap::chunkColumns() const { return chunkColumns_; }
int WorldMap::chunkRows() const { return chunkRows_; }
std::size_t WorldMap::chunkCount() const { return chunks_.size(); }

const WorldMap::Chunk* WorldMap::chunkAt(int chunkX, int chunkY) const {
    if (chunkX < 0 || chunkY < 0 || chunkX >= chunkColumns_ || chunkY >= chunkRows_) {
        return nullptr;
    }
    return &chunks_[static_cast<std::size_t>(chunkY) * chunkColumns_ +
                    static_cast<std::size_t>(chunkX)];
}

WorldMap::Chunk* WorldMap::chunkAt(int chunkX, int chunkY) {
    return const_cast<Chunk*>(
        static_cast<const WorldMap*>(this)->chunkAt(chunkX, chunkY));
}

int WorldMap::tileAt(std::size_t layer, int cx, int cy) const {
    if (layer >= layerFlags_.size() || cx < 0 || cy < 0 || cx >= width_ || cy >= height_) {
        return -1;
    }
    const Chunk* chunk = chunkAt(cx / kChunkSize, cy / kChunkSize);
    if (chunk == nullptr) return -1;
    return chunk->cells[layer][static_cast<std::size_t>(cy % kChunkSize) * kChunkSize +
                               static_cast<std::size_t>(cx % kChunkSize)];
}

void WorldMap::setTile(std::size_t layer, int cx, int cy, int tileId) {
    if (layer >= layerFlags_.size() || cx < 0 || cy < 0 || cx >= width_ || cy >= height_) {
        return;  // out-of-range probe: ignored, nothing dirty
    }
    Chunk* chunk = chunkAt(cx / kChunkSize, cy / kChunkSize);
    if (chunk == nullptr) return;
    chunk->cells[layer][static_cast<std::size_t>(cy % kChunkSize) * kChunkSize +
                        static_cast<std::size_t>(cx % kChunkSize)] = tileId;
    chunk->dirty = true;
}

bool WorldMap::chunkDirty(int chunkX, int chunkY) const {
    const Chunk* chunk = chunkAt(chunkX, chunkY);
    return chunk != nullptr && chunk->dirty;
}

std::size_t WorldMap::chunkBuilds() const { return chunkBuilds_; }

void WorldMap::buildChunk(Chunk& chunk, int chunkX, int chunkY) const {
    for (std::size_t l = 0; l < layerFlags_.size(); ++l) chunk.quads[l].clear();

    const int cxStart = chunkX * kChunkSize;
    const int cyStart = chunkY * kChunkSize;
    const int cxEnd = std::min(width_, cxStart + kChunkSize);
    const int cyEnd = std::min(height_, cyStart + kChunkSize);

    for (int cy = cyStart; cy < cyEnd; ++cy) {
        for (int cx = cxStart; cx < cxEnd; ++cx) {
            const std::size_t local =
                static_cast<std::size_t>(cy % kChunkSize) * kChunkSize +
                static_cast<std::size_t>(cx % kChunkSize);
            for (std::size_t l = 0; l < layerFlags_.size(); ++l) {
                const int id = chunk.cells[l][local];
                if (id < 0) continue;
                // Opaque coverage: a cell is dropped only when a LATER-drawn
                // layer is opaque and has a tile at this same cell. The
                // check is chunk-local because a cell never spans chunks.
                bool covered = false;
                for (std::size_t other = 0; other < layerFlags_.size(); ++other) {
                    if (!(layerFlags_[other] & kLayerOpaque)) continue;
                    const bool later = passOf(layerFlags_[other]) > passOf(layerFlags_[l]) ||
                                       (passOf(layerFlags_[other]) == passOf(layerFlags_[l]) &&
                                        other > l);
                    if (!later) continue;
                    if (chunk.cells[other][local] >= 0) {
                        covered = true;
                        break;
                    }
                }
                if (covered) continue;
                WorldMapQuad q;
                q.tileId = id;
                q.cellX = cx;
                q.cellY = cy;
                q.rect = {cx * tileWidth_, (height_ - 1 - cy) * tileHeight_, tileWidth_,
                          tileHeight_};
                chunk.quads[l].push_back(q);
            }
        }
    }
}

WorldMapSubmission WorldMap::submit(const UIRect& camera) {
    // Visible chunks, in submission order (top-to-bottom, left-to-right):
    // the chunk rect is its IN-MAP cells' extent, so a partially filled edge
    // chunk cannot claim spurious area — and the same overlap rule the quads
    // use decides visibility.
    std::vector<std::pair<int, int>> visible;
    for (int cr = 0; cr < chunkRows_; ++cr) {
        for (int cc = 0; cc < chunkColumns_; ++cc) {
            const int cxStart = cc * kChunkSize;
            const int cyStart = cr * kChunkSize;
            const int cxEnd = std::min(width_, cxStart + kChunkSize);
            const int cyEnd = std::min(height_, cyStart + kChunkSize);
            const UIRect chunkRect{cxStart * tileWidth_, (height_ - cyEnd) * tileHeight_,
                                   (cxEnd - cxStart) * tileWidth_,
                                   (cyEnd - cyStart) * tileHeight_};
            if (!overlaps(chunkRect, camera)) continue;
            Chunk* chunk = chunkAt(cc, cr);
            if (chunk == nullptr) continue;
            if (chunk->dirty) {
                buildChunk(*chunk, cc, cr);
                chunk->dirty = false;
                ++chunkBuilds_;  // one build per dirty chunk touched
            }
            visible.emplace_back(cc, cr);
        }
    }

    // Submission: per pass, layers authored, chunks top-to-bottom then
    // left-to-right, cells row-major within each chunk (chunk.quads are built
    // in that order). Same-cell overlaps always keep layer order, because
    // both quads sit in the same chunk.
    WorldMapSubmission out;
    for (unsigned pass = 0; pass <= 1; ++pass) {
        std::vector<WorldMapQuad>& list = pass == 0 ? out.below : out.above;
        for (std::size_t l = 0; l < layerFlags_.size(); ++l) {
            if (passOf(layerFlags_[l]) != pass) continue;
            for (const auto& [cc, cr] : visible) {
                const Chunk* chunk = chunkAt(cc, cr);
                if (chunk == nullptr) continue;
                for (const WorldMapQuad& q : chunk->quads[l]) {
                    if (overlaps(q.rect, camera)) list.push_back(q);
                }
            }
        }
    }
    return out;
}

// --- cross-chunk neighbour queries (b3c, E-b3.2) -----------------------------
//
// Directions are map-grid: N is the row ABOVE the cell (cy-1, world +y —
// row 0 is the map's top row), NE is (cx+1, cy-1), and so on around the
// compass. Every read goes through tileAt, which addresses the RIGHT chunk
// for any coordinate — that is what makes the query cross seams: a cell's
// neighbour is read whether it lives in this chunk, the next one, or no
// chunk at all (out of the map).

int WorldMap::neighbourMaskAt(std::size_t layer, int cx, int cy) const {
    const int centre = tileAt(layer, cx, cy);
    if (centre < 0) return kNoTileMask;  // empty or out of range: no terrain
    static constexpr int kDX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static constexpr int kDY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    static constexpr int kBit[8] = {kNeighbourN,  kNeighbourNE, kNeighbourE, kNeighbourSE,
                                    kNeighbourS,  kNeighbourSW, kNeighbourW, kNeighbourNW};
    int mask = 0;
    for (int i = 0; i < 8; ++i) {
        // Out-of-map reads return -1 != centre, so the map edge is a terrain
        // edge. The raw diagonal is reported as truth; dropping it onto the
        // blob-47 set is canonicalizeBlobMask's job at resolve time.
        if (tileAt(layer, cx + kDX[i], cy + kDY[i]) == centre) mask |= kBit[i];
    }
    return mask;
}

TileQuery WorldMap::resolveAt(std::size_t layer, int cx, int cy,
                              const AutoTileResolver& resolver) const {
    return resolver.resolve(neighbourMaskAt(layer, cx, cy));
}

}  // namespace ply
