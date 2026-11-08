#pragma once

#include "plyEngine/Component.hpp"
#include "plyEngine/Entity.hpp"

#include <algorithm>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

namespace ply {

class World;

namespace detail {

/// One component array for an archetype: aligned, capacity-managed storage.
/// Objects are placed/destroyed/moved through ComponentMeta, so non-trivial
/// components are supported.
struct Column {
    ComponentMeta meta;
    unsigned char* data = nullptr;
    size_t capacity = 0;

    Column() = default;
    explicit Column(const ComponentMeta& m) : meta(m) {}
    ~Column() { release(); }

    Column(const Column&) = delete;
    Column& operator=(const Column&) = delete;
    Column(Column&& other) noexcept
        : meta(other.meta), data(other.data), capacity(other.capacity) {
        other.data = nullptr;
        other.capacity = 0;
    }
    Column& operator=(Column&& other) noexcept {
        if (this != &other) {
            release();
            meta = other.meta;
            data = other.data;
            capacity = other.capacity;
            other.data = nullptr;
            other.capacity = 0;
        }
        return *this;
    }

    size_t size() const { return meta.size; }

    void release() {
        if (data) {
            ::operator delete(data, std::align_val_t{meta.align});
            data = nullptr;
        }
        capacity = 0;
    }

    /// Ensures room for `rowsNeeded` rows, relocating the `liveRows` rows that
    /// already hold constructed objects.
    void grow(size_t rowsNeeded, size_t liveRows) {
        if (rowsNeeded <= capacity) {
            return;
        }
        size_t newCapacity = capacity ? capacity * 2 : 8;
        while (newCapacity < rowsNeeded) {
            newCapacity *= 2;
        }
        auto* fresh = static_cast<unsigned char*>(
            ::operator new(newCapacity * meta.size, std::align_val_t{meta.align}));
        for (size_t r = 0; r < liveRows; ++r) {
            meta.relocate(fresh + r * meta.size, data + r * meta.size);
        }
        if (data) {
            ::operator delete(data, std::align_val_t{meta.align});
        }
        data = fresh;
        capacity = newCapacity;
    }

    void* at(size_t row) { return data + row * meta.size; }
    const void* at(size_t row) const { return data + row * meta.size; }
};

/// A set of component types plus columnar storage for the entities that have
/// exactly that set.
struct Archetype {
    static constexpr size_t kNoColumn = static_cast<size_t>(-1);

    std::vector<ComponentTypeId> types;  // sorted ascending
    std::vector<Column> columns;         // parallel to types
    std::vector<Entity> entities;

    size_t rowCount() const { return entities.size(); }

    size_t column(ComponentTypeId id) const {
        for (size_t i = 0; i < types.size(); ++i) {
            if (types[i] == id) {
                return i;
            }
        }
        return kNoColumn;
    }

    void* columnData(size_t col, size_t row) { return columns[col].at(row); }
};

}  // namespace detail

/// An archetype-based entity-component world (Bevy-style SoA storage).
///
/// Entities with the same set of components share an archetype; each component
/// type is a contiguous column, so each<Ts...>() walks memory linearly. Adding
/// or removing a component moves the entity between archetypes.
///
/// Components may be any movable type. Destroying or relocating a component
/// runs its destructor exactly once.
class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // --- Entities -----------------------------------------------------------
    Entity spawn();
    void destroy(Entity entity);
    bool alive(Entity entity) const;
    size_t entityCount() const { return aliveCount_; }
    size_t archetypeCount() const { return archetypes_.size(); }

    // --- Components ---------------------------------------------------------
    template <typename T>
    bool has(Entity entity) const {
        if (!alive(entity)) {
            return false;
        }
        const auto& record = records_[entity.index];
        const detail::Archetype& arch = archetypes_[record.archetype];
        return arch.column(componentTypeId<T>()) != detail::Archetype::kNoColumn;
    }

    template <typename T>
    T* get(Entity entity) {
        return const_cast<T*>(std::as_const(*this).get<T>(entity));
    }

    template <typename T>
    const T* get(Entity entity) const {
        if (!alive(entity)) {
            return nullptr;
        }
        const auto& record = records_[entity.index];
        const detail::Archetype& arch = archetypes_[record.archetype];
        const size_t col = arch.column(componentTypeId<T>());
        if (col == detail::Archetype::kNoColumn) {
            return nullptr;
        }
        return static_cast<const T*>(arch.columns[col].at(record.row));
    }

