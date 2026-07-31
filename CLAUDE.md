## Workflow: implementing a GitHub issue

Follow these steps in order for every issue worked from this repo's GitHub tracker (`Trosper3/Star-Reach`).

1. **Fresh base.** `git checkout main && git pull && git checkout -b <type>/<slug>`
   - Do this before *every* issue, even the next one in the same session right after finishing the last — never keep building on a branch from a prior issue.
   - Branch prefix matches the issue's label: `feature/`, `docs/`, `fix/`, `chore/`.

2. **Confirm dependencies are actually merged.** Many issues carry an explicit `Depends on: #NN` line asking to confirm `#NN` has merged to `main` before starting. Verify with `git log main` / `gh pr list --search "is:merged"` rather than assuming a closed issue means merged.

3. **Implement** against the issue body as the spec — its `Home:`, `Types:`, `Systems:`, and `Tests:` bullets are the actual scope, not just description. Flag anything the issue marks `❓ Open` rather than guessing a resolution.

4. **Build, don't drive.** Confirm a clean compile in both build trees before handing off:
   ```
   cmake --build build --target StarReach
   cmake --build out/build/x64-Debug --target StarReach
   ```
   Use the PowerShell tool (not Bash) for the vcvars64 + cmake chain — `cmd.exe` invoked via Bash silently no-ops in this environment. Adding a new `.cpp` requires a CMake reconfigure of both trees, not just a rebuild (sources are `file(GLOB_RECURSE ...)`).

   Do not launch `StarReach.exe` or drive the GUI (mouse/keyboard injection, screenshots) to verify features — the user verifies in-game themselves.

5. **Hand off, don't commit.** Report what changed and the exact runnable exe path(s) built. Do not commit or push unless explicitly asked — the user reviews, commits, and pushes themselves. The user also opens the PR; on request, provide a short summary of the work for the PR description (tied to the issue's Feature Description/Tests sections, "Closes #NN") but do not open the PR.
