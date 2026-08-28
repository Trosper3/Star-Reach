#include "modes/space/ui/CodexScreen.h"

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <optional>

#include "core/registries/ContentLibrary.h"
#include "shared/components/NetworkOwner.h"
#include "shared/ui/Fonts.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/UiInput.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::codex_screen {
namespace {

constexpr float kPanelWidth = 760.0f;
constexpr float kPanelTop = 90.0f;
constexpr float kHeaderHeight = 28.0f;
constexpr float kCloseButtonSize = 24.0f;
constexpr float kSearchHeight = 26.0f;
// 28, matching BayView's/EngineeringScreenDraw's own kSiblingStripHeight -- the same chamfered
// sr::ui::TabStrip widget, so the two chip rows read at the same height as every other screen's
// sibling strip rather than a size unique to this file.
constexpr float kChipRowHeight = 28.0f;
constexpr float kRowGap = 8.0f;
constexpr float kSectionLabelHeight = 18.0f;
constexpr float kListHeight = 120.0f;
constexpr int kMaxSearchLength = 40;
// The bordered-icon-box row treatment's own sizes (ModulesMenu's/Storage's/Research's precedent) --
// 4 rows fill kListHeight exactly, replacing the generic sr::ui::DrawListView's 6 flat text lines
// this file drew before (issue #227's visual-chrome pass never reached this file).
constexpr float kIconBoxSize = 22.0f;
constexpr float kRowHeight = 30.0f;

std::string ToUpper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    const std::string upperHaystack = ToUpper(haystack);
    const std::string upperNeedle = ToUpper(needle);
    return upperHaystack.find(upperNeedle) != std::string::npos;
}

entt::entity FindOrCreateSingleton(entt::registry& registry) {
    for (auto [entity] : registry.view<CodexStateSingleton>().each()) {
        return entity;
    }
    const entt::entity singleton = registry.create();
    registry.emplace<CodexStateSingleton>(singleton);
    registry.emplace<CodexState>(singleton);
    return singleton;
}

entt::entity FindSingleton(const entt::registry& registry) {
    for (auto [entity] : registry.view<CodexStateSingleton>().each()) {
        return entity;
    }
    return entt::null;
}

const core::knowledge::KnowledgeNetwork* NetworkFor(
    const entt::registry& registry, entt::entity vesselRoot,
    const core::knowledge::KnowledgeStore& knowledge) {
    if (vesselRoot == entt::null) {
        return nullptr;
    }
    const NetworkOwner* owner = registry.try_get<NetworkOwner>(vesselRoot);
    return owner != nullptr ? knowledge.Get(owner->network) : nullptr;
}

Rectangle PanelBounds() {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float height = kHeaderHeight + kSearchHeight + kChipRowHeight + kChipRowHeight +
                         3.0f * (kSectionLabelHeight + kListHeight) + 5.0f * kRowGap +
                         2.0f * sr::ui::kPanelPadding;
    return Rectangle{(screenWidth - kPanelWidth) * 0.5f, kPanelTop, kPanelWidth, height};
}

struct Layout {
    Rectangle header{};
    Rectangle closeButton{};
    Rectangle search{};
    Rectangle factionChips{};
    Rectangle tierChips{};
    Rectangle moduleLabel{};
    Rectangle moduleList{};
    Rectangle shellLabel{};
    Rectangle shellList{};
    Rectangle materialLabel{};
    Rectangle materialList{};
};

