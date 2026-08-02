#include "core/registries/ContentLibrary.h"

#include <fstream>

#include "core/registries/BlueprintJson.h"
#include "shared/blueprints/Validation.h"

namespace sr::core {
namespace {

// Reads and parses one JSON document, reporting rather than throwing.
bool ReadDocument(const std::filesystem::path& path, nlohmann::json& out, LoadReport& report) {
    const std::string name = path.filename().string();
    if (!std::filesystem::exists(path)) {
        report.errors.push_back({name, "file", "not found at " + path.string()});
        return false;
    }
    std::ifstream stream(path);
    if (!stream) {
        report.errors.push_back({name, "file", "could not be opened"});
        return false;
    }
    out = nlohmann::json::parse(stream, nullptr, false, /*ignore_comments=*/true);
    if (out.is_discarded()) {
        report.errors.push_back({name, "file", "is not valid JSON"});
        return false;
    }
    return true;
}

}  // namespace

template <typename T, typename Parser>
void ContentLibrary::LoadArrayFile(const std::filesystem::path& path, const char* key, Parser parse,
                                   std::unordered_map<std::string, T>& out, LoadReport& report) {
    nlohmann::json document;
    if (!ReadDocument(path, document, report)) {
        return;
    }
    const std::string name = path.filename().string();
    if (!document.is_object() || !document.contains(key) || !document[key].is_array()) {
        report.errors.push_back({name, "root", std::string("expected an array at '") + key + "'"});
        return;
    }

    const nlohmann::json& entries = document[key];
    for (size_t i = 0; i < entries.size(); ++i) {
        const std::string context = std::string(key) + "[" + std::to_string(i) + "]";
        if (!entries[i].is_object()) {
            report.errors.push_back({name, context, "must be an object"});
            continue;
        }
        const JsonReader reader(entries[i], name, context, report);
        T value = parse(reader);
        if (value.id.empty()) {
            continue;  // Missing id already reported by the parser.
        }
        // Rejected here rather than silently overwritten: features.md validation rule 9 wants
        // ids unique, and a duplicate is far more often a copy-paste slip than an intended
        // override. data/mods/ overrides, when they exist, will be an explicit merge pass.
        if (!out.emplace(value.id.str(), std::move(value)).second) {
            report.errors.push_back({name, context, "duplicate id"});
        }
    }
}

LoadReport ContentLibrary::LoadFromDirectory(const std::filesystem::path& directory) {
    LoadReport report;
    shells_.clear();
    modules_.clear();
    ships_.clear();

    LoadArrayFile(directory / "shells.json", "shells", ParseShellDef, shells_, report);
    LoadArrayFile(directory / "modules.json", "modules", ParseModuleDef, modules_, report);
    LoadArrayFile(directory / "ships.json", "ships", ParseShipBlueprint, ships_, report);
    return report;
}

const ShellDef* ContentLibrary::FindShell(const ShellId& id) const {
    const auto it = shells_.find(id.str());
    return it == shells_.end() ? nullptr : &it->second;
}

const ModuleDef* ContentLibrary::FindModule(const ModuleId& id) const {
    const auto crafted = craftedModules_.find(id.str());
    if (crafted != craftedModules_.end()) {
        return &crafted->second;
    }
    const auto it = modules_.find(id.str());
    return it == modules_.end() ? nullptr : &it->second;
}

void ContentLibrary::RegisterCraftedModule(ModuleDef module) {
    const std::string id = module.id.str();
    craftedModules_[id] = std::move(module);
}

const ShipBlueprint* ContentLibrary::FindShip(const BlueprintId& id) const {
    const auto it = ships_.find(id.str());
    return it == ships_.end() ? nullptr : &it->second;
}

std::vector<BlueprintId> ContentLibrary::ShipIds() const {
    std::vector<BlueprintId> ids;
    ids.reserve(ships_.size());
    for (const auto& [key, ship] : ships_) {
        ids.push_back(ship.id);
    }
    return ids;
}

LoadReport ContentLibrary::ValidateAll() const {
    LoadReport report;
    for (const auto& [key, ship] : ships_) {
        const ValidationResult result = Validate(ship, *this);
        for (const auto& error : result.errors) {
            const std::string context =
                error.mount.empty() ? ship.id.str() : ship.id.str() + " > " + error.mount.str();
            report.errors.push_back(
                {"ships.json", context, std::string(ToString(error.rule)) + ": " + error.message});
        }
    }
    return report;
}

}  // namespace sr::core
