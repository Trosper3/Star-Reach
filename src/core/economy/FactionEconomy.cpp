#include "core/economy/FactionEconomy.h"

#include <algorithm>

namespace sr::core::economy {

int FactionEconomy::Stock(const FactionId& faction) const {
    const auto it = stock_.find(faction);
    return it != stock_.end() ? it->second : 0;
}

void FactionEconomy::Deposit(const FactionId& faction, int amount) {
    if (amount <= 0) {
        return;
    }

    int remaining = amount;
    const auto streamsIt = royalties_.find(faction);
    if (streamsIt != royalties_.end()) {
        for (const RoyaltyStream& stream : streamsIt->second) {
            const int skimmed = static_cast<int>(static_cast<float>(amount) * stream.rate);
            if (skimmed <= 0) {
                continue;
            }
            stock_[stream.recipient] += skimmed;
            remaining -= skimmed;
        }
    }

    stock_[faction] += std::max(remaining, 0);
}

bool FactionEconomy::Spend(const FactionId& faction, int amount) {
    if (amount <= 0) {
        return true;
    }
    const auto it = stock_.find(faction);
    if (it == stock_.end() || it->second < amount) {
        return false;
    }
    it->second -= amount;
    return true;
}

void FactionEconomy::AddRoyalty(const FactionId& payer, const BlueprintId& templateId,
                                const FactionId& recipient, float rate) {
    std::vector<RoyaltyStream>& streams = royalties_[payer];
    const auto it = std::find_if(streams.begin(), streams.end(), [&](const RoyaltyStream& s) {
        return s.templateId == templateId;
    });
    if (it != streams.end()) {
        it->recipient = recipient;
        it->rate = rate;
    } else {
        streams.push_back(RoyaltyStream{templateId, recipient, rate});
    }
}

void FactionEconomy::RemoveRoyalty(const FactionId& payer, const BlueprintId& templateId) {
    const auto it = royalties_.find(payer);
    if (it == royalties_.end()) {
        return;
    }
    std::vector<RoyaltyStream>& streams = it->second;
    streams.erase(
        std::remove_if(streams.begin(), streams.end(),
                       [&](const RoyaltyStream& s) { return s.templateId == templateId; }),
        streams.end());
}

float FactionEconomy::RoyaltyRate(const FactionId& payer, const BlueprintId& templateId) const {
    const auto it = royalties_.find(payer);
    if (it == royalties_.end()) {
        return 0.0f;
    }
    for (const RoyaltyStream& stream : it->second) {
        if (stream.templateId == templateId) {
            return stream.rate;
        }
    }
    return 0.0f;
}

}  // namespace sr::core::economy
