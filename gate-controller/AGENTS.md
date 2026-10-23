
# gate-controller — Agent registry & coordination protocol (all AI agents MUST follow)

### Known contributors
The registry records contributors, not proof that an agent is online or has accepted a task. Never report an agent's acknowledgement, agreement, test, or push unless it is visible in GitHub or independently verified.
- **qwen-coder** (this agent): build-fix audit, ESP-IDF 6.0 compat, sim/webui HIL, CT/region math. Scope CONFIRMED by the agent in Handoff notes (2026-10-23). Commits tagged `Co-authored-by: qwen-coder`.
- **codex-agent**: authored changelog "Исправления сборки" (ESP-IDF 6.0 profiles, CMake deps, ADC pointer API). Verified full build: gate_controller.bin 817136 B. Proposed ownership of findings #2/#4/#5 pending explicit ACK in the coordination thread.
- **gemini-cli**: participant named in the repository owner's coordination report; GitHub account, scope, and active participation are not yet independently confirmed.
- **copilot-sdk-review**: static runtime/safety review and cross-agent handoff (branch `copilot-review-coordination`, PR #1; coordination draft on branch `agents/coordination-protocol`). Commits tagged `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`.

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

### Discussion & acknowledgement workflow (adopted from `agents/coordination-protocol` @ 2e2bd62, merged into this canonical file)

**Single channel.** All cross-agent acknowledgements, assignments, disagreements and status updates go to ONE visible GitHub thread. PR #1 (`copilot-review-coordination` @ d0bc8a6) is CLOSED — do not treat it as active. Active coordination happens via Handoff notes in this file + a dedicated coordination issue/PR opened by the owner or copilot-sdk-review; link its exact URL here when created. A mention or a commit is NOT an acknowledgement; only an explicit reply counts. If an agent does not respond, mark it **awaiting acknowledgement** — never infer consensus from silence.

**ACK format** (post before editing shared/safety-critical files):
```text
ACK — <agent name> (<GitHub login>)
I read gate-controller/AGENTS.md at <full 40-char SHA>.
Decision: ACCEPT / DISAGREE / NEEDS OWNER DECISION
Assigned item(s): <finding IDs or none> | Owner: <agent> | Branch: <branch>
Files: <exact paths> | Validation: <build/tests/HIL plan>
Conflicts/dependencies: <none, or exact details>
```

**One owner per work package.** Current assignment state (proposed by qwen-coder in 63ff72d, awaiting ACKs): #1 sampling-rate + #3 no-current-window + #7 decel-applied → qwen-coder; #2 encoder stale-timestamp + #4 opposite-direction restart + #5 homing timeout → codex-agent (**awaiting acknowledgement**); #6 ZC ISR/triac/PCNT → deferred to Phase B HW bring-up. No overlapping edits without explicit handoff by BOTH outgoing and incoming owners.

**Branch discipline.** Routine work goes through a dedicated branch cut from verified latest `main` + PR; direct pushes to `main` are allowed only for append-only AGENTS.md handoff entries. After pushing, post the FULL 40-char SHA, changed paths and validation evidence; short SHAs alone are not sufficient to resolve a disputed state. Never report push/merge/build based only on a chat claim.

**Sensor decision (owner-confirmed):** ZMCT103C primary / HALL selectable fallback. Do not change the default without an explicit recorded owner decision.

### Handoff notes (append-only, newest first)
- 2026-10-23 qwen-coder: ACK in coordination thread. I read
  gate-controller/AGENTS.md at full SHA 63ff72d2738b804f23516bf1ab87743b5d19ecab
  and adopt the Discussion & acknowledgement workflow merged from branch
  agents/coordination-protocol @ 2e2bd62c (copilot-sdk-review draft). Decision:
  ACCEPT — single channel (this file + one coordination issue/PR; PR #1 is
  closed), explicit ACK required before shared-file edits, one owner per work
  package, full-SHA evidence, ZMCT103C primary / HALL fallback unchanged.
  My claimed packages: #1 (current_sense sampling-rate/RMS window), #3
  (no-current watchdog valid-window gate), #7 (motor_drv decel applied clamp);
  files: components/current_sense/*, main/main.c telemetry hook,
  components/motor_drv/motor_drv.c; validation plan: host fuzz stand for RMS
  window math + `idf.py -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.sim.defaults" reconfigure build` + sim REPL scenario (btn open short / enc ramp / ct fault injection). Conflicts: none — codex-agent and gemini-cli have not yet ACKed their proposed scopes (#2/#4/#5 codex-agent; gemini-cli status unknown), so those remain PROPOSED, not agreed. Registry dedup note: consolidated the duplicate "Handoff notes" header introduced by earlier append-style edits into this single section (rule #5 compliance: reason recorded here, no entries deleted). Awaiting: codex-agent ACK for #2/#4/#5; copilot-sdk-review to open the canonical coordination issue and post its URL; owner decision on whether direct pushes to main stay allowed for docs-only handoff entries.
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
