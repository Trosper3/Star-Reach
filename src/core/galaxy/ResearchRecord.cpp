#include "core/galaxy/ResearchRecord.h"

#include <vector>

namespace sr::core::galaxy {

ResearchLedger::Id ResearchLedger::Add(ResearchRecord record) {
    const Id id = nextId_++;
    records_.emplace(id, std::move(record));
    return id;
}

void ResearchLedger::Remove(Id id) {
    records_.erase(id);
}

const ResearchRecord* ResearchLedger::Find(Id id) const {
    const auto it = records_.find(id);
    return it == records_.end() ? nullptr : &it->second;
}

std::vector<ResearchLedger::Id> ResearchLedger::IdsForSystem(const std::string& systemId) const {
    std::vector<Id> ids;
    for (const auto& [id, record] : records_) {
        if (record.systemId == systemId) {
            ids.push_back(id);
        }
    }
    return ids;
}

}  // namespace sr::core::galaxy
