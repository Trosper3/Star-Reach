#include "core/galaxy/Topology.h"

namespace sr::core::galaxy {

SystemId IdFromCoord(SystemCoord coord) {
    const auto ux = static_cast<std::uint32_t>(coord.x);
    const auto uy = static_cast<std::uint32_t>(coord.y);
    return (static_cast<SystemId>(ux) << 32) | static_cast<SystemId>(uy);
}

SystemCoord CoordFromId(SystemId id) {
    const auto ux = static_cast<std::uint32_t>(id >> 32);
    const auto uy = static_cast<std::uint32_t>(id & 0xFFFFFFFFULL);
    return SystemCoord{static_cast<std::int32_t>(ux), static_cast<std::int32_t>(uy)};
}

const SystemRecord* Topology::Find(SystemId id) const {
    const auto it = records_.find(id);
    return it != records_.end() ? &it->second : nullptr;
}

SystemRecord& Topology::GetOrCreate(SystemCoord coord) {
    const SystemId id = IdFromCoord(coord);
    auto [it, inserted] = records_.try_emplace(id);
    if (inserted) {
        it->second.coord = coord;
    }
    return it->second;
}

void Topology::Erase(SystemId id) {
    records_.erase(id);
}

std::vector<SystemCoord> Topology::Neighbors(SystemCoord coord) const {
    std::vector<SystemCoord> neighbors;
    neighbors.reserve(8);
    for (std::int32_t dy = -1; dy <= 1; ++dy) {
        for (std::int32_t dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            neighbors.push_back(SystemCoord{coord.x + dx, coord.y + dy});
        }
    }
    return neighbors;
}

}  // namespace sr::core::galaxy