Layout ComputeLayout() {
    const Rectangle bounds = PanelBounds();
    const Rectangle content = sr::ui::PanelContentRect(bounds);
    Layout layout;
    layout.header = {content.x, content.y, content.width, kHeaderHeight};
    layout.closeButton = {content.x + content.width - kCloseButtonSize, content.y, kCloseButtonSize,
                          kCloseButtonSize};
    float y = content.y + kHeaderHeight + kRowGap;
    layout.search = {content.x, y, content.width, kSearchHeight};
    y += kSearchHeight + kRowGap;
    layout.factionChips = {content.x, y, content.width, kChipRowHeight};
    y += kChipRowHeight + kRowGap;
    layout.tierChips = {content.x, y, content.width, kChipRowHeight};
    y += kChipRowHeight + kRowGap;

    layout.moduleLabel = {content.x, y, content.width, kSectionLabelHeight};
    y += kSectionLabelHeight;
    layout.moduleList = {content.x, y, content.width, kListHeight};
    y += kListHeight + kRowGap;

    layout.shellLabel = {content.x, y, content.width, kSectionLabelHeight};
    y += kSectionLabelHeight;
    layout.shellList = {content.x, y, content.width, kListHeight};
    y += kListHeight + kRowGap;

    layout.materialLabel = {content.x, y, content.width, kSectionLabelHeight};
    y += kSectionLabelHeight;
    layout.materialList = {content.x, y, content.width, kListHeight};
    return layout;
}

std::string TagValue(const Entry& entry) {
    const std::string faction =
        entry.faction.empty() ? "UNFACTIONED" : ToUpper(entry.faction.str());
    return faction + "  |  TIER " + std::to_string(entry.tier);
}

// Kept as one build-and-append helper rather than three copy-pasted loops -- Entries()'s only
// three call sites (FindModule/FindShell/FindElement below) differ solely in id type, def type
// and EntryKind, everything else about turning a resolved def into a Row is identical.
template <typename Def>
void AppendEntry(std::vector<Entry>& out, const std::string& id, EntryKind kind, const Def& def) {
    Entry entry;
    entry.id = id;
    entry.kind = kind;
    entry.displayName = def.displayName;
    entry.faction = def.faction;
    entry.tier = def.tier;
    entry.row.label = def.displayName;
    entry.row.glyph[0] =
        static_cast<char>(std::toupper(def.displayName.empty() ? '?' : def.displayName[0]));
    entry.row.value = TagValue(entry);
    out.push_back(std::move(entry));
}

std::vector<sr::ui::Row> ToRows(const std::vector<Entry>& entries) {
    std::vector<sr::ui::Row> rows;
    rows.reserve(entries.size());
    for (const Entry& entry : entries) {
        rows.push_back(entry.row);
    }
    return rows;
}

std::vector<Entry> OfKind(const std::vector<Entry>& entries, EntryKind kind) {
    std::vector<Entry> out;
    for (const Entry& entry : entries) {
        if (entry.kind == kind) {
            out.push_back(entry);
        }
    }
    return out;
}

std::vector<std::string> ChipLabels(const std::vector<FactionId>& factions) {
    std::vector<std::string> labels{"ALL"};
    for (const FactionId& faction : factions) {
        labels.push_back(faction.empty() ? "UNFACTIONED" : ToUpper(faction.str()));
    }
    return labels;
}

std::vector<std::string> ChipLabels(const std::vector<int>& tiers) {
    std::vector<std::string> labels{"ALL"};
    for (const int tier : tiers) {
        labels.push_back("TIER " + std::to_string(tier));
    }
    return labels;
}

}  // namespace

std::vector<Entry> Entries(const core::knowledge::KnowledgeNetwork& network,
                           const core::ContentLibrary& content) {
    std::vector<Entry> entries;
    for (const std::string& id : network.unlockedBlueprints) {
        if (const ModuleDef* module = content.FindModule(ModuleId(id)); module != nullptr) {
            AppendEntry(entries, id, EntryKind::Module, *module);
            continue;
        }
        if (const ShellDef* shell = content.FindShell(ShellId(id)); shell != nullptr) {
            AppendEntry(entries, id, EntryKind::Shell, *shell);
            continue;
        }
        if (const ElementDef* element = content.FindElement(ElementId(id)); element != nullptr) {
            AppendEntry(entries, id, EntryKind::Material, *element);
            continue;
        }
        // Unreachable in practice -- see this function's own header comment.
    }
    return entries;
}

