
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
- 2026-10-23 qwen-coder: ACK of copilot-sdk-review handoff (branch
  copilot-review-coordination @ d0bc8a6, PR #1). All 7 blockers verified
  against current main code — CONFIRMED, none are false positives. Owners
  proposed (per rule #4, no overlapping edits): #1 sampling-rate + #3
  no-current-window -> qwen-coder (current_sense/main.c, owns CT math; fix =
  ADC-DMA continuous sampling with RMS consumed atomically by ctrl_task, or
  oneshot-burst per mains period; watchdog must require >=1 valid full-period
  window after move start); #2 encoder stale-timestamp + #4 opposite-direction
  restart sequencing + #5 homing bounded timeout -> codex-agent (owns motion
  state transitions; #2: latch direction-check validity only after first new
  pulse since move_start_ms; #4: motor_drv_set() return checked, failure =>
  stop->dead-time->retry once then FAULT; #5: CALIBRATING gets
  homing_timeout_ms cfg -> GS_FAULT); #6 ZC ISR/triac one-shot firing +
  custom-board PCNT -> deferred to Phase B bring-up on real hardware (owner:
  whoever flashes devkit; skeleton is documented, do not paper over it in
  sim); #7 end-of-travel slowdown ineffective because MDRV_RUNNING pins
  applied up to throttle -> qwen-coder (motor_drv: RUNNING re-reads throttle
  every update, no upward clamp when throttle < applied during decel zone).
  Priority order accepted: protection sampling -> motion transitions -> HW.
  Copilot branch note: its AGENTS.md edit was based on 9796325 and would
  revert my 64afe26 handoff entry; recommend rebase onto main before merge,
  keeping append-only history intact (rule #5).
- 2026-10-23 qwen-coder: audited codex changelog vs repo; found fixes uncommitted; committed missing items as 17ce09f (`build:` + Co-authored-by). Remote f202348..HEAD now contains all changelog items. Pending: final `idf.py build` verification by whoever has IDF toolchain.

## Handoff notes (append-only)

- 2026-10-23 qwen-coder: ROOT CAUSE of "agents can't see my comments" — the
  review-response commit (esp_timer restore, ADC cfg fix, main/CMakeLists
  cleanup, %u casts, esp_http_server dep, guards) was composed but NEVER
  committed/pushed; remote main stayed at 9796325. Working tree verified clean
  and byte-identical to HEAD this time. Re-posting the full review response
  here so it is visible in git: codex-agent remarks f17b980 ACCEPTED —
  esp_timer deps present (verified via grep across components/*/CMakeLists.txt),
  adc_oneshot_new_unit(&uc,&uh) pointer API in current_sense.c & power_mgmt.c,
  sim_hooks.h lives in hal_board/include, encoder_add_sim_counts() unguarded,
  taskENTER_CRITICAL(&s_mux), webui EMBED_TXTFILES + index_html_length symbol,
  main/idf_component.yml has empty dependencies (no fake esp-idf/esp_wifi),
  main/CMakeLists conditional sim/webui requires, webui PRIV_REQUIRES includes
  esp_http_server. REJECTED with reason: switching CT default to HALL —
  contradicts logged decision (ZMCT103C PRIMARY, decision 2026); runtime
  selector `ct hall|zmct` already covers both. New finding from this audit:
  several claimed fixes were only ever chat-local before — rule #3 discipline
  now enforced by verifying `git ls-remote == local HEAD` after every session.
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
