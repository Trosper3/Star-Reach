## Workflow: implementing a GitHub issue

Follow these steps in order for every issue worked from this repo's GitHub tracker (`Trosper3/Star-Reach`).

1. **Fresh base.** `git checkout main && git pull && git checkout -b <type>/<slug>`
   - Do this before *every* issue, even the next one in the same session right after finishing the last — never keep building on a branch from a prior issue.
   - Branch prefix matches the issue's label: `feature/`, `docs/`, `fix/`, `chore/`.

2. **Confirm dependencies are actually merged.** Many issues carry an explicit `Depends on: #NN` line asking to confirm `#NN` has merged to `main` before starting. Verify with `git log main` / `gh pr list --search "is:merged"` rather than assuming a closed issue means merged.

3. **Implement** against the issue body as the spec — its `Home:`, `Types:`, `Systems:`, and `Tests:` bullets are the actual scope, not just description. Flag anything the issue marks `❓ Open` rather than guessing a resolution.

4. **Build and test, don't drive.** Confirm a clean compile and a passing test suite in one local tree (`out/build/x64-Debug`) before handing off:
   ```
   cmake --build out/build/x64-Debug --target StarReach
   cmake --build out/build/x64-Debug --target sr_tests
   out/build/x64-Debug/bin/sr_tests.exe
   ```
   The last two steps are architecture.md section 11.8's own recipe (`ctest --preset release`
   there is the equivalent of running `sr_tests.exe` directly here) — a task isn't done on a clean
   compile alone if it touched anything sr_tests exercises. Don't also build a second local Windows
   tree "for safety" — CI's `build.yaml` matrix
   (`windows-latest`/`ubuntu-latest`/`macos-latest`) already runs one build per platform on every
   PR, including a second Windows generator's worth of verification. A second local Windows tree
   duplicates that at zero added coverage; it isn't Mac or Linux, it's the same compiler twice.

   Use the PowerShell tool (not Bash) for the vcvars64 + cmake chain — `cmd.exe` invoked via Bash
   silently no-ops in this environment. Sources are explicit lists in `CMakeLists.txt` /
   `tests/CMakeLists.txt`, **not** `file(GLOB_RECURSE ...)` — adding a new `.cpp` means editing
   those lists *and* reconfiguring, not just rebuilding.

   **Local speedup, not a repo change:** once a tree's packages are installed, reconfigure it once
   with `-DVCPKG_MANIFEST_INSTALL=OFF` (e.g. `cmake out/build/x64-Debug -DVCPKG_MANIFEST_INSTALL=OFF`)
   to skip vcpkg's ~8s re-verification on every subsequent reconfigure — which a `CMakeLists.txt`
   edit triggers on essentially every issue, since new sources land there. This is a per-build-dir
   CMake cache setting; it never touches a checked-in file and does not affect CI, which always
   starts from a clean checkout and needs the install step to run. If `vcpkg.json` itself changes,
   reconfigure once with `-DVCPKG_MANIFEST_INSTALL=ON` (or run `vcpkg install` by hand) to pick it
   up — otherwise CMake just fails to find the new package, loudly, rather than silently going
   stale.

   Do not launch `StarReach.exe` or drive the GUI (mouse/keyboard injection, screenshots) to verify features — the user verifies in-game themselves.

5. **Hand off, don't commit.** Report what changed and the exact runnable exe path(s) built. Do not commit or push unless explicitly asked — the user reviews, commits, and pushes themselves. The user also opens the PR; on request, provide a short summary of the work for the PR description (tied to the issue's Feature Description/Tests sections, "Closes #NN") but do not open the PR.