std::vector<FactionId> DistinctFactions(const std::vector<Entry>& entries) {
    std::vector<FactionId> factions;
    for (const Entry& entry : entries) {
        if (std::find(factions.begin(), factions.end(), entry.faction) == factions.end()) {
            factions.push_back(entry.faction);
        }
    }
    return factions;
}

std::vector<int> DistinctTiers(const std::vector<Entry>& entries) {
    std::vector<int> tiers;
    for (const Entry& entry : entries) {
        if (std::find(tiers.begin(), tiers.end(), entry.tier) == tiers.end()) {
            tiers.push_back(entry.tier);
        }
    }
    std::sort(tiers.begin(), tiers.end());
    return tiers;
}

std::vector<Entry> Filtered(const std::vector<Entry>& entries, const CodexState& state) {
    std::vector<Entry> filtered;
    for (const Entry& entry : entries) {
        if (!state.factionFilter.empty() && entry.faction != state.factionFilter) {
            continue;
        }
        if (state.tierFilter != 0 && entry.tier != state.tierFilter) {
            continue;
        }
        if (!ContainsCaseInsensitive(entry.displayName, state.searchQuery)) {
            continue;
        }
        filtered.push_back(entry);
    }
    return filtered;
}

bool IsOpen(const entt::registry& registry) {
    const entt::entity singleton = FindSingleton(registry);
    return singleton != entt::null && registry.get<CodexState>(singleton).open;
}

void Open(entt::registry& registry) {
    registry.get<CodexState>(FindOrCreateSingleton(registry)).open = true;
}

void Close(entt::registry& registry) {
    const entt::entity singleton = FindSingleton(registry);
    if (singleton != entt::null) {
        registry.get<CodexState>(singleton).open = false;
    }
}

void Update(entt::registry& registry, entt::entity vesselRoot,
            const core::knowledge::KnowledgeStore& knowledge, const core::ContentLibrary& content) {
    const entt::entity singleton = FindSingleton(registry);
    if (singleton == entt::null) {
        return;
    }
    CodexState& state = registry.get<CodexState>(singleton);
    if (!state.open) {
        return;
    }

    // Text entry -- polled every frame while open, independent of a mouse click this frame
    // (raylib's GetCharPressed/IsKeyPressed both read "this frame's" state, the same reason
    // avionics_menu::Update is polled unconditionally, SpaceFlight.cpp's own header comment).
    for (int character = GetCharPressed(); character != 0; character = GetCharPressed()) {
        if (character >= 32 && character < 127 &&
            static_cast<int>(state.searchQuery.size()) < kMaxSearchLength) {
            state.searchQuery.push_back(static_cast<char>(character));
        }
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !state.searchQuery.empty()) {
        state.searchQuery.pop_back();
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return;
    }
    const sr::ui::UiInput input{GetMousePosition(), true, 0.0f};
    const Layout layout = ComputeLayout();

    if (sr::ui::ButtonClicked(layout.closeButton, input)) {
        state.open = false;
        return;
    }

    const core::knowledge::KnowledgeNetwork* network = NetworkFor(registry, vesselRoot, knowledge);
    const std::vector<Entry> entries =
        network != nullptr ? Entries(*network, content) : std::vector<Entry>{};

    const std::vector<FactionId> factions = DistinctFactions(entries);
    if (const std::optional<int> hit = sr::ui::TabStripHitTest(
            layout.factionChips, static_cast<int>(factions.size()) + 1, input.cursor)) {
        state.factionFilter =
            *hit == 0 ? FactionId{} : factions[static_cast<std::size_t>(*hit - 1)];
        return;
    }

    const std::vector<int> tiers = DistinctTiers(entries);
    if (const std::optional<int> hit = sr::ui::TabStripHitTest(
            layout.tierChips, static_cast<int>(tiers.size()) + 1, input.cursor)) {
        state.tierFilter = *hit == 0 ? 0 : tiers[static_cast<std::size_t>(*hit - 1)];
        return;
    }
}