    /// Adds (or replaces) component T on entity. Returns nullptr if the entity
    /// is not alive.
    template <typename T, typename... Args>
    T* add(Entity entity, Args&&... args) {
        if (!alive(entity)) {
            return nullptr;
        }
        auto& record = records_[entity.index];

        // Already present: assign in place.
        if (T* existing = get<T>(entity)) {
            *existing = T(std::forward<Args>(args)...);
            return existing;
        }

        std::vector<ComponentTypeId> target = archetypes_[record.archetype].types;
        target.push_back(componentTypeId<T>());
        std::sort(target.begin(), target.end());

        const size_t oldArchetype = record.archetype;
        const size_t oldRow = record.row;
        const size_t newArchetype = ensureArchetype(target);
        const size_t newRow = appendRow(newArchetype, entity);

        // Move the components the entity already had into the new row, then
        // construct the new one. The old row is now vacated (objects moved
        // from), so it is dropped without running destructors again.
        moveSharedColumns(archetypes_[oldArchetype], oldRow,
                          archetypes_[newArchetype], newRow);

        detail::Archetype& dst = archetypes_[newArchetype];
        const size_t col = dst.column(componentTypeId<T>());
        T* object = new (dst.columns[col].at(newRow)) T(std::forward<Args>(args)...);

        dropRow(oldArchetype, oldRow);
        record.archetype = newArchetype;
        record.row = newRow;
        return object;
    }

    template <typename T>
    void remove(Entity entity) {
        if (!alive(entity)) {
            return;
        }
        auto& record = records_[entity.index];
        const ComponentTypeId id = componentTypeId<T>();
        const size_t removedColumn =
            archetypes_[record.archetype].column(id);
        if (removedColumn == detail::Archetype::kNoColumn) {
            return;
        }

        std::vector<ComponentTypeId> target = archetypes_[record.archetype].types;
        target.erase(std::find(target.begin(), target.end(), id));

        const size_t oldArchetype = record.archetype;
        const size_t oldRow = record.row;

        // NOTE: ensureArchetype() and appendRow() may push_back into
        // archetypes_, reallocating it. Never hold a reference across them;
        // always re-index by archetype id.
        const size_t newArchetype = ensureArchetype(target);
        const size_t newRow = appendRow(newArchetype, entity);

        moveSharedColumns(archetypes_[oldArchetype], oldRow,
                          archetypes_[newArchetype], newRow);

        // Destroy the component being removed (re-fetched: see NOTE above).
        detail::Column& removed = archetypes_[oldArchetype].columns[removedColumn];
        removed.meta.destroy(removed.at(oldRow));

        dropRow(oldArchetype, oldRow);
        record.archetype = newArchetype;
        record.row = newRow;
    }

    // --- Iteration ----------------------------------------------------------
    /// Calls fn(Entity, Ts&...) for every entity that has all of Ts.
    /// Do not spawn/destroy/add/remove during iteration: it may reallocate.
    template <typename... Ts, typename F>
    void each(F&& fn) {
        static_assert(sizeof...(Ts) > 0, "each<Ts...>() needs at least one type");

        for (detail::Archetype& arch : archetypes_) {
            size_t cols[sizeof...(Ts)];
            if (!matchColumns<Ts...>(arch, cols)) {
                continue;
            }
            invokeRows<F, Ts...>(fn, arch, cols,
                                 std::index_sequence_for<Ts...>{});
        }
    }

private:
    struct EntityRecord {
        uint32_t generation = 0;
        bool alive = false;
        size_t archetype = 0;
        size_t row = 0;
    };

    size_t ensureArchetype(const std::vector<ComponentTypeId>& sortedTypes);
    size_t appendRow(size_t archetypeIndex, Entity entity);
    /// Destroys the row's components, then removes the row (moving the last
    /// row into its place).
    void destroyRow(size_t archetypeIndex, size_t row);
    /// Removes a row whose components have already been moved out/destroyed.
    void dropRow(size_t archetypeIndex, size_t row);
    void moveSharedColumns(detail::Archetype& src, size_t srcRow,
                           detail::Archetype& dst, size_t dstRow);

    template <typename... Ts>
    static bool matchColumns(detail::Archetype& arch, size_t* cols) {
        const ComponentTypeId want[] = {componentTypeId<Ts>()...};
        for (size_t i = 0; i < sizeof...(Ts); ++i) {
            const size_t col = arch.column(want[i]);
            if (col == detail::Archetype::kNoColumn) {
                return false;
            }
            cols[i] = col;
        }
        return true;
    }

    template <typename F, typename... Ts, size_t... Is>
    static void invokeRows(F& fn, detail::Archetype& arch,
                           const size_t (&cols)[sizeof...(Ts)],
                           std::index_sequence<Is...>) {
        for (size_t row = 0; row < arch.entities.size(); ++row) {
            fn(arch.entities[row],
               *static_cast<Ts*>(arch.columns[cols[Is]].at(row))...);
        }
    }

    std::vector<EntityRecord> records_;
    std::vector<uint32_t> freeIndices_;
    std::vector<detail::Archetype> archetypes_;
    std::map<std::vector<ComponentTypeId>, size_t> archetypeLookup_;
    size_t aliveCount_ = 0;
};

}  // namespace ply
