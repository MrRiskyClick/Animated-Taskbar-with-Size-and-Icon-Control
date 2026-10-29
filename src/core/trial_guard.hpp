// Original Stage-0 prototype. No Windows or Explorer state is changed here.
// This in-memory engine is NOT a durable journal or an independent watchdog.
#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace animated_taskbar {

class TrialGuard {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    enum class State {
        Idle,
        Active,
        Reverting,
        Reverted,
        RollbackFailed
    };

    struct Operation {
        std::string name;
        // Caller must capture/record the baseline BEFORE invoking apply.
        // undo must tolerate partial apply and repeat attempts.
        std::function<bool()> apply;
        std::function<bool()> undo;
    };

    bool Arm(TimePoint now, std::chrono::milliseconds lifetime) {
        if (state_ != State::Idle || lifetime.count() <= 0) {
            return false;
        }
        deadline_ = now + lifetime;
        state_ = State::Active;
        return true;
    }

    bool Apply(TimePoint now, Operation operation) {
        if (Expire(now) || state_ != State::Active || !operation.apply ||
            !operation.undo) {
            return false;
        }

        // Register compensating action first, in case apply partially mutates.
        operations_.push_back(std::move(operation));
        bool ok = false;
        try {
            ok = operations_.back().apply();
        } catch (...) {
            ok = false;
        }

        if (!ok) {
            Revert();
        }
        return ok;
    }

    bool Expire(TimePoint now) {
        if (state_ == State::Active && now >= deadline_) {
            Revert();
            return true;
        }
        return state_ == State::Reverted || state_ == State::RollbackFailed;
    }

    bool Revert() {
        if (state_ != State::Active && state_ != State::RollbackFailed) {
            return state_ == State::Reverted;
        }

        state_ = State::Reverting;
        bool allRestored = true;
        // Retry only failed inverses; successful ones are removed.
        std::vector<Operation> failed;
        for (auto it = operations_.rbegin(); it != operations_.rend(); ++it) {
            bool restored = false;
            try {
                restored = it->undo && it->undo();
            } catch (...) {
                restored = false;
            }
            if (!restored) {
                allRestored = false;
                failed.push_back(*it);
            }
        }
        operations_.assign(failed.rbegin(), failed.rend());
        state_ = allRestored ? State::Reverted : State::RollbackFailed;
        return allRestored;
    }

    State state() const { return state_; }
    std::size_t pendingUndoCount() const { return operations_.size(); }
    TimePoint deadline() const { return deadline_; }

private:
    State state_ = State::Idle;
    TimePoint deadline_{};
    std::vector<Operation> operations_;
};

}  // namespace animated_taskbar
