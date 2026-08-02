#include "modes/space/systems/TemplateMarketSystem.h"

#include <algorithm>

#include "core/events/Intent.h"

namespace sr::space::template_market_system {
namespace {

// DiplomacyMatrix has no native numeric value -- Relation is a 3-state enum (Relation.h) -- but
// architecture.md 12.7's roll formula wants "core/diplomacy/DiplomacyMatrix, -100..100." This is
// the discrete-to-numeric bridge: the extremes of that documented range for Hostile/Friendly,
// zero for Neutral.
float RelationValue(core::diplomacy::Relation relation) {
    switch (relation) {
        case core::diplomacy::Relation::Hostile: return -100.0f;
        case core::diplomacy::Relation::Friendly: return 100.0f;
        case core::diplomacy::Relation::Neutral:
        default: return 0.0f;
    }
}

bool PassesGate(const SystemContext& ctx, const core::PitchTemplateIntent& pitch) {
    if (ctx.diplomacy == nullptr) {
        return false;
    }
    return ctx.diplomacy->Get(pitch.buyerFaction, pitch.sellerFaction) !=
           core::diplomacy::Relation::Hostile;
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
