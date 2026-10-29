#include "src/core/trial_guard.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using animated_taskbar::TrialGuard;
using namespace std::chrono_literals;

int main() {
    const auto t0 = TrialGuard::Clock::now();

    // Bounded trial expires and executes compensation.
    {
        TrialGuard trial;
        int value = 42;
        assert(trial.Arm(t0, 120s));
        assert(trial.Apply(t0, {"height", [&] { value = 60; return true; },
                                    [&] { value = 42; return true; }}));
        assert(value == 60);
        assert(!trial.Expire(t0 + 119s));
        assert(trial.Expire(t0 + 120s));
        assert(value == 42);
        assert(trial.state() == TrialGuard::State::Reverted);
        assert(!trial.Arm(t0 + 121s, 120s)); // no silent re-arm
        assert(!trial.Apply(t0 + 121s, {"forbidden", [] { return true; },
                                         [] { return true; }}));
    }

    // Reverse order reverts dependent modifications correctly.
    {
        TrialGuard trial;
        std::vector<std::string> calls;
        assert(trial.Arm(t0, 120s));
        assert(trial.Apply(t0, {"a", [&] { calls.push_back("apply-a"); return true; },
                                  [&] { calls.push_back("undo-a"); return true; }}));
        assert(trial.Apply(t0, {"b", [&] { calls.push_back("apply-b"); return true; },
                                  [&] { calls.push_back("undo-b"); return true; }}));
        assert(trial.Revert());
        assert((calls == std::vector<std::string>{
                    "apply-a", "apply-b", "undo-b", "undo-a"}));
        assert(trial.Revert()); // idempotent
        assert(trial.pendingUndoCount() == 0);
    }

    // Apply failure after partial mutation must also be rolled back.
    {
        TrialGuard trial;
        int n = 1;
        assert(trial.Arm(t0, 120s));
        assert(!trial.Apply(t0, {"partial", [&] { n = 9; return false; },
                                        [&] { n = 1; return true; }}));
        assert(n == 1);
        assert(trial.state() == TrialGuard::State::Reverted);
    }

    // Failed compensation is observable and retryable.
    {
        TrialGuard trial;
        bool restoreCanSucceed = false;
        int n = 1;
        assert(trial.Arm(t0, 120s));
        assert(trial.Apply(t0, {"partial", [&] { n = 9; return true; },
                                       [&] {
                                           if (!restoreCanSucceed) return false;
                                           n = 1; return true;
                                       }}));
        assert(!trial.Revert());
        assert(trial.state() == TrialGuard::State::RollbackFailed);
        assert(trial.pendingUndoCount() == 1);
        restoreCanSucceed = true;
        assert(trial.Revert());
        assert(n == 1);
    }

    // Expiry is checked even when a new apply is attempted late.
    {
        TrialGuard trial;
        bool changed = false;
        assert(trial.Arm(t0, 120s));
        assert(!trial.Apply(t0 + 121s,
                            {"late", [&] { changed = true; return true; },
                                     [&] { changed = false; return true; }}));
        assert(!changed);
    }

    std::cout << "Stage-0 trial rollback contract tests passed.\n";
}
