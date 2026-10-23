## Agent registry & coordination protocol (all AI agents MUST follow)

### Known contributors
- **qwen-coder** (this agent): build-fix audit, ESP-IDF 6.0 compat, sim/webui HIL, CT/region math. Commits tagged `Co-authored-by: qwen-coder`.
- **codex-agent**: authored changelog "Исправления сборки" (ESP-IDF 6.0 profiles, CMake deps, ADC pointer API). Verified full build: gate_controller.bin 817136 B.
- **copilot-sdk-review**: static runtime/safety review and cross-agent handoff; commits tagged `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`.

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
- 2026-10-23 copilot-sdk-review: reviewed current `main` at 9796325; no code changed. Static review blockers to coordinate before real-hardware use: (1) current-sense code assumes ~10 kS/s while ctrl_task calls it every 10 ms (~100 samples/s), invalidating the claimed mains-synchronous RMS timing and current-based protection; (2) encoder direction check can use a stale pre-move timestamp and fault before the new move produces pulses; (3) no-current watchdog can read initial/stale zero RMS before a valid sample window; (4) opposite-direction commands ignore motor_drv_set() failure and need stop/dead-time/restart; (5) homing lacks bounded timeout/fault exit; (6) zero-cross ISR/triac firing and custom-board PCNT remain explicit skeletons; (7) lowering throttle for end-of-travel slowdown does not reduce `applied` in MDRV_RUNNING. Request qwen-coder and codex-agent to acknowledge this handoff, identify an owner/branch for each item, and avoid overlapping edits; prioritize validated current sampling/protection, then motion-state transitions, then hardware-specific ZC/triac/PCNT implementation and HIL tests. Review evidence was gathered from `main/main.c`, `components/current_sense/current_sense.c`, `components/gate_logic/gate_logic.c`, `components/motor_drv/motor_drv.c`, and `components/encoder/encoder.c`. This is static analysis only; no bench/HIL validation was performed.
- 2026-10-23 qwen-coder: audited codex changelog vs repo; found fixes uncommitted; committed missing items as 17ce09f (`build:` + Co-authored-by). Remote f202348..HEAD now contains all changelog items. Pending: final `idf.py build` verification by whoever has IDF toolchain.

## Handoff notes (append-only)

- 2026-10-23 qwen-coder: repo layout restructured per owner request — active
  project is `gate-controller/` (build from inside it). STM32F103 legacy
  sources moved to `legacy-stm32-f103/` (read-only reference, pure git mv,
  no content changes). Root then held: AGENTS.md, ESP32_PORT_PLAN.md, easyeda/,
  legacy-stm32-f103/, gate-controller/. Other agents: update any scripts/docs
  that assume root-level build paths.
- 2026-10-23 qwen-coder: AGENTS.md MOVED into the active project folder —
  it now lives at `gate-controller/AGENTS.md` (repo root no longer contains
  it). All paths in this file are relative to `gate-controller/`. Build
  contract unchanged: `cd gate-controller && idf.py ...`.
