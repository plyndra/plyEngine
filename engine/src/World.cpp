#include "plyEngine/World.hpp"

#include <atomic>
#include <vector>

namespace ply {

namespace detail {

namespace {
std::atomic<uint32_t>& counter() {
    static std::atomic<uint32_t> c{0};
    return c;
}

std::vector<ComponentMeta>& registry() {
    static std::vector<ComponentMeta> r;
    return r;
}
}  // namespace

ComponentTypeId registerComponent(const ComponentMeta& meta) {
    auto& r = registry();
    const ComponentTypeId id = counter().fetch_add(1);
    if (r.size() <= id) {
        r.resize(id + 1);
    }
    r[id] = meta;
    return id;
}

const ComponentMeta* componentMeta(ComponentTypeId id) {
    auto& r = registry();
    if (id >= r.size()) {
        return nullptr;
    }
    return &r[id];
}

uint32_t registeredComponentCount() { return counter().load(); }

}  // namespace detail

World::World() {
    // Archetype 0 is the empty archetype: entities with no components.
    ensureArchetype({});
}

World::~World() {
    // Destroy every live component so non-trivial components release resources.
    for (detail::Archetype& arch : archetypes_) {
        for (size_t row = 0; row < arch.entities.size(); ++row) {
            for (detail::Column& column : arch.columns) {
                column.meta.destroy(column.at(row));
            }
        }
    }
}

Entity World::spawn() {
    uint32_t index;
    if (!freeIndices_.empty()) {
        index = freeIndices_.back();
        freeIndices_.pop_back();
        ++records_[index].generation;
        records_[index].alive = true;
    } else {
        index = static_cast<uint32_t>(records_.size());
        records_.push_back(EntityRecord{});
        records_[index].generation = 1;
        records_[index].alive = true;
    }

    const size_t row = appendRow(0, Entity{index, records_[index].generation});
    records_[index].archetype = 0;
    records_[index].row = row;

    ++aliveCount_;
    return Entity{index, records_[index].generation};
}

void World::destroy(Entity entity) {
    if (!alive(entity)) {
        return;
    }
    auto& record = records_[entity.index];
    destroyRow(record.archetype, record.row);
    record.alive = false;
    freeIndices_.push_back(entity.index);
    --aliveCount_;
}

bool World::alive(Entity entity) const {
    if (!entity.valid() || entity.index >= records_.size()) {
        return false;
    }
    const auto& record = records_[entity.index];
    return record.alive && record.generation == entity.generation;
}

size_t World::ensureArchetype(const std::vector<ComponentTypeId>& sortedTypes) {
    auto it = archetypeLookup_.find(sortedTypes);
    if (it != archetypeLookup_.end()) {
        return it->second;
    }

    detail::Archetype arch;
    arch.types = sortedTypes;
    arch.columns.reserve(sortedTypes.size());
    for (ComponentTypeId id : sortedTypes) {
        const detail::ComponentMeta* meta = detail::componentMeta(id);
        arch.columns.emplace_back(meta ? *meta : detail::ComponentMeta{});
    }

    const size_t index = archetypes_.size();
    archetypes_.push_back(std::move(arch));
    archetypeLookup_[sortedTypes] = index;
    return index;
}

size_t World::appendRow(size_t archetypeIndex, Entity entity) {
    detail::Archetype& arch = archetypes_[archetypeIndex];
    const size_t live = arch.entities.size();
    const size_t row = live;
    // The archetype must know the real handle so iteration yields valid ids.
    arch.entities.push_back(entity);
    for (detail::Column& column : arch.columns) {
        column.grow(row + 1, live);
    }
    return row;
}

void World::destroyRow(size_t archetypeIndex, size_t row) {
    detail::Archetype& arch = archetypes_[archetypeIndex];
    for (detail::Column& column : arch.columns) {
        column.meta.destroy(column.at(row));
    }
    dropRow(archetypeIndex, row);
}

void World::dropRow(size_t archetypeIndex, size_t row) {
    detail::Archetype& arch = archetypes_[archetypeIndex];
    const size_t last = arch.entities.size() - 1;

    if (row != last) {
        // Move the last row into the hole and repoint its entity.
        for (detail::Column& column : arch.columns) {
            column.meta.relocate(column.at(row), column.at(last));
        }
        const Entity moved = arch.entities[last];
        arch.entities[row] = moved;
        if (alive(moved)) {
            records_[moved.index].row = row;
        }
    }

    arch.entities.pop_back();
}

void World::moveSharedColumns(detail::Archetype& src, size_t srcRow,
                              detail::Archetype& dst, size_t dstRow) {
    for (size_t sc = 0; sc < src.types.size(); ++sc) {
        const size_t dc = dst.column(src.types[sc]);
        if (dc == detail::Archetype::kNoColumn) {
            continue;
        }
        src.columns[sc].meta.relocate(dst.columns[dc].at(dstRow),
                                      src.columns[sc].at(srcRow));
    }
}

}  // namespace ply
