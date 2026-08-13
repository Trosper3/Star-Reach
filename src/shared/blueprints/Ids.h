#pragma once

#include <compare>
#include <functional>
#include <string>
#include <utility>

// Stable string identity (architecture.md Law 3).
//
// Entity handles are registry-local and volatile; they never enter a save, a wire packet, or a
// player Template. Anything persistent is one of the ids below.
//
// These are strong types rather than aliases on purpose. `ShellId` and `ModuleId` are both
// strings, they both come out of the same JSON files, and mixing them up produces a lookup that
// fails at runtime with a message about a missing id rather than at compile time. One template
// and a tag struct removes that entire class of bug.
namespace sr {

template <typename Tag>
class StringId {
public:
    StringId() = default;
    explicit StringId(std::string value) : value_(std::move(value)) {}

    const std::string& str() const noexcept { return value_; }
    bool empty() const noexcept { return value_.empty(); }

    friend auto operator<=>(const StringId&, const StringId&) = default;
    friend bool operator==(const StringId&, const StringId&) = default;

private:
    std::string value_;
};

struct ModuleIdTag {};
struct ShellIdTag {};
struct BlueprintIdTag {};
struct FactionIdTag {};
struct MountIdTag {};
struct KnowledgeNetworkIdTag {};
struct MaterialIdTag {};

using ModuleId = StringId<ModuleIdTag>;
using ShellId = StringId<ShellIdTag>;
using BlueprintId = StringId<BlueprintIdTag>;
using FactionId = StringId<FactionIdTag>;

// Content-registry lookup key for a MaterialDef (shared/blueprints/MaterialDef.h). Deliberately
// not used for MaterialStack/MaterialDrop/MaterialChance's existing `std::string materialId`
// fields -- those stay plain strings, per Loot.h's own comment, until something forces that wider
// rename. CargoView resolves a deposit's mass through this id before it ever reaches shared/rig/.
using MaterialId = StringId<MaterialIdTag>;

// Mount ids are unique only within one blueprint, which is what lets a Template be relocated
// or re-parented without a global rename.
using MountId = StringId<MountIdTag>;

// Stable id for a knowledge network (architecture.md 12.1, core/knowledge/KnowledgeNetwork.h).
// Named `KnowledgeNetworkId` rather than the bare `NetworkId` architecture.md 12.1's table uses --
// `sr::space::NetworkId` (modes/space/data/SystemWorld.h) already owns that name for a completely
// different concept, a per-registry entity-addressing id. Different namespace, so it would still
// compile, but the two are unrelated enough that sharing a name would mislead more than it saves.
using KnowledgeNetworkId = StringId<KnowledgeNetworkIdTag>;

}  // namespace sr

namespace std {
template <typename Tag>
struct hash<sr::StringId<Tag>> {
    size_t operator()(const sr::StringId<Tag>& id) const noexcept {
        return std::hash<std::string>{}(id.str());
    }
};
}  // namespace std
