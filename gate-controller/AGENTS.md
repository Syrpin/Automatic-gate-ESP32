
## Agent registry & coordination protocol (all AI agents MUST follow)

### Known contributors
- **qwen-coder** (this agent): build-fix audit, ESP-IDF 6.0 compat, sim/webui HIL, CT/region math. Commits tagged `Co-authored-by: qwen-coder`.
- **codex-agent**: authored changelog "Исправления сборки" (ESP-IDF 6.0 profiles, CMake deps, ADC pointer API). Verified full build: gate_controller.bin 817136 B.
- (add new agents below as they join — name, scope, commit tag)

### Rules of engagement
1. **Fetch before edit.** `git fetch origin main` at session start; never edit from a stale base. Local `main` must contain remote HEAD before committing.
2. **Small commits, one concern each**, message prefixed with area: `build:`, `feat(<component>):`, `docs:`, `fix(<component>):`. Always add `Co-authored-by: <agent-name>` trailer.
3. **Push immediately after commit** (no long-lived local-only work — that is how changes get lost between agents). If push fails for auth, state it loudly in the response and keep changes committed locally.
4. **Before touching a file another agent owns** (see scopes above), check its recent history: `git log --oneline -5 -- <path>`. If unsure, coordinate via this file: append a line under "Handoff notes".
5. **Never revert or restructure others' commits** without noting it in Handoff notes with reason.
6. **Secrets policy:** no tokens/cookies/PAT in any commit, file or log. `.gitignore` enforces; verify before commit (`git diff --cached | grep -iE 'pat_|token|cookie|session='`).
7. **Build contract:** project must stay compilable with
   `idf.py -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.sim.defaults" reconfigure build` (ESP-IDF 6.0). Run it after any change to CMakeLists/manifests/Kconfig; report bin size delta in commit body.
8. **Decisions log:** architecture decisions live in ESP32_PORT_PLAN.md / SCHEMATIC.md sections; code must not silently contradict them (e.g., ZMCT103C is PRIMARY CT, battery telemetry descoped — hold-up is health-check only).

### Handoff notes (append-only, newest first)
- 2026-10-23 qwen-coder: audited codex changelog vs repo; found fixes uncommitted; committed missing items as 17ce09f (`build:` + Co-authored-by). Remote f202348..HEAD now contains all changelog items. Pending: final `idf.py build` verification by whoever has IDF toolchain.

## Handoff notes (append-only)

- 2026-10-23 qwen-coder: repo layout restructured per owner request — active
  project is `gate-controller/` (build from inside it). STM32F103 legacy
  sources moved to `legacy-stm32-f103/` (read-only reference, pure git mv,
  no content changes). Root then held: AGENTS.md, ESP32_PORT_PLAN.md,
  easyeda/, legacy-stm32-f103/, gate-controller/. Other agents: update any
  scripts/docs that assume root-level build paths.
- 2026-10-23 qwen-coder: AGENTS.md MOVED into the active project folder —
  it now lives at `gate-controller/AGENTS.md` (repo root no longer contains
  it). All paths in this file are relative to `gate-controller/`. Build
  contract unchanged: `cd gate-controller && idf.py ...`.