namespace {

// architecture.md 2.2's function-length cap -- split out of Draw() below, one section each.

// The label row + divider shared by all three sections: a dim title over a hairline rule --
// mirrors ModulesMenu.cpp's own DrawColumnLabel / Research's/Storage's own SectionLayout label
// treatment, this file's own precedent for a section header now that it draws through fonts too.
void DrawSectionLabel(Rectangle bounds, const sr::ui::Fonts& fonts, const std::string& title) {
    DrawTextEx(fonts.body, title.c_str(), {bounds.x, bounds.y}, 14.0f, 1.0f, sr::ui::kLabelDim);
    const float dividerY = bounds.y + bounds.height;
    DrawLineEx({bounds.x, dividerY}, {bounds.x + bounds.width, dividerY}, 1.0f, sr::ui::kDivider);
}

// One entry row: a bordered icon box (the entry's own glyph carries identity, features.md 3.9),
// its display name, and its already-formatted "FACTION | TIER n" tag right-aligned and dim --
// Storage's own single-line DrawStorageRow shape (no integrity/disabled channel: an unlocked
// Codex entry carries neither), replacing the generic sr::ui::ListView's flat monogram-prefixed
// text line this file drew before.
void DrawCodexRow(Rectangle bounds, const sr::ui::Fonts& fonts, const sr::ui::Row& row) {
    const Rectangle iconBox{bounds.x, bounds.y + (bounds.height - kIconBoxSize) / 2.0f,
                            kIconBoxSize, kIconBoxSize};
    DrawRectangleLinesEx(iconBox, 1.0f, sr::ui::kPanelChrome);
    if (row.glyph[0] != '\0') {
        const Vector2 glyphSize = MeasureTextEx(fonts.heading, row.glyph, 13.0f, 1.0f);
        DrawTextEx(fonts.heading, row.glyph,
                   {iconBox.x + (iconBox.width - glyphSize.x) / 2.0f,
                    iconBox.y + (iconBox.height - glyphSize.y) / 2.0f},
                   13.0f, 1.0f, sr::ui::kPanelChrome);
    }

    const float textX = iconBox.x + iconBox.width + 10.0f;
    DrawTextEx(fonts.heading, row.label.c_str(), {textX, bounds.y + bounds.height / 2.0f - 8.0f},
               14.0f, 1.0f, sr::ui::kValueBright);

    const float valueWidth = MeasureTextEx(fonts.body, row.value.c_str(), 12.0f, 1.0f).x;
    DrawTextEx(fonts.body, row.value.c_str(),
               {bounds.x + bounds.width - valueWidth, bounds.y + bounds.height / 2.0f - 6.0f},
               12.0f, 1.0f, sr::ui::kLabelDim);
}

// The row list, top to bottom inside `bounds`, divider rules between rows -- mirrors Storage's/
// Research's own fixed (non-scrolling) row lists: this file never scrolled past kListHeight's
// worth of rows before this pass either, so that stays out of this pass's scope.
void DrawCodexRows(Rectangle bounds, const sr::ui::Fonts& fonts,
                   const std::vector<sr::ui::Row>& rows, const std::string& emptyMessage) {
    BeginScissorMode(static_cast<int>(bounds.x), static_cast<int>(bounds.y),
                     static_cast<int>(bounds.width), static_cast<int>(bounds.height));
    if (rows.empty()) {
        DrawTextEx(fonts.body, emptyMessage.c_str(), {bounds.x, bounds.y}, 14.0f, 1.0f,
                   sr::ui::kLabelDim);
        EndScissorMode();
        return;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const float y = bounds.y + static_cast<float>(i) * kRowHeight;
        if (y > bounds.y + bounds.height) {
            break;
        }
        if (i > 0) {
            DrawLineEx({bounds.x, y}, {bounds.x + bounds.width, y}, 1.0f, sr::ui::kDivider);
        }
        DrawCodexRow({bounds.x, y, bounds.width, kRowHeight}, fonts, rows[i]);
    }
    EndScissorMode();
}

}  // namespace

