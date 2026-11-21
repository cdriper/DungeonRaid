# Dungeon Raid — rules for AI assistants

Read `README.md` first (layout, build). The C++ core in `core/` holds the game logic; platform hosts
(`platform/android/`, later `platform/windows/`) stay thin.

## MANDATORY: tool installations and system changes
1. **Ask the user first and get explicit approval** before installing any tool or package (pip, npm, apt, SDKs, plugins...), or changing the system in any way (env vars, PATH, registry, services, global configs, system settings). This applies to the user's PC and to any shared environment. **Without the user's approval it is strictly prohibited.** No exceptions for "small" or "temporary" installs.
2. **Record every approved installation or change in `docs/tools.md`**: which tool (and version), approximate size, install location, and how to uninstall or revert it. Update the entry when something is removed.

## MANDATORY: file changes
- **Don't create, modify, move or delete any file in the project unless the user explicitly asks for that change.** If a change seems useful but wasn't requested (including fixing docs that look outdated), propose it and wait for the user to say yes.

## MANDATORY: git
- **Never commit, push, amend, rebase, tag or create branches.** The user handles all git operations. Leave changes uncommitted in the working tree.

## Other project rules
- Game state must stay bit-exact with the original: run the golden replay test (`ctest`, see `README.md`) after any change to `core/`. Floating-point contraction stays off on every compiler.
- Keep the `0x...` address comments in `core/`: they map the code to the original library.
- Keep paths ≤ 7 folder levels below the repo root. Kotlin sources go flat in `platform/android/app/src/main/kotlin/`.
