## Summary

<!-- What does this change do, and why? Link the task/issue if one exists. -->

## Checklist

**Local checks** (mirrors `.github/workflows/build.yaml` — run before pushing, per `docs/architecture.md` §11.8)

- [ ] `python tools/ci/check_all.py` passes (size caps, layer rules, content pipeline, no dead dirs)
- [ ] `clang-format --dry-run --Werror` is clean on every changed `.h`/`.cpp`
- [ ] `cmake --build --preset release` compiles clean on Windows
- [ ] `ctest --preset release --output-on-failure` passes, including a new test for this change if it adds systems/logic

**Architecture conformance** (see `docs/architecture.md` for the full laws — check what's relevant)

- [ ] New per-entity state is a POD component in `shared/components/` (no methods, no owning pointers, no `entt::entity` stored persistently) — Law 3/4
- [ ] New simulation logic lives in `modes/space/systems/` as free functions taking `SystemContext`, registered in `SystemSchedule.cpp` in this commit — §11.3
- [ ] New composite objects (ships/stations/rigs) are built by a factory, not constructed inline in a system — Law 5
- [ ] New/changed content (stats, weapons, ships, modules) went into `data/base_game/*.json`, not a C++ definition — Law 10
- [ ] No layer boundary crossed the wrong way (`shared/` has no raylib/`core/`/`modes/`; `core/` has no raylib/`engine/`/`modes/`) — §2.3
- [ ] UI changes push Intents rather than mutating game state directly — Law 9
- [ ] No new file/function/mode-class exceeds the size caps (600 lines / 80 lines / 25 members) — §2.2

## Testing

<!-- What did you actually run, and what did you observe? UI/feature changes should be exercised in a real build, not just verified by tests. -->

## Notes for reviewers

<!-- Anything intentionally deferred, known gaps, or design tradeoffs worth flagging. -->
