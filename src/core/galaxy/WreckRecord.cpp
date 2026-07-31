#include "core/galaxy/WreckRecord.h"

#include <vector>

namespace sr::core::galaxy {

WreckLedger::Id WreckLedger::Add(WreckRecord record) {
    const Id id = nextId_++;
    records_.emplace(id, std::move(record));
    return id;
}

void WreckLedger::Remove(Id id) {
    records_.erase(id);
}

const WreckRecord* WreckLedger::Find(Id id) const {
    const auto it = records_.find(id);
    return it == records_.end() ? nullptr : &it->second;
}

std::vector<WreckLedger::Id> WreckLedger::IdsForSystem(const std::string& systemId) const {
    std::vector<Id> ids;
    for (const auto& [id, record] : records_) {
        if (record.systemId == systemId) {
            ids.push_back(id);
        }
    }
    return ids;
}

void WreckLedger::Tick(float dt) {
    std::vector<Id> expired;
    for (auto& [id, record] : records_) {
        record.lifetimeSeconds -= dt;
        if (record.lifetimeSeconds <= 0.0f) {
            expired.push_back(id);
        }
    }
    for (const Id id : expired) {
        records_.erase(id);
    }
}

}  // namespace sr::core::galaxy
