#include "core/knowledge/KnowledgeNetwork.h"

#include <utility>

namespace sr::core::knowledge {

KnowledgeNetworkId KnowledgeStore::Create(NetworkOwnerKind ownerKind) {
    const KnowledgeNetworkId id(std::to_string(nextId_++));
    KnowledgeNetwork network;
    network.ownerKind = ownerKind;
    networks_.emplace(id, std::move(network));
    return id;
}

void KnowledgeStore::Destroy(const KnowledgeNetworkId& id) {
    networks_.erase(id);
}

const KnowledgeNetwork* KnowledgeStore::Get(const KnowledgeNetworkId& id) const {
    const auto it = networks_.find(id);
    return it == networks_.end() ? nullptr : &it->second;
}

void KnowledgeStore::Grant(const KnowledgeNetworkId& id, NetworkEntryKind kind,
                           const std::string& itemId) {
    const auto it = networks_.find(id);
    if (it == networks_.end()) {
        return;
    }
    switch (kind) {
        case NetworkEntryKind::UnlockedBlueprint:
            it->second.unlockedBlueprints.insert(itemId);
            return;
        case NetworkEntryKind::SavedTemplate: it->second.savedTemplates.insert(itemId); return;
        case NetworkEntryKind::DiscoveredSystem:
            it->second.discoveredSystems.insert(itemId);
            return;
    }
}

void KnowledgeStore::Copy(const KnowledgeNetworkId& sourceNetwork,
                          const KnowledgeNetworkId& destNetwork, const std::string& templateId) {
    const auto sourceIt = networks_.find(sourceNetwork);
    if (sourceIt == networks_.end() || !sourceIt->second.savedTemplates.contains(templateId)) {
        return;
    }
    const auto destIt = networks_.find(destNetwork);
    if (destIt == networks_.end()) {
        return;
    }
    destIt->second.savedTemplates.insert(templateId);
}

const std::unordered_map<KnowledgeNetworkId, KnowledgeNetwork>& KnowledgeStore::All() const {
    return networks_;
}

void KnowledgeStore::LoadNetwork(const KnowledgeNetworkId& id, KnowledgeNetwork network) {
    networks_.insert_or_assign(id, std::move(network));
}

std::uint64_t KnowledgeStore::NextIdCounter() const {
    return nextId_;
}

void KnowledgeStore::SetNextIdCounter(std::uint64_t next) {
    nextId_ = next;
}

}  // namespace sr::core::knowledge
