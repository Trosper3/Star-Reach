#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/registries/DamageTypeEffects.h"
#include "core/registries/JsonReader.h"
#include "shared/blueprints/DefLibrary.h"
#include "shared/blueprints/ElementDef.h"
#include "shared/blueprints/ShipBlueprint.h"

namespace sr::core {

// The loaded content set: every shell, module, and ship blueprint authored in data/base_game/.
//
// Implements DefLibrary so blueprint validation in sr_shared can resolve ids without sr_shared
// depending on sr_core. That inversion is what keeps the layer graph acyclic.
class ContentLibrary final : public DefLibrary {
public:
    // Loads shells.json, modules.json, elements.json, ships.json, and damage_types.json from
    // `directory`.
    //
    // Never throws and never partially aborts: a malformed entry is reported and skipped, and
    // the rest of the set still loads. Callers check the report and decide whether to run. That
    // matters for the eventual data/mods/ overlay, where one bad mod must not take the game
    // down with it.
    LoadReport LoadFromDirectory(const std::filesystem::path& directory);

    const ShellDef* FindShell(const ShellId& id) const override;

    // Checks runtime-registered modules (below) first, then the JSON-loaded set. A crafted id
    // never collides with an authored one in practice -- RegisterCraftedModule's caller
    // (EngineerSystem, architecture.md 12.12) derives ids from the two source modules, and
    // content authors do not use that scheme -- but "crafted wins" is the defined tie-break if
    // it ever happens, since a crafted module is player state, not swappable authored content.
    const ModuleDef* FindModule(const ModuleId& id) const override;

    const ShipBlueprint* FindShip(const BlueprintId& id) const;

    // Not part of DefLibrary -- blueprint validation never resolves an element id, only
    // CargoView-adjacent systems do, and they already have a concrete ContentLibrary in hand.
    const ElementDef* FindElement(const ElementId& id) const;

    // Never fails to resolve -- an unauthored DamageType (Kinetic, Energy) returns the default
    // absorb-or-bypass row (architecture.md 12.33).
    DamageTypeEffect LookupDamageTypeEffect(DamageType type) const;

    // Registers a module built at runtime rather than loaded from JSON -- architecture.md 12.12's
    // EngineerMenu, which merges two owned modules into a new one. The merged ModuleDef is
    // player-generated content, not authored content, the same distinction CustomizeMenu's
    // Template draft already makes (architecture.md 12.9): its caller builds it by plain
    // declaration and field assignment rather than an initializer, so Law 10's
    // check_content_pipeline.py -- which forbids constructing ModuleDef with an initializer
    // outside core/registries/, tests/, tools/ -- has nothing to flag. Survives
    // LoadFromDirectory(): reloading the authored set must not erase a player's crafted
    // inventory.
    void RegisterCraftedModule(ModuleDef module);

    // Runs Validate() over every loaded ship blueprint. Called by the content-validation test
    // and by tools/, so shipping a blueprint that cannot be instantiated fails CI rather than
    // the player's session.
    LoadReport ValidateAll() const;

    size_t ShellCount() const { return shells_.size(); }
    size_t ModuleCount() const { return modules_.size(); }
    size_t ShipCount() const { return ships_.size(); }
    size_t ElementCount() const { return elements_.size(); }

    std::vector<BlueprintId> ShipIds() const;

private:
    // Parses one top-level array file. `key` names the array inside the document.
    template <typename T, typename Parser>
    void LoadArrayFile(const std::filesystem::path& path, const char* key, Parser parse,
                       std::unordered_map<std::string, T>& out, LoadReport& report);

    std::unordered_map<std::string, ShellDef> shells_;
    std::unordered_map<std::string, ModuleDef> modules_;
    std::unordered_map<std::string, ShipBlueprint> ships_;
    std::unordered_map<std::string, ModuleDef> craftedModules_;
    std::unordered_map<std::string, ElementDef> elements_;
    DamageTypeEffects damageTypeEffects_;
};

}  // namespace sr::core
