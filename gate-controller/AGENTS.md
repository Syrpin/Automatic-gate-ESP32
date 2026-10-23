## Agent registry & coordination protocol (all AI agents MUST follow)

### Known contributors
- **qwen-coder**: build/CMake/ESP-IDF compatibility and `AGENTS.md` coordination (scope as reported by the repository owner; agent confirmation requested). Older notes also describe sim/webui HIL and CT/region work; update the scope only after the agent confirms ownership in the coordination thread.
- **codex-agent**: build-fix and documentation/handoff contributions (scope as reported in prior repository history; current GitHub account and scope confirmation requested).
- **gemini-cli**: participant named in the repository owner's coordination report; GitHub account, scope, and active participation are not yet independently confirmed.
- **copilot-sdk-review**: static runtime/safety review and handoff contributions.

The registry records contributors, not proof that an agent is online or has accepted a task. Never report an agent's acknowledgement, agreement, test, or push unless it is visible in GitHub or independently verified.

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

### Mandatory discussion and acknowledgement workflow

This workflow is intended to make cross-agent coordination observable. A mention or a commit is not an acknowledgement; the agent must reply in the shared GitHub discussion thread. Use the currently open coordination PR for this protocol. PR #1 is closed and must not be treated as an active thread; link the new coordination PR here after it is created. If the coordination PR is closed, use a single clearly titled GitHub issue as the replacement and update this section.

#### 1. Acknowledge before work
Before editing shared or safety-critical files, each participating agent MUST post a short reply in the coordination thread containing all of the following:

```text
ACK — <agent name> (<GitHub login)
I read gate-controller/AGENTS.md at <full commit SHA>.
Decision: ACCEPT / DISAGREE / NEEDS OWNER DECISION
Assigned item(s): <review finding IDs or none>
Owner: <agent name> | Branch: <branch name>
Files: <exact paths>
Validation: <specific build/tests/HIL plan>
Conflicts/dependencies: <none, or exact details>
```

If the agent cannot accept the proposed scope or decision, it must say so and explain why. Do not infer agreement from silence, a prior contribution, a GitHub mention, or a user report. If an agent does not respond, mark its status **awaiting acknowledgement**; do not assign it work or claim cross-agent agreement. The repository owner resolves scope and architecture disagreements before implementation.

#### 2. One owner per work package
The coordinating owner lists findings as numbered work packages and assigns exactly one implementation owner to each. An agent may claim a package only after posting the acknowledgement above. Other agents may review or advise, but must not edit the same files concurrently without an explicit handoff. Before starting, check recent path history and current branches/PRs. If ownership overlaps or is unclear, stop and ask in the shared thread; do not "race" another agent.

A task claim must name: finding ID, owner, branch, files, dependencies, acceptance criteria, and validation. A claim is not a task completion. Any transfer of ownership must be stated by both outgoing and incoming owners in the thread.

#### 3. Shared channel and decision record
Use the active coordination PR conversation for acknowledgements, assignment, questions, disagreements, and status. Use a focused implementation PR for code review and its linked issue for larger discussions. Do not create duplicate handoff documents or rely on chat-local messages that other contributors cannot see. Keep each decision in one canonical place; update this file only for durable protocol, registry, or architecture decisions. Link the exact issue/PR/commit rather than using short SHAs alone.

Sensor decision is explicit and owner-confirmed: **ZMCT103C is the primary current sensor; HALL is a selectable fallback.** Do not change the default or describe HALL as primary without an explicit decision from the repository owner recorded here first. An agent may propose a change in the discussion, with technical rationale and hardware evidence, but must not implement it before approval.

#### 4. Branch, push, and synchronization rules
Create a dedicated branch from the verified latest `main`; never push routine work directly to `main`. Keep commits focused. Open a PR against `main`, link the coordination/decision thread, and request review from the actual GitHub accounts confirmed by the agents themselves. Do not guess account identities from display names.

After pushing, the owner posts the full 40-character commit SHA, PR link, changed paths, and validation evidence in the PR. A second participant verifies the branch tip from GitHub before anyone calls it synchronized. Use full SHAs in handoffs; short SHAs are not enough to resolve a disputed state. If a reported SHA cannot be fetched or does not match the branch tip, mark the state **unverified**, stop dependent changes, and ask the author to reconcile it. Never report a push, merge, closure, or build based only on a chat claim.

#### 5. Review, verification, and completion
Reviewers reply with `APPROVE`, `REQUEST CHANGES`, or `COMMENT` and actionable evidence. The implementation owner addresses each item or explains a disagreement; do not silently dismiss review findings. Before declaring a package done, report exact commands, target/profile, result, binary size where relevant, and any tests not run. For hardware-dependent behavior, clearly separate compile/simulator success from bench/HIL proof. Never claim safety validation from a successful build alone.

A work package is complete only when its changes are pushed, its full SHA is verified, required build/tests pass, review feedback is addressed, and the coordinating owner records the outcome in the thread. Overall coordination is complete only when every participating agent has explicitly acknowledged, every finding has an owner or an explicit deferral, and unresolved disagreements are listed. Absence of a reply must remain visible as pending; do not convert it into consensus.

### Handoff notes (append-only, newest first)
- 2026-10-23 copilot-sdk-review: drafted mandatory cross-agent acknowledgement and single-thread workflow on branch `agents/coordination-protocol` from verified visible `main` `64afe261850d9dc3a58b296e8b524cf58de1fe7a`. No source code, sensor default, or architecture decision changed. Agent scopes and logins remain provisional until each agent confirms in the shared GitHub thread.
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
  no content changes). Root then held: AGENTS.md, ESP32_PORT_PLAN.md, easyeda/,
  legacy-stm32-f103/, gate-controller/. Other agents: update any scripts/docs
  that assume root-level build paths.
- 2026-10-23 qwen-coder: AGENTS.md MOVED into the active project folder —
  it now lives at `gate-controller/AGENTS.md` (repo root no longer contains
  it). All paths in this file are relative to `gate-controller/`. Build
  contract unchanged: `cd gate-controller && idf.py ...`.
