#include "modes/space/systems/TemplateMarketSystem.h"

#include <algorithm>

#include "core/events/Intent.h"

namespace sr::space::template_market_system {
namespace {

// DiplomacyMatrix's Relation is a six-state band (Relation.h, architecture.md 12.32), but
// architecture.md 12.7's roll formula wants "core/diplomacy/DiplomacyMatrix, -100..100." This is
// the discrete-to-numeric mapping -- evenly spaced across the documented range, zero for Neutral.
// The exact per-band value is a tuning question for whoever wires the roll for real (architecture
// md 12.32); only the ordering and the Neutral==0 anchor are load-bearing here.
float RelationValue(core::diplomacy::Relation relation) {
    switch (relation) {
        case core::diplomacy::Relation::War: return -100.0f;
        case core::diplomacy::Relation::Hostile: return -60.0f;
        case core::diplomacy::Relation::Distrustful: return -20.0f;
        case core::diplomacy::Relation::Neutral: return 0.0f;
        case core::diplomacy::Relation::Friendly: return 60.0f;
        case core::diplomacy::Relation::Allied: return 100.0f;
    }
    return 0.0f;
}

// features.md 5.3: Distrustful and below refuses the pitch before evaluating anything else --
// Neutral, Friendly and Allied all pass.
bool PassesGate(const SystemContext& ctx, const core::PitchTemplateIntent& pitch) {
    if (ctx.diplomacy == nullptr) {
        return false;
    }
    return ctx.diplomacy->Get(pitch.buyerFaction, pitch.sellerFaction) >=
           core::diplomacy::Relation::Neutral;
}

bool PassesAccept(const SystemContext& ctx, const core::PitchTemplateIntent& pitch) {
    if (!pitch.archetypeFits || !pitch.beatsCurrentManufacture) {
        return false;
    }
    if (ctx.economy == nullptr) {
        return false;
    }
    return ctx.economy->Stock(pitch.buyerFaction) >= pitch.materialsCost;
}

float PayoutMultiplier(const SystemContext& ctx, const core::PitchTemplateIntent& pitch) {
    const float reputationScore =
        ctx.reputation != nullptr ? ctx.reputation->Score(pitch.buyerFaction) : 0.0f;
    const float relationValue =
        ctx.diplomacy != nullptr
            ? RelationValue(ctx.diplomacy->Get(pitch.buyerFaction, pitch.sellerFaction))
            : 0.0f;

    float rateBonus =
        0.5f * reputationScore + 0.4f * relationValue + (pitch.archetypeFits ? 20.0f : -10.0f);
    rateBonus = std::clamp(rateBonus, -100.0f, 100.0f);
    return 1.0f + rateBonus / 200.0f;
}

void ResolvePitch(const SystemContext& ctx, const core::PitchTemplateIntent& pitch) {
    if (!PassesGate(ctx, pitch) || !PassesAccept(ctx, pitch)) {
        return;
    }

    ctx.economy->Spend(pitch.buyerFaction, pitch.materialsCost);
    if (ctx.knowledge != nullptr) {
        ctx.knowledge->Copy(pitch.sellerNetwork, pitch.buyerNetwork, pitch.templateId.str());
    }

    const float multiplier = PayoutMultiplier(ctx, pitch);
    if (pitch.payment == core::TemplatePayment::LumpSum) {
        ctx.economy->Deposit(pitch.sellerFaction, static_cast<int>(pitch.basePayout * multiplier));
    } else {
        ctx.economy->AddRoyalty(pitch.buyerFaction, pitch.templateId, pitch.sellerFaction,
                                pitch.basePayout * multiplier);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    ctx.intents.ForEach<core::PitchTemplateIntent>(
        [&ctx](const core::PitchTemplateIntent& pitch) { ResolvePitch(ctx, pitch); });
}

}  // namespace sr::space::template_market_system
