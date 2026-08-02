#include "core/economy/FactionEconomy.h"

namespace sr::core::economy {

int FactionEconomy::Stock(const FactionId& faction) const {
    const auto it = stock_.find(faction);
    return it != stock_.end() ? it->second : 0;
}

void FactionEconomy::Deposit(const FactionId& faction, int amount) {
    if (amount <= 0) {
        return;
    }
    stock_[faction] += amount;
    totalProduction_[faction] += amount;
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

int FactionEconomy::TotalProduction(const FactionId& faction) const {
    const auto it = totalProduction_.find(faction);
    return it != totalProduction_.end() ? it->second : 0;
}

}  // namespace sr::core::economy