void Draw(const entt::registry& registry, entt::entity vesselRoot,
          const core::knowledge::KnowledgeStore& knowledge, const core::ContentLibrary& content,
          const sr::ui::Fonts& fonts) {
    if (!IsOpen(registry)) {
        return;
    }
    const CodexState& state = registry.get<CodexState>(FindSingleton(registry));
    const Layout layout = ComputeLayout();
    sr::ui::DrawPanelFrame(PanelBounds());

    DrawTextEx(fonts.heading, "CODEX", {layout.header.x, layout.header.y}, 22.0f, 1.0f,
               sr::ui::kValueBright);
    sr::ui::DrawChamferedButton(layout.closeButton, "X", fonts.body, 14.0f, sr::ui::kPanelGlass,
                                sr::ui::kStatusCritical, sr::ui::kValueBright);

    DrawRectangleLinesEx(layout.search, 1.0f, sr::ui::kPanelChrome);
    const float searchTextY = layout.search.y + (layout.search.height - 14.0f) / 2.0f;
    const Vector2 searchLabelSize = MeasureTextEx(fonts.body, "SEARCH ", 14.0f, 1.0f);
    DrawTextEx(fonts.body, "SEARCH ", {layout.search.x + 8.0f, searchTextY}, 14.0f, 1.0f,
               sr::ui::kLabelDim);
    const std::string searchValue = state.searchQuery + (state.searchQuery.empty() ? "_" : "");
    DrawTextEx(fonts.body, searchValue.c_str(),
               {layout.search.x + 8.0f + searchLabelSize.x, searchTextY}, 14.0f, 1.0f,
               sr::ui::kValueBright);

    const core::knowledge::KnowledgeNetwork* network = NetworkFor(registry, vesselRoot, knowledge);
    const std::vector<Entry> entries =
        network != nullptr ? Entries(*network, content) : std::vector<Entry>{};
    const std::vector<Entry> filtered = Filtered(entries, state);

    const std::vector<FactionId> factions = DistinctFactions(entries);
    const std::vector<std::string> factionLabels = ChipLabels(factions);
    int factionSelected = 0;
    for (std::size_t i = 0; i < factions.size(); ++i) {
        if (factions[i] == state.factionFilter && !state.factionFilter.empty()) {
            factionSelected = static_cast<int>(i) + 1;
        }
    }
    sr::ui::DrawTabStrip(layout.factionChips, factionLabels, factionSelected, fonts.body);

    const std::vector<int> tiers = DistinctTiers(entries);
    const std::vector<std::string> tierLabels = ChipLabels(tiers);
    int tierSelected = 0;
    for (std::size_t i = 0; i < tiers.size(); ++i) {
        if (tiers[i] == state.tierFilter && state.tierFilter != 0) {
            tierSelected = static_cast<int>(i) + 1;
        }
    }
    sr::ui::DrawTabStrip(layout.tierChips, tierLabels, tierSelected, fonts.body);

    DrawSectionLabel(layout.moduleLabel, fonts, "MODULES");
    DrawCodexRows(layout.moduleList, fonts, ToRows(OfKind(filtered, EntryKind::Module)),
                  "NOTHING UNLOCKED");

    DrawSectionLabel(layout.shellLabel, fonts, "SHELLS");
    DrawCodexRows(layout.shellList, fonts, ToRows(OfKind(filtered, EntryKind::Shell)),
                  "NOTHING UNLOCKED");

    DrawSectionLabel(layout.materialLabel, fonts, "MATERIALS");
    DrawCodexRows(layout.materialList, fonts, ToRows(OfKind(filtered, EntryKind::Material)),
                  "NOTHING UNLOCKED");
}

}  // namespace sr::space::ui::codex_screen
