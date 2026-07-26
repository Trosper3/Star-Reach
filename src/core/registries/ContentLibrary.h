#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/registries/JsonReader.h"
#include "shared/blueprints/DefLibrary.h"
#include "shared/blueprints/ShipBlueprint.h"

namespace sr::core {

// The loaded content set: every shell, module, and ship blueprint authored in data/base_game/.
//
// Implements DefLibrary so blueprint validation in sr_shared can resolve ids without sr_shared
// depending on sr_core. That inversion is what keeps the layer graph acyclic.
class ContentLibrary final : public DefLibrary {
public:
    // Loads shells.json, modules.json, and ships.json from `directory`.
    //
    // Never throws and never partially aborts: a malformed entry is reported and skipped, and
    // the rest of the set still loads. Callers check the report and decide whether to run. That
    // matters for the eventual data/mods/ overlay, where one bad mod must not take the game
    // down with it.
    LoadReport LoadFromDirectory(const std::filesystem::path& directory);

    const ShellDef* FindShell(const ShellId& id) const override;
    const ModuleDef* FindModule(const ModuleId& id) const override;
    const ShipBlueprint* FindShip(const BlueprintId& id) const;

    // Runs Validate() over every loaded ship blueprint. Called by the content-validation test
    // and by tools/, so shipping a blueprint that cannot be instantiated fails CI rather than
    // the player's session.
    LoadReport ValidateAll() const;

    size_t ShellCount() const { return shells_.size(); }
    size_t ModuleCount() const { return modules_.size(); }
    size_t ShipCount() const { return ships_.size(); }

    std::vector<BlueprintId> ShipIds() const;

private:
    // Parses one top-level array file. `key` names the array inside the document.
    template <typename T, typename Parser>
    void LoadArrayFile(const std::filesystem::path& path, const char* key, Parser parse,
                       std::unordered_map<std::string, T>& out, LoadReport& report);

    std::unordered_map<std::string, ShellDef> shells_;
    std::unordered_map<std::string, ModuleDef> modules_;
    std::unordered_map<std::string, ShipBlueprint> ships_;
};

}  // namespace sr::core
