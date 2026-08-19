#include "core/registries/DamageTypeEffects.h"

#include <fstream>

namespace sr::core {
namespace {

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

LoadReport DamageTypeEffects::LoadFromFile(const std::filesystem::path& path) {
    LoadReport report;
    rows_.clear();

    nlohmann::json document;
    if (!ReadDocument(path, document, report)) {
        return report;
    }
    const std::string name = path.filename().string();
    if (!document.is_object() || !document.contains("damage_types") ||
        !document["damage_types"].is_array()) {
        report.errors.push_back({name, "root", "expected an array at 'damage_types'"});
        return report;
    }

    const nlohmann::json& entries = document["damage_types"];
    for (size_t i = 0; i < entries.size(); ++i) {
        const std::string context = "damage_types[" + std::to_string(i) + "]";
        if (!entries[i].is_object()) {
            report.errors.push_back({name, context, "must be an object"});
            continue;
        }
        const JsonReader reader(entries[i], name, context, report);

        std::string typeToken;
        reader.Require("type", typeToken);
        if (typeToken.empty()) {
            continue;  // Require already reported the missing/mistyped field.
        }
        DamageType type = DamageType::Kinetic;
        if (!FromString(typeToken, type)) {
            reader.Error(std::string("field 'type' has unknown value '") + typeToken + "'");
            continue;
        }

        DamageTypeEffect effect;
        reader.Optional("alwaysAbsorbedByAnyShield", effect.alwaysAbsorbedByAnyShield);
        reader.Optional("bypassStillDrainsShieldCharge", effect.bypassStillDrainsShieldCharge);
        reader.Optional("hullDamageFraction", effect.hullDamageFraction);
        reader.Optional("powerDrainFraction", effect.powerDrainFraction);

        const auto key = static_cast<std::uint8_t>(type);
        if (!rows_.emplace(key, effect).second) {
            report.errors.push_back({name, context, "duplicate damage type"});
        }
    }
    return report;
}

DamageTypeEffect DamageTypeEffects::Lookup(DamageType type) const {
    const auto it = rows_.find(static_cast<std::uint8_t>(type));
    return it == rows_.end() ? DamageTypeEffect{} : it->second;
}

}  // namespace sr::core
