#pragma once

#include <utility>
#include <vector>

#include "core/events/Intent.h"

namespace sr::core {

// The single channel from input to simulation (Law 9).
//
// Deliberately not an event bus. Law 12 bans a system from emitting in response to receiving,
// and the cheapest way to keep that honest is for this queue to have exactly one producer side
// (input / UI / network) and one consumption point per tick, drained by the orchestrator in a
// known order. A system that wants to cause something else to happen calls it, or writes a
// component another system reads -- it does not push here.
class IntentQueue {
public:
    void Push(Intent intent) { pending_.push_back(std::move(intent)); }

    // Systems read this during a tick. Stable for the whole tick: nothing pushes mid-drain.
    const std::vector<Intent>& Pending() const { return pending_; }

    // Called once per fixed tick by the orchestrator, after every system has consumed.
    void Clear() { pending_.clear(); }

    bool Empty() const { return pending_.empty(); }
    size_t Size() const { return pending_.size(); }

    // Convenience for the common case: visit every intent of one alternative.
    template <typename T, typename Fn>
    void ForEach(Fn&& fn) const {
        for (const Intent& intent : pending_) {
            if (const T* typed = std::get_if<T>(&intent)) {
                fn(*typed);
            }
        }
    }

private:
    std::vector<Intent> pending_;
};

}  // namespace sr::core
