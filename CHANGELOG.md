# Changelog

## Unreleased

> Research-status clarification (2026-10-26): dated experiment entries below retain their original implementation plans and then-current hypotheses. [Current G1/G2 evidence](docs/independent-behavior-graph.md#current-status) supersedes stale “awaiting run”, complete-isolation and single-root-cause interpretations; original contracts/logs remain intact. This clarification changes documentation only.

> Posterior clarification (2026-10-27): the dated audit18–21 entries retain their original interpretations. Their unique-generation and manager-registration necessity claims are superseded by [audit22](docs/behavior-graph-construction-audit.md#audit22); audit21's scratch-shell behavior guard remains valid. [Audit23](docs/behavior-graph-construction-audit.md#audit23) adds complete chained-unwind ranges and separates poseLocal copies from outer scene application. These static corrections do not establish a runtime pass.

### Added

- Capture the source's own equip/unequip timeline in game (G3-A, v41): a 20-second F7 window produced 117 settled lines covering both transitions — `IsEquipping`/`IsUnequipping` pulses, `currentDefaultState` 0→10→1→12→0, `iState` 0→9→0, `iRightHandEquipped`/`iMagicEquipped`/`iLeftHandEquipped`, and node counts 36→51→81→66→33→82→36. The settled phase trajectories are not mirror images. They establish observations, while external writer roles, runtime types and event timing still need verification before a G3-B replay contract; `iState` and `currentDefaultState` remain observation-only. See the [run record](docs/independent-behavior-graph.md#g3-a-contract).

- Record the source graph's own timeline for G3-A (v37): once per post-UI frame, and only when `[Experiment] SourceTimeline` is on, the plugin diffs the graph's 652 word variables and the active-node count/hash, logging changed indices with their names and the int/float reading of the same four bytes. It reuses the existing render hook — no new hook, no native call, no write, no event dispatch, no candidate interaction — and stops after 200 lines per process. Contract and pre-registered readings: [G3-A](docs/independent-behavior-graph.md#g3-a-contract), build: [v37](docs/build.md#v37-source-timeline-2026-10-28).

- Probe the engine's behavior-graph registry count around the candidate load (v35), then withdraw it (v36): the 23:16 run printed `registry=300474432->300474432`, and re-reading the instructions showed that `[0x315C950] + 0x228` is a local parity value that is never written back while `63529` is a lock-free append rather than a registry insert. The earlier "the loader registers the graph and bumps a count" reading is retired; the item is left without a measurable anchor instead of being guessed at. See the [correction](docs/independent-behavior-graph.md#g1-teardown-audit).

- Record the teardown decision from the same audit: `hkbBehaviorGraph::Deactivate` dereferences `activeNodes` without a null check, early-returns when `context+8` is set, releases `variableValueSet` and clears `isActive`, and the engine only calls it from `BShkbAnimationGraph`'s destructor. Because the candidate shares `rootGenerator` with the source graph and the active-node entries are not proven to be clones, destroying the retained candidate is a documented **NO-GO** — corrupting the source graph would follow the save, which is worse than a reproducible crash.

- Add the v21 pose-content decode for the v20a experiment (2026-10-27), the bounded follow-up to "the raw pose hash changed 120/120 times": at steps 0/60/120 the drive now reads every `character.poseLocal` entry through the declared `hkQsTransform` (0x30 stride, capped at 256), checks finiteness over translation xyz / rotation xyzw / scale xyz (the fourth lane is padding and excluded), compares each entry component-wise with the previous checkpoint at a 1e-6 threshold (a NaN operand yields an infinite delta so it can never read as unchanged), does the same for the source's own `poseLocal`, and counts candidate/source bit-identical entries as an aliasing watch. Four index-spread bones (`0 / ⅓ / ⅔ / last`, i.e. 0/38/76/115 for the observed 116-bone skeleton) get named rows via a bounded, plausibility-checked name read. The pure arithmetic lives in a new [pose-content policy](src/animation/behavior_graph/pose_content_policy.h) with one 20-assertion engine-free case; no native call, write, REL id, recipe or previous budget changes, and the new rows (`pose-content` ≤3, `pose-bone` ≤12) are accounted separately from `GRAPH FRAME` 30+1, `INDEPENDENT GRAPH` 40 and `BEHAVIOR GRAPH` 36. The pre-registered verdict is staged: any non-finite entry stops the round (audit23's three poseLocal copy paths become the next question); `source-equal>0` stops as an aliasing suspicion; all-finite with `moved≥16` at 60 and 120 plus at least two of four sampled bones moving is "pose content valid" — which is still not a G2 pass — and `moved<16` would demote v20a's motion to padding/noise. Clean identity `CharacterPanel-scenegraph-behavior-graph-v21-pose-content-20261027-fe7c4d15e1-cl94faaed0c6-20261027T141004Z` — DLL `e0880651…` (1,267,200 bytes), PDB `787b3005…`, CodeView age 322 — packaged as `CharacterPanel-behavior-graph-v21-pose-content-20261027.zip` (9,928,507 bytes, SHA-256 `4cfa5ac8…`, manifest `e2873121…`) and deployed after backing the v20a trio up to `dist/deploy-backup-20261027-v20a/`; the deployed INI is `inject` and the repository sample stays `off`. The first compile failed on cl C4267 (`hkArray::operator[]` takes `size_type`) and was fixed by indexing the declared `data()` array; the rebuild and all four engine-free cases pass. No game run yet, so the pose verdict is pending and G2 remains NOT PASSED. See the [contract](docs/independent-behavior-graph.md#g2-v21-contract) and the [build record](docs/build.md#v21-pose-content-2026-10-27).

- Record the first v20a runtime result (2026-10-27 22:02) — the experiment reached 63589 and the candidate pose moved for the first time. The loaded identity matched the package, the session produced `GRAPH ENTRY` 1, `GRAPH FRAME` 30+1, `INDEPENDENT GRAPH` 40 and `BEHAVIOR GRAPH` 36 rows (three row-identical source blocks), with no `[E]`, no crash and only the 15 pre-existing DAC/studio-light warnings; panel open 22:02:36.590, experiment 22:02:36.597–36.600, close 22:02:42.216 with `DAC restore … result=matched`. The guard was `ready` and the corrected g3-gates chain resolved on the candidate (`character=0x1dab1c853c0 setup=0x1d572e9c9c0 data=0x1d5371d26f0 footik-info=0x1d5371d5b70`). New runtime facts, none seen in v7–v19: the first returned 63589 already fills the read slot (`numData=116 onFraction=1.0` at step 0 post-sample, both slots from step 2, `tracks-end[+32=0x0074 +38=1.000]`); the t pair arms at step 1 (`t0=-0.0166667 t1=0.175`) and `w245=0x0 b246=0` (Path B) holds from step 2, with a later re-arm (step 119 `t0=0.05 t1=-0.2`) proving the v19 contract's "arming window is only step 0" was an artefact of the v17 recipe; `gen0elem` moves (`cf37f9140cd1eb83`→`cbd25238b4fbce52`) and native `cand-anim` becomes `0x0074`; and the candidate raw pose changes on 120/120 returned calls (`bc160a8c2b3a5a58`→`63f638a6ac08d955` at 60→`8fb4cd336c4395bf` at 120, `arm-observed=yes`, `raw-hash-changes=120 first-changed-completed=1`) while the source pose stays `983fc2d4a169fa5b` before and after with `source-drift=none` and `source-stable=true retained=true`. This answers the audit21/22 runtime question: the v17–v19 null result came from the entry choice, because the direct 63581 call skips 63589's outer generation, header back-fill and arm/apply. **G2 remains NOT PASSED**: the evidence is still a padding-sensitive raw-buffer hash, an element-head hash and header counts/fractions, with no per-bone or finite-value proof and no visual confirmation (`acceptance=unjudged`), and loaded teardown, global convergence, G3–G6 and OAR parity stay open. The log is archived verbatim ([40 KB](docs/diagnostics/CharacterPanel-g2v20a-entry-20261027-2202.log), 43,330 bytes, SHA-256 `E05BFEAF…`), the deployed INI is back to `IndependentGraph=off`, and the next bounded step is a read-only pose-content decode (named bones: finite translation/quaternion, change at 60/120, source comparison) without new native calls. See the [run record](docs/independent-behavior-graph.md#g2-v20a-run).

- Fix the v20 pre-entry crash and rebuild as v20a (2026-10-27). The v16-era `g3-gates` row started its chain at `[ctx+0x50]` although `sizeof(RE::hkbContext)` is exactly 0x50, so the row had been reading one slot past the context object since v16 and only faulted once v20's stack layout put a stale pointer there. The row now uses audit23's real chain through declared members (`hkbCharacter::setup` → `hkbCharacterSetup::data` → `hkbCharacterData::footIkDriverInfo`), types `[graph+0x100]` as `hkbFootIkDriver` (`disableFootIk`), and filters every intermediate with the new engine-free [observation-chain policy](src/animation/behavior_graph/observation_chain_policy.h); the reported fields become `character/setup/data/footik-info`. Nothing else changes: the single behavior variable (63589(&mode0) under inject plus exact AE 1.6.1170), the v19 eye point, the step/flags/world/event/retention recipes, the guard, the budgets (ENTRY ≤1, FRAME 30+1) and the pre-registered adjudication table all stay. Three engine-free cases pass under `g++` 15.1 with `-Wall -Wextra -Werror` (new policy 11 rows, guard 22 rows, frame-trace 3). Clean identity `CharacterPanel-scenegraph-behavior-graph-v20a-chain-fix-20261027-7d18a90787-cl94faaed0c6-20261027T135614Z` — DLL `691f7592…` (1,254,400 bytes), PDB `5b3d8cc8…`, CodeView age 321 — was packaged as `CharacterPanel-behavior-graph-v20a-chain-fix-20261027.zip` (9,747,510 bytes, SHA-256 `f4586e16…`, five entries re-hashed against its manifest) and deployed after backing the v20 trio up to `dist/deploy-backup-20261027-v20/`; the deployed INI is `inject` (1,073 bytes `6e9d2005…`) and the repository sample stays `off`. No runtime evidence follows: at build time 63589 still had never executed, the corrected row only proves the four declared hops are readable, and G2 had not passed. The run recorded above then reached the entry and moved the pose; that is the newer state. See the [build record](docs/build.md#v20a-chain-fix-2026-10-27) and the [run record](docs/independent-behavior-graph.md#g2-v20a-run).

- Record the first v20 runtime attempt (2026-10-27 21:48) and its pre-entry crash. The deployed `CharacterPanel-scenegraph-behavior-graph-v20-generation-entry-20261027-a83e5229e7-cl94faaed0c6-20261027T134152Z` loaded on exact AE 1.6.1170 with `SCENEGRAPH BUILD` matching the package manifest, the single private-state guard passed (`GRAPH ENTRY … outcome=ready reason=ready entry=63589 mode=0`), and the thread then faulted with `EXCEPTION_ACCESS_VIOLATION` at `CharacterPanel.dll+0x28067`, instruction `mov r8, [rbx+0x88]`, `independent_graph.cpp:955` — the v16-era read-only g3-gates diagnostic whose chain starts at `[ctx+0x50]` although `sizeof(RE::hkbContext)==0x50`, i.e. one past the context buffer. Earlier runs read zero in that stack slot and short-circuited (`ctx+50=null +40=null +88=null` in the v16/v18/v19 logs); this run read a stale non-null value whose `[+0x40]` produced a non-canonical pointer, so `[+0x88]` faulted. Because that read precedes the drive loop, **63589 was never called**: no `GRAPH FRAME`, no `completed`, no pose evidence, and the run says nothing about the native entry. Per pre-registration the session stopped: the log and crash report are archived verbatim (`2278201C…` 16,920 bytes and `4074DF23…` 119,326 bytes), the deployed INI is back to `IndependentGraph=off`, and no retry or parameter change happened. [Audit24](docs/behavior-graph-construction-audit.md#audit24) aligns this with audit23's chain-start correction (`[context]`→`+0x50`→`+0x40`→`+0x88`); the next bounded step is to correct or retire that diagnostic, re-check sibling `context+offset` reads, and retest under a new identity. G2 has not passed.

- Rebuild, repackage and redeploy the v20 guarded native-entry package from committed HEAD `a83e522` (2026-10-27) because the frozen artifacts recorded earlier are absent from this checkout: `build/behavior-graph-v20-generation-entry-20261027/` did not exist, `build/Release` held the v19 pair, and no `*v20-generation-entry*` file existed on this machine. No source change: the engine-free guard case was compiled and re-run (22 value rows pass under `g++` 15.1 with `-Wall -Wextra -Werror`), and Release configure/build produced the clean identity `CharacterPanel-scenegraph-behavior-graph-v20-generation-entry-20261027-a83e5229e7-cl94faaed0c6-20261027T134152Z` — DLL SHA-256 `13688a9d…` (1,253,888 bytes), PDB `07a509da…` (35,467,264 bytes), CodeView GUID `{7F586C2E-…}` age 320 — with only the existing install-rule and D9025 warnings. The isolated package (`build/behavior-graph-v20-generation-entry-20261027/test-package/`, manifest `a9ae936d…`) and its ZIP (9,750,395 bytes, SHA-256 `076cf6f7…`, five forward-slash entries re-extracted and re-hashed against the manifest) are new; the earlier `242d0a5+dirty` identity and its hashes remain history, and byte identity with that uncommitted-tree build is not claimed. Deployment backed the v19 trio up to `dist/deploy-backup-20261026-v19/` (DLL `344d9abb…`, PDB `9848cd14…`, `off` INI 1,034 bytes), placed this DLL/PDB in the MO2 mod, and set the deployed INI to `IndependentGraph=inject` (1,055 bytes) while the repository sample stays `off`. That entry was written before the run: no game launch happened during the rebuild task, and the guard/pose outcome was still unobserved at that point. The first attempt followed (see the entry above), so the current state is a pre-entry crash rather than an unrun test. See the [build record](docs/build.md#v20-rebuild-2026-10-27) and the [handoff](docs/independent-behavior-graph.md#v20-handoff).

- Add the guarded v20 native-entry experiment for exact AE 1.6.1170 inject: restore the audited synchronous 63589 mode0 wrapper while preserving all other v19 inputs and source controls. Document audit22's corrected metadata-visitor/Generate attribution and audit23's generation, poseLocal-copy and scene-application boundaries. The frozen [test handoff](docs/independent-behavior-graph.md#v20-handoff) awaits its first runtime result; no manager registration, G2 pass or loaded-graph teardown success is claimed.

- Add the twenty-first 1.6.1170 binary audit (2026-10-27), which settles why the recipe never generated: 63602's whole generation block is guarded by the first argument's `behaviorGraph` (`0xbb62ae: cmp qword [r13+0x208],0; je 0xbb63bd`), and the block contains both the 65149 generation call and 63602's internal 63587 call — so on the unloaded scratch graph neither call happens at all (stronger than audit 18's "returns immediately"). `65177` (0xc263c0) is a name joiner: it builds `<[object+0x38]>/<[buffer+0x38]>` from tagged string pointers (low bit masked) into a stack hkStringPtr-like builder (`+0x108` data, `+0x110` size, `+0x112` capacity); `59268` (0xacf770) is the two-instruction leaf `movss [rcx+0xF0], xmm1; ret`. 63602's "buffer" is a **descriptor of twelve stack sub-object pointers** starting at `[rbp+0x140]` (each 8-byte slot filled with `&[rbp+...]`), so audit 20's reading of `+0x00/+0x08` as embedded fields holds only for the other sink class used by 0xba4e30 (whose first field is a BSFixedString assigned by 69166). Conclusion: there is no safe minimal generation experiment left — getting engine-generated pose for the candidate requires either putting the candidate's real behaviorGraph into 63602's first argument (unverified side effects on a real graph) or registering the candidate in a manager (breaking the G1 no-registration boundary). That is an architecture decision, not another observation; audit only, no game run, no recipe change. See [audit](docs/behavior-graph-construction-audit.md#audit21) and [next questions](docs/independent-behavior-graph.md#next-audit).

- Add the twentieth 1.6.1170 binary audit (2026-10-27), which decodes the sink adapter's slot-5 (`0xc25ee0`, the 65149 adapter) and the buffer it consumes. Slot-5 first calls `65177([ctx+8], buffer+0x38, &local_0x118)` to resolve a name/state, then builds a `BSFixedString` key (static empty when the local name is null) and looks it up in the hash at `[[adapter+8]+8]+0x28` (capacity `+0xC`, sentinel `+0x18`), reads the entry's float at `+0x8` and calls `59268(buffer, float)`; it then reads `buffer+0x88`. The buffer's known fields: `+0x00` a refcounted project-name string (assigned by `69166`, which is a refcounted string assign whose release goes through `Entry::release8` at 0xced9a0), `+0x08` a pointer to the name→float registry, `+0x38` the name/state sub-object read by 65177, `+0x88` another pointer; the total is at least 0xA0 bytes (63602's stack buffer at `[rbp+0x140]`). Consequence: fabricating a sink buffer is not acceptable — it would corrupt a refcounted string and violate unknown lifetimes — so the v20 "explicit generation" contract is parked. The next bounded choice is either audit21 (decode 65177/59268 and the registry source) or a read-only observation contract that samples a real engine sink without constructing or writing one. Audit only: no game run, no recipe change. See [audit](docs/behavior-graph-construction-audit.md#audit20) and [next questions](docs/independent-behavior-graph.md#next-audit).

- Add the nineteenth 1.6.1170 binary audit (2026-10-27), which pins the engine's real per-frame generation context. The chain is `0x53fd20` (outer, sole caller of the next) → `0xba4e30` (picks `graph = [mgr+0x48][index]` with `[mgr+0x50]` count and `[mgr+0xb0]` default index, calls 69166 with `graph+0x1F0` = projectName, then 63595) → `63595` (0xbb31b0; gate `[graph+0x200] != 0` = `ProjectDBData*`, then resolves the request's id array against the `projectDBData+0xc8` hash — capacity `+0xAC`, sentinel `+0xB8` — and only calls 63859 when the resolved count matches) → `63859` (0xbc32d0) → 65232 → 65237. The sink is a 0x10-byte adapter `{vtable, buffer}`: three known vtables (0x1992da8 built inside 65149/63602, 0x1989320 for 0xbc3270, 0x1989360 for 0xbc32d0), slots 1-4 are four `mov al,1; ret` predicates, slot0 is the deleting destructor and **slot5 is the output-forwarding virtual** (`0xc25ee0`/`0xbc51c0`/`0xbc72e0`). 63602's own sink buffer is its stack local at `[rbp+0x140]` (fields up to `[rbp+0x1e0]`/`[rbp+0x1f4]`, roughly 0xA0 bytes). Consequence: generation cannot be obtained by calling 63602 or 63587; it needs `(graph, request, sink)` or `(character, values, sink)` with a valid adapter/buffer, and the candidate's `ProjectDBData` gate already passes while `behaviorGraph.isActive`/`numStaticNodes` remain uncollected. The next bounded question is audit20 (the slot-5 buffer layout and the `0x53fd20` driver) before any explicit-generation contract. Audit only: no game run, no recipe change. See [audit](docs/behavior-graph-construction-audit.md#audit19) and [next questions](docs/independent-behavior-graph.md#next-audit).

- Add the eighteenth 1.6.1170 binary audit (2026-10-26/27), which corrects the long-standing attribution of the per-step pipeline: 63602 (0xbb4c50) binds `r13 = rcx` and performs all of its per-graph work — 65149 generation (`rcx = r13+0xC0`), 63587 update (`rcx = r13`), 63584 output/timer reset (`rcx = r13`) — on its **first** argument, while the second only receives vtable table writes (slots 0x30/0x38/0x40/0x48/0x50/0x60 with 0x28-stride table entries). The engine's own per-graph helper 0xbacc90 constructs a scratch graph (63555), calls 63602(scratch, target), then destroys it (63556) — exactly the recipe the plugin mirrors — so the first argument is an unloaded scratch graph whose `character.behaviorGraph` (`+0xC0+0x58`), `+0x110` and `+0x208` are null: 65149 returns at its first gate and 63587 at its prologue, so the pass generates nothing. The real generation entries are the character-level wrappers 0xbc3270/0xbc32d0, both gated on `character.behaviorGraph != 0` and calling 65232 (gate: `behaviorGraph.isActive (+0x12C)`; partition sizing from `numStaticNodes (+0x128)`) and then 65237; 0xbc3270 is vtable-only and 0xbc32d0 has a single caller in 0xbb31b0. 63584 (0xbb0710) resets `+0x1E8/+0x1EC` to `-MAXFLT` and `+0x230` to 0 on the same first argument. Consequence: v7–v19 never generated pose data for the candidate, so `generatorOutputs`'s `numData` staying 0 is the expected result, not a failing gate; the next bounded question is audit19 (the `0xbb31b0` caller and the sink/adapter layout at vtables 0x1989320/0x1989360). Audit only: no game run, no recipe change. See [audit](docs/behavior-graph-construction-audit.md#audit18) and [next questions](docs/independent-behavior-graph.md#next-audit).

- Add the seventeenth 1.6.1170 binary audit (2026-10-26), which resolves the v19 no-arm: the arm block's step gate is `63864` (0xbc3990, sole call site 0xbb2603), a minimum over `hkbBehaviorGraph::activeNodes` (`+0x98`) of a normalised per-node animation state value (default FLT_MAX, per-node inputs `[node+0x64]/[node+0x68]/[node+0x72]/[node+0x106]` and the `[node+0x50]` array via `0xacf340`); on the candidate the returned value is ≤0.04583, so `xmm6 = ret - 0.5*dt` can never clear the `1.25*dt` arm threshold. The candidate's `rootNode` (+0x218) is zeroed by the `BShkbAnimationGraph` constructor (0xbad239, `r15=0`) and no audited function re-fills it, so `63575`'s distance path and the arm block's distance branch are both unavailable; who writes `rootNode` on a native graph remains UNVERIFIED (the per-function `+0x218` store sweep is documented but desyncs on the constructor block). Conclusion: the eye point was necessary but not sufficient — the missing input is animation selection/generation, not another record field — so the next bounded question is audit18 (the 65236/65237 generation entry and 63666's output selection). Audit only: no game run, no recipe change. See [audit](docs/behavior-graph-construction-audit.md#audit17) and [next questions](docs/independent-behavior-graph.md#next-audit).

- Record the G2 v19 run and adjudication (2026-10-26 23:46 session): the loaded identity matched the build, all `GRAPH FRAME schema=v19` rows (30 + 1) carried `root218=0x0 b248=0` and a non-null `eye`, the run produced 40 INDEPENDENT GRAPH and 36 BEHAVIOR GRAPH rows with no `[E]` and no crash, `source-stable=true retained=true`, and the summary reported `arm-observed=no` with the pose unchanged at `bc160a8c2b3a5a58`. Adjudication: pre-registered branch 6 — the arm block's outer gates all passed (non-null eye, `+0x2A=0`, `+0x2B=1`, `[graph+0x248]=0`, step-0 pre-swap read slot `+4=21`/`+0x38=1.0`), so the stop is inside the step computation: with rootNode null the distance branch is skipped (`xmm2=FLT_MAX`) and the 0xbb2603 `63864()` normalisation replaces the 0.25 candidate (`xmm6 = 63864() - 0.5*dt` when the return is ≤0.25), leaving `xmm1 = xmm6 + t_larger ≤ 1.25*dt`; no timer was rewritten, so no Path B. New runtime facts: the candidate's `[graph+0x218] rootNode = 0` (the earlier non-null reading is withdrawn) and audit16's "null root means a fixed 0.25 step" is refuted. The log is archived verbatim as [CharacterPanel-g2v19-eye-arm-20261026-2346.log](docs/diagnostics/CharacterPanel-g2v19-eye-arm-20261026-2346.log) (41,470 bytes, SHA-256 `45AAEE09…`); the deployed INI was returned to `off` and the v19 DLL/PDB stay deployed. G2 remains NOT PASSED; the next bounded question is audit17 (`63864`'s return semantics and the missing candidate rootNode), with no eye-offset, dt, timer or weight changes authorised before it. See the [v19 run record](docs/independent-behavior-graph.md#g2-v19-run).

- Implement the v19 eye-position experiment (commit `e17d5d1`): in the inject branch only (exact 1.6.1170) each step's `BSAnimationUpdateData`-shaped record now carries a non-null `+0x18` — a read-only stack point derived from the candidate rootNode's `+0xA0..+0xA8` shifted +900 on Z (beyond the arm block's 800-unit cap, maximum 0.25 step), or a zero vector when the root is null (the arm step is then fixed and unread, but the pointer stays non-null). Drive/construct/load keep the v18 null record field. The frame trace becomes `GRAPH FRAME schema=v19`: stage rows append `root218/b248/eye` and the summary appends `arm-observed` (`min(t0,t1)` above the `{0,-MAXFLT}` reset value), with no new row kinds and the 30+1 / 40-row budgets unchanged. `frame_trace_policy.h` gains the engine-free `EyePoint`, `make_eye_point` and `arm_observed` helpers, and the case set grows to three (null root → zero point, root → +900 on Z; reset false, armed true); all three pass under `g++` 15.1 with `-Wall -Wextra -Werror`. Release build identity `CharacterPanel-scenegraph-behavior-graph-v19-eye-arm-20261026-e17d5d12f6-cl94faaed0c6-20261026T153952Z` (DLL SHA-256 `344d9abb…`, 1,249,792 bytes; PDB `9848cd14…`) with only the existing D9025 and install-rule warnings. Deployed on 2026-10-26 without packaging (v18 pair and its `off` INI backed up under `dist/deploy-backup-20261026-v18/`) and run once; the run entry above records the branch-6 outcome. See [contract](docs/independent-behavior-graph.md#g2-v19-contract) and [build record](docs/build.md#v19-eye-arm-build).

- Add the G2 v19 pre-registered contract (design only, not implemented): a single-variable causal experiment that points the per-step `BSAnimationUpdateData::optionalEyePosition` at a read-only 3×float value (rootNode position +900 on Z, or a zero vector when rootNode is null) while leaving the call order, the 40-row `INDEPENDENT GRAPH` budget and the 30+1 `GRAPH FRAME` budget unchanged. It appends `root218/b248/eye` to existing `GRAPH FRAME` rows and `arm-observed` to the summary rather than adding row kinds. Six pre-registered branches are pinned (outer arm gate unreachable, armed-but-still-Path-A, Path B with the f gate still closed, full chain, crash/assertion, arming internal gate), and the contract states that the arming window is effectively only step 0 because the `+0x2C=1` old-read-slot `f>0` gate is satisfied solely by the step-0 slot-2 write — a failed step 0 stops the round without parameter tweaks. No code, build, package, deployment or run is included; G2 acceptance is unchanged. See [contract](docs/independent-behavior-graph.md#g2-v19-contract).

- Add the sixteenth 1.6.1170 binary audit (2026-10-26), which explains the v18 five-stage observation at instruction level: the update record is `RE::BSAnimationUpdateData`, whose `+0x18` is `optionalEyePosition` (`NiPoint3*`) and whose only writer is the REFR-family vtable slot 0x3C8 `ModifyAnimationUpdateData` (`TESObjectREFR` 0x2f71a0 is a bare `ret 0` stub, `Actor` 0x6a3750 fills the eye/face node +0xa0 or null, `PlayerCharacter` 0x74e7a0 overrides). 63587 has exactly two call sites (manager loop 0xba3dd5, 63602 tail 0xbb63b8); the engine's own per-frame producer 0x54c130 zeroes `+0x10..+0x1F` and never calls slot 0x3C8, while the seven external-record sites let their REFR fill the record before 0x54c1f0. The Path B fork condition is corrected to `record+0x2A==0 && t_larger>=0 && t_smaller>-MAXFLT` (no upper bound; `{0,-MAXFLT}` fails even at dt=0, so the earlier "dt=0 is the only statically satisfiable form" reading is withdrawn). The t-arming block (0xbb252b) needs a non-null `+0x18`, `+0x2B!=0`, `[graph+0x248]==0` and the `+0x2C`/slot conditions; it writes `t_smaller = t_larger + step` and jumps past the reset, while the reset (0xbb2662) runs only while `-t_larger>=0` and clears both slots' onFraction/numData. 63581's gate requires both slots `+4>2` **and** `+0x38>0` plus `t_larger>=0`/`t_smaller<0`; `f=1.0` is written only by 63582 (63568 only back-fills `+0x32`), and 63575's count also depends on the eye position. Consequence: the candidate's `+0x18=null` pins t at the reset value, forces Path A on every step, and that reset clears the weights — v18's observations now have a machine-code explanation instead of an open hypothesis. The audit retires audit 15's "engine per-frame record (transform=null) → Path B" and "63581 gates pass statically after a Path A reset" claims. Audit only: no game run and no source/behaviour change; the bounded next step is a v19 causal-experiment contract that supplies an eye position (not implemented). See [audit](docs/behavior-graph-construction-audit.md#audit16) and [next questions](docs/independent-behavior-graph.md#next-audit).

- Record the G2 v18 run and its adjudication (2026-10-26 23:01 session, actor 00000014/request=1): the loaded `SCENEGRAPH BUILD` identity matched the rebuilt package, the raw log is archived verbatim as [CharacterPanel-g2v18-trace-20261026-2301.log](docs/diagnostics/CharacterPanel-g2v18-trace-20261026-2301.log) (40,366 bytes, SHA-256 `D42DB99D…`), the run produced 30 stage rows plus one summary, 40 INDEPENDENT GRAPH rows and 36 BEHAVIOR GRAPH rows (three 12-row source blocks, all identical), with no `[E]`, no crash, `source-stable=true retained=true`; the deployed INI was returned to `off`. Adjudication (pre-registered, not relaxed): both slots satisfy the whole-track count gate (`numTracks(+0x4)=21` at every stage) while `numData(+0x32)` stays 0 and both slots' `onFraction(+0x38)` are 0 at every sampling moment — the slot-2-written 1.0 is cleared between the step's 63602 pass and the 63587 return, so the reset is localised inside the update wrapper rather than the pipeline or "never initialised"; the time pair is pinned to the Path A reset value `{0,-MAXFLT}` and every step shows the Path A signature (`w245=0x101 b246=1`, one double-buffer swap per step) because the prologue subtracts dt before the fork and the only in-function time advance needs a non-null `record+0x18`; `numAnim(+0x240)` is 0 after each native update (the 116 the sampler sees is the plugin mirror), so 63575/0x246-clear/63568-back-fill are all bypassed; the candidate pose stays `bc160a8c2b3a5a58` across 120 returned 63581 calls with zero raw-buffer changes and the source pose/baseline is unchanged. `completed` counts returned calls only; generator payload, 63581's native context, 63666/63582 and 58423 remain unobserved. G2 remains NOT PASSED; the narrowed questions (where a valid Path B time interval comes from, what fills the headers after the fork, and the post-gate application chain) are the next bounded audit, and no seeding or recipe change is authorised before it. See [run record](docs/independent-behavior-graph.md#g2-v18-run) and [next questions](docs/independent-behavior-graph.md#next-audit).

- Rebuild, repackage and redeploy the v18 frame-trace observation package from committed HEAD `7ee4cce` (2026-10-26 evening) because the previously recorded artifacts are absent from this checkout: `build/` was still configured with label `scenegraph` and held only the v17 pair, so the record's `…-92a3ed071d+dirty…` identity, `e24ad2a5…` DLL and `cd364569…` ZIP described files that could not be shown to exist. No source change was involved. New identity `CharacterPanel-scenegraph-behavior-graph-v18-frame-trace-20261026-7ee4cce3bf-cl94faaed0c6-20261026T143615Z`, DLL SHA-256 `72c33a0e…` (1,249,280 bytes), PDB `1baceb55…` (35,409,920 bytes), CodeView GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}` age 317, ZIP `dad6b72c…` (9,763,102 bytes, five entries re-extracted and re-hashed against the manifest). The two engine-free policy/progress cases pass under the local `g++` 15.1 (the earlier record's `C:/env/llvm` clang path does not exist here); Release configure/build pass with only the existing D9025 `/Ob2`→`/Ob3` and install-rule warnings. History vs. the earlier record carries two inert differences only: the label says `7ee4cce3bf` instead of the pre-commit `92a3ed0+dirty`, and the previously untracked, unreferenced `src/base/util.cpp`/`util.h` are no longer in the checkout so the unchanged CMake glob no longer compiles them. The v17 pair was preserved (`build/backup-v17-20261026/`, DLL `904D6B5B…` matches that record) and backed up under `dist/deploy-backup-20261026-v17/`; the MO2 instance carries this build, its INI was set to `inject` for the run recorded in the entry above and is back to `off`, and the repository sample INI stays `off`. The earlier hashes are history only — the loaded `SCENEGRAPH BUILD` identity plus the package manifest are authoritative. See [build record](docs/build.md#v18-frame-trace-build) and [handoff](docs/independent-behavior-graph.md#v18-handoff).

- Add bounded v18 frame observations to the explicit independent-graph inject experiment on AE 1.6.1170: separate output-slot counts/weights and update timing at thirty boundaries plus one raw-pose progress summary. Preserve the v17 native recipe and default-off behavior; no game result or G2 pass is claimed.

- Clarify the G2 v17 result (2026-10-26): the [raw run](docs/diagnostics/CharacterPanel-g2v17-native-20261026-0021.log) loads in 8.289 ms, allocates 116 poses and dispatches WeapEquip with node/variable changes, but candidate pose hashes remain equal at 0/60/120; G2 has not passed. G1 passes revised Option A only; loaded-graph teardown and resource convergence remain unverified. The 63581 entry is reached, not a proved sampling/mixing/application body. slots n is numData at +0x32, distinct from native numTracks at +0x4; f=0 at recorded checkpoints is a blocker. slot2/post-pipeline1 already set f=1 before update resets it, so initialization/reset/refill timing is a prioritized hypothesis, not the proved sole cause. Path B also needs the t condition; the failed v17 prediction applies to its tested recipe, not all positive-dt native frames. See the [posterior correction](docs/independent-behavior-graph.md#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定).
- Historical v17 pre-run implementation/deployment record (2026-10-26; predicted Path B signature superseded by the [run correction](docs/independent-behavior-graph.md#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)): Implement, build and deploy the G2 v17 engine-native frame recipe ([contract section](docs/independent-behavior-graph.md#2026-10-26-g2-v17-契约引擎原生每帧序path-b-记录--谓词翻假--63581-直调经续审计十五设计)), designed from audit 15's closing fact list. Three changes to the [drive loop](src/animation/behavior_graph/independent_graph.cpp) behind the unchanged `IndependentGraph=inject` setting, observation budget held at 40 rows (only the `phase=slot2` row gains a `removed=` field): **(1) Path B record** — the per-step 63587 record switches from v15's 63602-tail copy (`+0x2A=1`, a slot-5 zero-dt pipeline special) to the manager's real per-frame producer 0x54c130 byte map (audit 15 Q4): `float dt=1/60` at +0x00, `+0x08..+0x27` zero, `u16 +0x28 id=0`, `+0x2A=0` (Path B — no double-buffer swap, no track reset, 0x246 cleared, `[0x240]` recomputed via 63575), `+0x2B=1`, `+0x2C=1`, `+0x2E=1`; **(2) predicate flip** — slot 3 (63622, RemoveRagdollFromWorld) moves to immediately after the slot-2 call, before the drive loop, undoing slot 2's per-body world registration while the physics-world borrow is still in place so `rigidBodies[0]->world` is null at every sampling moment and 63564 returns false (audit 15 Q1: the only non-chain-breaking flip on the candidate; v12 proved slot 3 leaves the track headers intact); the post-loop slot-3 call stays as an idempotent re-check feeding the existing `phase=pipeline` `ragdoll-removed=` field; **(3) direct 63581** — the per-step sampling entry 63589 is retired (its b246 fork was the very detour v16 pinned) and the sampling block is reached by a direct `63581(candidate, word[graph+0x240])` call (`SamplingBranchID`; audit 15 Q2's two-live-argument convention, second argument read from the plugin's existing per-step mirror field after the mirror write); `sampling_entry`, `SamplingEntryID` and `mode_byte` are removed with zero dangling references. Pre-registered criteria ①–⑤ in the contract section: ① `pred=0x00` + pose triple diverges (source stable, no crash) → G2 motion evidence achieved (PASSED on the motion criterion), next = OAR integration gates with a stability rerun; ② `pred=0x00` pose still frozen → audit 16 instrumenting 63581 internals (63666 output / 63582 mix / 58423 gates); ③ `pred=0x01` → audit the re-registration path; ④ `post-update1` without the Path B signature `w245=0x0000 b246=0` → re-review before any further step; ⑤ no crash, source stable; early-step gate ②/③ failures of 63581 (the `[0x228]` slot's onFraction may still be 0.0 before the pipeline fills it) are pre-registered as expected transients. Expected new signatures: `post-update1` shows `w245=0x0000 b246=0 num-anim=0x0074` (Path B recomputes it) and `phase=slots` pointers no longer alternate (no swap). Built as `CharacterPanel-scenegraph-scenegraph-a32766a080+dirty-cl94faaed0c6-20261025T160248Z` (DLL SHA-256 `904D6B5B…`, PDB `1B208E04…`), deployed on 2026-10-26 after backing the v16 pair up to `dist/deploy-backup-20261026-v16/`; the deployed INI was set to `inject` for the run adjudicated in the entry above and is back to `off` since.
- Record the G2 v16 run adjudication (2026-10-25) and add the binary-audit 15 section, "63564 谓词真值条件与 63581 采样入口约定（2026-10-25 续审计十五）" ([doc](docs/behavior-graph-construction-audit.md)). The run's raw log was archived verbatim as `CharacterPanel-g2v16-gates-20261025-2258.log` (byte-identical by `cmp`; 2026-10-25 22:58 session, build `…a32766a080+dirty-cl94faaed0c6-20261025T2245xx` per the v16 contract's recorded identity, DLL SHA-256 `A732AC08…`; request=1, actor 00000014, exactly 40 rows = the 40-row budget, no crash, `source-stable=true`). The decisive rows are verbatim — `phase=g3-gates g100=0x27073572de0 flag4a=0x00 ctx+50=null +40=null +88=null`, `phase=pred step=0 r=0x01`, `phase=pred step=60 r=0x01` — the reading: **pre-registered criterion ① landed, G1 confirmed** — the 63564 predicate returns true on the candidate at both checkpoints, so 63581 skips its entire sampling/mixing/pose-application block every step and the candidate pose stays bind-frozen (`bc160a8c2b3a5a58`×3); the predicate reads exactly the two fields the borrow window makes non-null (physicsWorld borrowed, ragdollDriver own), confirming the suspicion raised at contract time as the mechanism to defeat in v17. A compounding recipe fact (second gate in the same run): 63587's prologue writes `word[0x245]=0x101` every step and Path A never resets 0x246, so at sampling time `b246=1` every step → 63589 routes to 63568 (header back-fill only, dl=0 → return) — **63581 was never reached even absent the predicate gate** (supporting rows: `post-update1 … w245=0x101 b246=1 b249=1` matching v15; the step-120 slots row `p0=… n=0x0074 f=1.000 h0=48bfefcceccb91f1` showing the back-fill landed in the read slot with a changed element hash against the all-zero pristine hash `1ec64cb0955cbfc0` elsewhere). The `g3-gates` row shows the plugin's 58315-built context chain entirely null (`ctx+50=null`) — moot under criterion ① (63581 builds its own context at 0xbafe48; the 58423 gates were never reached), recorded for the v17 design. Slots-sequence facts: the two slot pointers alternate every step (Path A swap visible at steps 0→1→2) and both slots carry the pristine hash until the step-120 read slot shows the written state. Audit 15 answers the four questions v17's fix design depends on, all address-cited with reproduction commands: **Q1** — 63564 (0xbae3d0, 0x61 B) is a pure five-level non-empty chain (`[graph+0x238]` physicsWorld → `[graph+0xF0]` ragdollDriver → `driver+0x88` hkaRagdollInstance via the two-instruction pure getter 58286 → `rigidBodies[0]` → the rigid body's `hkpWorldObject::world` at +0x10) with no numeric comparison — the audit-10 blend-out/mode-byte suspect (`driver+0x10`) is structurally refuted (that offset is read only as the `rigidBodies` data pointer); on a healthy living actor the predicate returns false exactly on `rigidBodies[0]->world == 0` (ragdoll bodies not registered into a physics world), and that is the only flip that does not break the chain (the explicit field values that would flip the return are listed per exit). **Q2** — the 63589→63581 call site (0xbb2ac3) passes exactly two live arguments `(rcx = graph, edx = word[graph+0x240] u16)` with no stack arguments; every other register is dead at first use, and the plugin's `&mode_byte` argument is consumed only by the 63568 branch (audit 14's edx reading pinned into a full convention). **Q3** — 63581's own gates before the sampling block are enumerated (edx≠0; both slots' `+4>2`/`+0x38>0`; the t-pair bracket — all statically satisfied after 63587's Path A reset), the plugin could direct-call 63581 with `(rcx=candidate, edx=word[candidate+0x240])` to bypass the 63589 fork, and pre-gate side effects are bounded (stack copy, `[0x230]/[0x234]` bookkeeping, conditional 63771) with runtime magnitudes UNVERIFIED. **Q4** — the manager's real per-frame 63587 record at the 0xba3dd5 call site (inside 0xba3c50) is passed through from caller rdx and built by 0x54c130 as `{dt=xmm1, callback/args/transform=null, id=0, +0x2A=0, +0x2B=1, +0x2C=1, +0x2E=1}` — **+0x2A=0, Path B semantics** (the LOD resolver runs and 0x245/0x246 are cleared), so the engine's own per-frame sampling executes the 63581 branch; the `+0x2A=1` records (v15's and 63602's tail) are slot-5 zero-dt pipeline specials. Audit 7's "+0x2A non-zero = skip selection" reading is confirmed verbatim at 0x54c1f0's `cmp byte [rdx+0x2A], bl`. The closing factual summary names what v17 must arrange (predicate flip on `rigidBodies[0]->world` + sampling reach via Path-B-semantics b246=0, direct-call convention documented) without designing v17, and marks the 63581-built context chain values UNVERIFIED (never to be extrapolated from the null plugin-context g3-gates row). Doc-head conclusion, G2 ledger row, next-stage entry and README line 17 all name the "v16 adjudicated → v17 next" stop point. INI returned to `off` per convention (the deployed v16 build stays in place).
- Add the binary-audit 14 section, "63587 内部条件与位姿最后一公里（2026-10-25 续审计十四）", resolving the four questions the v15 run raised ([doc](docs/behavior-graph-construction-audit.md)): Q1 — the Path A/B fork is decided by `record+0x2A` (non-zero walks Path A unconditionally; both the engine's 63602-tail record and the v15 direct-call record carry 1, so all 120 steps swapped and reset; audit 13's ±MAXFLT reading was corrected to the necessary side only) and the +0x1E8 pair is zeroed to {0, -MAXFLT} by Path A's common tail each step; Q2 — the per-step sampling reads the `[0x220]` slot, the swap is a pure pointer exchange, the gen0elem hash step proves real content writes, and `tracks-end +32=0x74` is compatible with 63568 having run only on step 0 (driven by the plugin's own 0x240 mirror); Q3 — 63568 never copies transform data, 63582 is the write-slot mixer, and poseLocal's only static write chain is 63589→63581→58423; Q4 — 58429 is a two-instruction leaf writing one float into `[graph+0x100]+0x4C`, unrelated to poseLocal (audit 13's "pose apply" reading was an over-read). The closing section names the remaining gate candidates for v16 (the 63564 predicate, the 63666 output base, the 58423 gates, or the pipeline-vs-reset ordering), all UNVERIFIED runtime values explicitly marked; the doc-head conclusion, G2 ledger row and next-stage entry were refreshed accordingly and README line 17 updated. No product code changed; no v16 design included.
- Record the G2 v15 run adjudication (2026-10-25): the pre-registered criterion ② landed, and the engine recipe ran fully alive on the candidate for the first time. The build was `CharacterPanel-scenegraph-scenegraph-a32766a080+dirty-cl94faaed0c6-20261025T075136Z` (DLL SHA-256 `8DED2F93…`); the 2026-10-25 19:04 session (request=1, actor 00000014) produced 32 rows within the 34-row budget, no crash, `source-stable=true`; raw log archived as `CharacterPanel-g2v15-wrapper-20261025-1904.log` (byte-identical to the live log by `cmp`). The two decisive rows are verbatim — `phase=post-pipeline1 num-anim=0x0 w245=0x0 b246=0 b249=0 out0[+32=0x0000 +38=1.000] out1[+32=0x0000 +38=0.000]` versus `phase=post-update1 num-anim=0x0 w245=0x101 b246=1 b249=1 out0[+32=0x0000 +38=0.000] out1[+32=0x0000 +38=0.000]` — the reading: 63587 ran on the candidate for the first time (prologue bookkeeping `w245=0x101`, `b249=1`); step 0 took Path A (num-anim not recomputed; the slot-2-written `+38=1.000` was consumed by the double-buffer swap and track reset; post-pipeline1 reproduces v14's all-zero, so 63602's tail still delivers nothing). First-time liveness facts, each new versus v7–v14: the active-node count evolves during the drive (97 → 125 at step 60 → 111 at step 120) and the activeNodes array itself was reallocated by the engine (`0x1af9385e440` → `0x1af9385e490`); the variable-set hash evolves per checkpoint (`893b33fd…` → `214e2111…` → `a4bb41b6…`); the gen0elem hash moves during the drive for the first time (`09335c48760eef8e` → `33583b6274311017`, stable at step 120); `tracks-end[+32=0x0074 +38=1.000]` — the numData=116 back-fill that v12–v14 chased is achieved. Adjudication (pre-registered, not loosened): bookkeeping present while the candidate pose triple `bc160a8c2b3a5a58`×3 stays unchanged (bind pose) — per pre-registration the gap moves inside 63587's internal conditions; the sharpened question is what still prevents the track transform data from reaching poseLocal with the kPose track header fully bound (numData=0x74, onFraction=1.0) and per-step element motion. Next work package = audit 14 (63587 internals and the last mile to poseLocal), then v16 designed from its findings; the deployed INI is back to `off` per convention and the deployed v15 build stays in place.
- Record the G2 v14 run and adjudication (2026-10-25) and deploy the G2 v15 direct update-wrapper build. The v14 evidence row answered its pre-registered question on branch ②: `phase=post-pipeline1` showed every 63587 bookkeeping field zero (`num-anim=0x0 w245=0x0 b246=0 b249=0`), the double-buffer slots were not swapped (out0's `+38=1.000` is only the run's own slot-2 write, out1 stayed 0.000), the injection transition and pose/track states reproduced exactly as before, and the `cand-anim=0074` at steps 60/120 is the plugin's own per-step `[graph+0x240]` mirror write (present since v7, verified in the v8e/v10 archived logs), not an engine write — so **63602's tail never ran 63587 on the candidate**. v15 replaces the bare `hkbBehaviorGraph::Update` 58329 per-step call with one direct `63587(candidate, &record, 0)` call built on 63602's tail stack-record layout (zero-initialized ≥0x30 bytes, float dt=1/60 at +0x00, byte modes +0x2a..+0x2e = 1,1,1,0,1 with footIK=1, rebuilt fresh every step), keeps the pipeline pass, the `[behavior+0x12E]=1` write, the `[graph+0x240]` mirror and the sampling entry unchanged, and adds one read-only `phase=post-update1` row (same field set as post-pipeline1, emitted at step 0 after the first direct call; budget 34 rows, net +1). Pre-registered criteria: pose hashes diverge (source stable, no crash) → motion evidence achieved; any 63587 bookkeeping appears while outputs stay frozen → the gap moves inside 63587's internal conditions (Path A/B, indirect update call) → audit 14; still all zero → 63587's own prologue gates (+0x110 / [[+0x110]+0x20] / +0x208 / [[+0x208]+0x12c]) block on the candidate → the +0x208 gate-chain audit is next. Built as `CharacterPanel-scenegraph-scenegraph-a32766a080+dirty-cl94faaed0c6-20261025T075136Z` (DLL SHA-256 `8DED2F93…`, PDB `605A0703…`), deployed on 2026-10-25 after backing the v14 build up to `dist/deploy-backup-20261025-v14/`; the deployed INI was set to `inject` for the run adjudicated in the entry above and is back to `off` since. Raw v14 log archived as `CharacterPanel-g2v14-pipeline1-20261025-1336.log`.
- Add the thirteenth 1.6.1170 binary audit (the 63587 full recipe and the 0x246 fork) and the G2 v14 evidence-only build. The v13 run falsified audit 12's "sampling blocked at the first gate" inference — the candidate's `[graph+0x110]` was already filled by its own load subchain (the slot-0xC call was a no-op), so the sampling entry's first four gates pass; the real fork is the fifth: `[graph+0x246]=0` routes every step into the 63581 branch, never reaching 63568 (the numData writer). Audit 13 then pinned the per-frame writer of 0x245/0x246/0x240: **63587, the engine-level update wrapper — the complete per-frame recipe** (hkbContext build 58315, preupdate 63761, the behavior update through indirect calls, event drain 63583, pose apply 58429, the numAnimBones/`word 0x245=0x101` bookkeeping, and the generatorOutputs +0x220/+0x228 double-buffer swap); the plugin's raw 58329 call skips all of it, and 63602's own tail invokes 63587 on the target graph with a stack record (dt=0, footIK=1) — whether the candidate ever receives that call is undetermined (246=0 fits both "received, Path B" and "never received"). v14 adds one read-only row (`phase=post-pipeline1`) after the first pipeline pass, logging numAnimBones/0x245/0x246/0x249 and both double-buffer slots' track fields, with pre-registered adjudication: bookkeeping values present or slots swapped → 63587 runs on the candidate (gap moves inside its conditions); all zero and unchanged → 63587 never reaches it (v15 = call 63587 directly with the engine's own record layout in place of the raw update). Built as `CharacterPanel-scenegraph-scenegraph-a32766a080+dirty-cl94faaed0c6-20261024T172336Z` (DLL SHA-256 `33CF02FF…`), deployed on 2026-10-25 after backing the v13 build up to `dist/deploy-backup-20261025-v13/`; the deployed INI is set to `inject`, awaiting the next game run. Raw v13 log archived as `CharacterPanel-g2v13-slot0c-20261025-0118.log`.
- Add the twelfth 1.6.1170 binary audit (sampling-entry gates and the numData writer) and the G2 v13 direct slot-0xC call. The kPose numData back-fill writer is **63568**: after building an hkbContext via 58315 and resetting every track header it stores `[tracks+0x32] = word [graph+0x240]` (numAnimBones — the very field the plugin already mirrors each step). Its only caller is the per-step sampling entry 63589, gated behind five checks whose first is `[graph+0x110] != 0` — so the candidate's sampling entry has been returning at its first gate every step and never sampled at all. `graph+0x110` is filled only by slot 0xC (63631), which picks one of the projectDBData +0x170/+0x178 objects by its bool argument — the engine reaches it only behind the same 63643 gate as slot 2 (audit 11 and 12 are two faces of one gap: the candidate never got this one assembly step). v13 calls 63631 directly once after the slot-2 call (dl=false, retried with dl=true only if +0x110 stays null), with a new `phase=slot0c` row logging `[graph+0x110]`, the picked object's +0x20 and the `[graph+0x246]` branch byte before/after; pre-registered adjudication: 110 filled && tracks-end +32=0x74 && outputs move → the +0x110 assembly was the last gap; 110 filled but +32 still 0 → sampling-internal branches; 110 still null → 63631 precondition review. Built as `CharacterPanel-scenegraph-scenegraph-a32766a080+dirty-cl94faaed0c6-20261024T171309Z` (DLL SHA-256 `216A003E…`, PDB `C16B6EA6…`), deployed on 2026-10-25 after backing the v12 build up to `dist/deploy-backup-20261025-v12/`; the deployed INI is set to `inject`, awaiting the next game run.
- Record the G2 v12 run and adjudication (2026-10-25). The direct slot-2 call executed for the first time on the candidate: `added=true`, and 63621's head wrote the kPose track's onFraction from 0.000 to 1.000 exactly as audited (+32 stayed 0 — slot 2 itself writes 0), slot 3 removed the ragdoll cleanly afterwards (`ragdoll-removed=true`), the injection transition reproduced, the source stayed byte-stable and nothing crashed — but the candidate pose hashes stayed equal across all three checkpoints and the gen element hash never moved, landing the pre-registered ② branch: slot 2 is runnable on the candidate but not the last gap; the remaining gap is the numData=0x74 back-fill path, resolved the same session by the twelfth audit above. Raw log archived as `CharacterPanel-g2v12-slot2-20261025-0106.log`; the deployed INI is back to `off` pending the v13 deployment.
- Add the eleventh 1.6.1170 binary audit (slot-2 identity and the element-field writer) and the G2 v12 direct slot-2 call. The evidence fields decode as the generator output's kPose track header: `output+0x00 → Tracks{0x10 master + 0x10-stride TrackHeader}` with numData at +0x32 and onFraction at +0x38 — 0x74=116 is numPoseLocal, unrelated to the 18 ragdoll bodies. **Slot 2 (63621) is BSIRagdollDriver::AddRagdollToWorld and its head is the writer**: right after the world/driver/instance gates it stores `[tracks+0x32]=0` and `[tracks+0x38]=1.0f` unconditionally, then activates each rigid body into the graph's physics world (slot 3 / 63622 is the symmetric removal). The candidate's +38=0.0 therefore proves slot 2 never executed on it — and 63602 only reaches its slot-2 call behind a gate whose provider is 63643 (a per-frame current-animations hash fed by slot 0xC / 63631, which reads the projectDBData +0x170/+0x178 object pair), a chain the candidate never satisfies. Two audit-9/10 readings are corrected: 63630 never writes the passed rdx out-slot (its count returns in EAX), and the full main vtable is now mapped slot 00–0D. v12 removes the v11 instance borrow (proven non-load-bearing) and directly calls 63621 on the candidate before the drive (same direct-call discipline as 58453/58377), with a new `phase=slot2` row (added result + track fields before/after) and slot-3 removal after the drive before the world restore (`phase=pipeline` gains `ragdoll-removed=` and `tracks-end[]`); pre-registered adjudication: added && +38=1.0 && outputs move → slot 2 was the last gap; +38=1.0 but frozen → the numData=116 back-fill path is next; added=false → gate re-review. Built as `CharacterPanel-scenegraph-scenegraph-a32766a080+dirty-cl94faaed0c6-20261024T165857Z` (DLL SHA-256 `EA5B3B27…`, PDB `CAB61CED…`), deployed on 2026-10-25 after backing the v11 build up to `dist/deploy-backup-20261025-v11/`; the deployed INI is set to `inject`, awaiting the next game run.
- Record the G2 v11 run and adjudication (2026-10-25). The corrected `phase=driver` row took its first evidence and **falsified the audit-10 static inference**: the candidate already owns an 18-body hkaRagdollInstance (`driver+0x88` non-null; the source's is 18 too — `0x74`=116 is numPoseLocal animation bones, not rigidBodies, so audit 9's 116=rigidBodies.size reading was also wrong), meaning the binding loop's count was never zero and v10's "loop ran zero iterations" attribution collapses; the candidate-side +0x88 fill happens somewhere inside the plugin's 63585 load subchain (fill site to be traced). The v11 borrow executed and restored cleanly (`ragdoll=borrowed saved=…`, `ragdoll-restored=true`), the injection transition reproduced (97→92 active nodes, variables rewritten), the source stayed byte-stable, and nothing crashed — but the candidate pose hashes stayed equal across all three checkpoints and the candidate gen0 element hash never moved (binding fields +32/+38 unwritten; the probe reads the candidate side), landing the pre-registered ② branch: the gap moves inside the loop — per-bone element pairing / generation-traversal reachability. The next work package is audit 11: a whole-image store scan for the writer of the source element's `+32=0x74/+38=1.0` (binding-time vs per-frame, and the track↔rigid-body mapping source — animationBindingSet / boneToRigidBodyMap / 65237 reachability), plus tracing the candidate's +0x88 fill site; 63952 argument reconstruction is parked until ① rules. Raw log archived as `CharacterPanel-g2v11-borrow-20261025-0015.log`; the deployed INI is back to `off`.
- Add the tenth 1.6.1170 binary audit (ragdoll provider identity and fill chain) and the G2 v11 ragdoll-instance borrow. The audit retires all three audit-9 suspects (58453 is only the hkbContext build plus the `[this+0x58]` object's vtable slot 4; 63356 only walks the actor's graph array calling the 63586 activation wrapper; 0xaea12b is a cold split-off of a larger function), then resolves the chain RTTI-first via the new `ragdoll_rtti_scan.py` (type descriptor → COL → vtable; vtable slots and vtable[-1] are stored as full VAs in this image, only intra-RTTI pointers are RVAs): the bone provider 63630 reads is the **hkaRagdollInstance** at `[graph+0xF0]→hkbRagdollDriver→[+0x88]` (58286 is the pure `[rcx+0x88]` getter), and its table/count are the `rigidBodies` hkArray at instance +0x10 (`{data +0x10, size +0x18, capacity +0x1C}` — audit 9's offsets were right but one object level off; the driver's own +0x10/+0x18 hold a blend-out float and mode bytes). The fill chain is pinned: the hkbRagdollDriver constructor (58280, raw) nulls +0x88 and is created only through 58449 — called by the graph constructor 0xbad070, the loader 63585, and **63870 (0xbc44e0, the engine's full character assembly; audited once before as an undecoded `proj_63870` listing)**; 63870 creates the driver, passes it into 63949 (where the plugin's load chain stops), and — when the project carries ragdoll data (`[[setup+0x40]+0x40]+0x98 ≠ 0`) — builds a dedicated hkaRagdollInstance through 63952 (64145 construction from the project skeleton) and stores it with 58285 (`hkbRagdollDriver::SetRagdoll`, the +0x88 writer). The candidate gap is therefore exactly a missing ragdoll instance, explaining v10's pass-through-gates-but-count-zero run. The audit-9 `phase=driver` row read the wrong object (float/mode bytes) and never ran (the 23:33 launch exited before plugin load; the log stayed at the 22:45 v10 session) — v11 fixes the read chain (`driver+0x88` → instance → table/count, plus the +0x80 character back-pointer), and borrows the source's ragdoll instance pointer inside the v10 world-borrow window (raw pointer copy, no refcount churn, restored beside the world restore; `phase=borrow` gains `ragdoll=borrowed|skipped-no-driver|skipped-no-source-ragdoll saved=`, `phase=pipeline` gains `ragdoll-restored=`) with pre-registered adjudication (pose/element-binding change → the per-bone binding loop was the last gap; borrowed-but-still-frozen → per-bone element pairing/generation reachability; `skipped-no-source-ragdoll` → falsifies the source-rigidBodies premise against the v9 +32=0x74 evidence). Built as `CharacterPanel-scenegraph-scenegraph-a32766a080+dirty-cl94faaed0c6-20261024T160418Z` (DLL SHA-256 `E57C19A8…`, PDB `0C5509E7…`), deployed on 2026-10-25 to the MO2 instance after backing the audit-9 build up to `dist/deploy-backup-20261025-audit9/`; the deployed INI stays `inject`, awaiting the next game run. Candidate-side proper fix (replaying the 63870 tail with candidate-own project data) is parked for G3+ with the 63952 argument reconstruction as its prerequisite audit.
- Add the ninth 1.6.1170 binary audit (binding-loop bone-mapping semantics and vtable clarification). Slot 0x58 of the final main vtable 0x19881f8 is slot 0B = 63630 (the earlier 0E/0x11 readings were dump overrun garbage); 63630 is the per-bone mapping provider whose count comes from the driver-side array and whose loop is the per-bone output-binding path 63602 walks (revising v10's "manager current-animations list" attribution), leaving the candidate gap as an empty driver bone array. This audit's object-level reading was corrected one level deeper by the tenth audit (see above); its deployed `phase=driver` observation row (build `…20261024T150352Z`, DLL SHA-256 `39665773…`) was superseded by v11 before ever reaching a game run.
- Add the eighth 1.6.1170 binary audit (event dispatch chain and slot-2 output gates) and the G2 v8 `inject` experiment value. The audit pins the injection primitive at 58377 — the engine's event-triggered-transition dispatch that Update's own tail and the batch queue dispatcher (0xab313e) route queued events through — plus the ring queue layout (entries/capacity/read cursor/pending count at +0x00/+0x08/+0xC/+0x14), the runtime name→id resolution over the projectDBData BSTHashMaps (event names are not in the executable), and the discovery that the per-frame output reset (graph vtable slot 2) is double-gated on physicsWorld and the ragdoll driver, both empty on the candidate. `IndependentGraph=inject` runs the v7 engine-pipeline drive preceded by a read-only name→id lookup for the pre-registered candidates (MT_Idle/MoveStart/WeaponDraw/SyncForward) and one 58377 dispatch of the first resolved name; unchanged generator elements after injection attribute to the slot-2 gates by pre-registration. Built (`…440e7cf784+dirty…20261024T133638Z`, DLL SHA-256 `69B8683D…`) and deployed with the INI set to `inject`. The first run crashed in the plugin's own read-only map walk: bucket chains end at the BSTHashMap sentinel (`map+0x18`, exactly where the engine's walks stop), not null, so the first strcmp dereferenced the sentinel's contents; the 58377 dispatch never executed and the pre-registered criteria are unaffected. The fixed walk (sentinel stop plus chain-length cap) is rebuilt as `…20261024T134847Z` (DLL SHA-256 `109D1C7D…`) — and the rerun crashed at the same strcmp with different name bytes, proving the +0xA0 map cannot be walked with the unk70-derived layout at all. The map-walk route is retired in favor of a raw `dbdump` row plus a bounded linear scan of the animation-name array, rebuilt as `…20261024T135805Z` (DLL SHA-256 `5764BCB5…`). That rerun was clean but all four pre-registered names missed (pre-registered clean skip), and the dbdump decode corrected the crash root cause: both hash maps match the declared layout, but their bucket chains hold tombstone entries with garbage key bytes — the engine only pointer-compares keys, so by-content walks are unsafe anywhere in them. The authoritative event source is the behavior graph's own `stringData->eventNames` (event id = array index, the state-machine transition id space), with a bounded first-16-names evidence row; rebuilt as `…20261024T141115Z` (DLL SHA-256 `D86BAA87…`). Its run was clean and put the graph's own event table on record for the first time (8223 events; `FootLeft`/`WeapEquip`/`SprintStart`/`1HM_Unequip` among the first sixteen), proving the four pre-registered names are Skyrim-level anim-event names rather than graph events (clean pre-registered skip; the dispatch still has not run). The list is re-registered from that evidence (`WeapEquip` → `SprintStart` → `FootLeft` → `FootRight`) as `…20261024T141951Z` (DLL SHA-256 `17364747…`). That run is the injection milestone: all four names resolved, the 58377 dispatch executed for the first time without a crash, and the state machine genuinely transitioned (97→92 active nodes, variables rewritten, stable settling) — the candidate graph is fully event-responsive — while the generator outputs still did not move, landing in the pre-registered slot-2 attribution branch. v9 adds a read-only gates row (source/candidate physicsWorld and ragdoll driver, both output-element heads) whose run confirmed the slot-2 hypothesis: the living source passes both gates, the candidate already owns a ragdoll driver and lacks exactly the physics world, and the source's output element carries the bound track fields (`+32=0x0074`, `+38=1.0`) where the candidate's are zero. The pre-borrow audit showed slot 2 touches the shared world only through a pure getter and a release-disabled assertion stub, so v10 borrows the source's physicsWorld for the drive duration only (restored right after, `phase=borrow` row; criterion ① narrowed to the pose hashes — element-hash changes alone only prove the binding gate opened); rebuilt as `…20261024T144235Z` (DLL SHA-256 `789C5026…`). Its run executed and restored the borrow cleanly with the injection/transition reproduced, but the binding fields were still never written: the slot-2 call inside 63602 sits in a loop whose count comes from ARG2's vtable slot 0x58 out-parameter, and the candidate's count is zero. The gap converges back — precisely this time — to the manager-maintained current-animations/channels list that the binding loop walks; the INI is back to `off` and the next work package is the static audit of that provider and its list, plus the vtable slot-label ambiguity.
- Add the seventh 1.6.1170 binary audit (per-frame pipeline and record protocol), which revises the six-drive-iteration strategic conclusion: the 63587 update record carries NO animation identity — 63575 is the LOD sample-count resolver behind `graph+0x240`, 63583 is an event-queue drain plus name bookkeeping whose registration block is statically skipped with the engine's own null third argument, and the engine-built slot-5 record is all zeros except flags. The real gap behind the silent generator outputs was that bare `hkbBehaviorGraph::Update` runs no generation traversal; generation (65149→65232→65237) only happens inside the engine's per-graph frame pipeline 63602, which hosts each pass on a stack shell graph (63555/63556 pair, notification-center slot-5 recipe). The G2 v7 contract redefines the motion criterion on that basis: each of the 120 drive steps now runs one `63602(shell, candidate)` pipeline pass before the proven real-dt update, transform-count mirror and sampling entry, with pre-registered failure attribution (changed generator elements + frozen pose → sampling chain; frozen elements → default state has no active generator, idle needs event injection in G3). Implemented, built (`…440e7cf784+dirty…20261024T130326Z`, DLL SHA-256 `12663FFE…`), deployed and run: the pipeline recipe is safe on the candidate (120 shell pairs plus the real-dt update/mirror/sampling without a crash, source untouched) but the generator-output elements stayed constant and the pose buffer keeps the bind pose, so the pre-registered frozen-elements attribution applies — without event injection the default state produces no pose, the remaining gap is the event/initialization sequence that walks the state machine into idle, and G2 v7 is NOT PASSED on the motion criterion with the INI back to `off`.
- Add a second 1.6.1170 binary audit of the behavior-graph loader 63585: its second argument is a NUL-terminated full project path (`const char*`) that helper 69488 splits on the last `\`/`/` into directory and file name, helper 69490 canonicalizes (`/`→`\`, `.`/`..` segments) before a lookup against the global project registry singleton; the rejected G1b run is fully explained by the experiment passing a `BSFixedString*` instead (pointer bytes scanned as path content, registry miss, no side effects), and the resolver writes `projectName` as the file-name segment of that path on success.
- Implement the G1 independent-graph experiment behind a default-off `[Experiment] IndependentGraph=off|construct|load`: one native null-holder construction per process on the first paused panel-open request, an optional audited project load, at most 24 `INDEPENDENT GRAPH` observation rows, reference-count release and no activation, drive, variable write or OAR/DAC evaluation. Release build passes; the 2026-10-24 session ran the `construct` stage (G1a) and it passed the observable criteria. Three `load` iterations followed: two rejections (a copied `BSFixedString*`, then a `behaviorPath+projectName` rebuild) pinned down that the loader takes a `const char*` full project path whose registry keys carry directory and extension, and the third run loaded successfully on the first race-derived candidate (`Actors\Character\DefaultFemale.hkx`, 8.7 ms, every assembly field non-null, shared resources same-addressed as the source) but crashed in the destruction chain, where `hkbBehaviorGraph::Deactivate` dereferences the null `activeNodes` of a never-activated graph — destroying a loaded-but-unactivated graph is outside the engine's lifecycle. Per the design review the contract was revised (destruction evidence = shell release plus the loaded graph's teardown inside the G2 lifecycle) and the load branch retains the assembled candidate in a deliberately never-released owning smart pointer; the v4 run then completed the gate — load reproduced, the post-load source baseline is byte-identical, the retained graph caused no crash across further panel cycles — so **G1 is PASSED under the revised criteria**. A follow-up audit pinned the activation/update entries (engine activator 63586, `hkbBehaviorGraph::Update` vtable slot 5, context builder 58315), and a new `drive` experiment value implements the G2 contract: one engine activator call, 120 fixed 1/60 s update steps through a bare built context, layered motion/structure observations with source control hashes. The first G2 run: activation fully succeeded (97 active nodes, variable set allocated), the source stayed untouched, no crash — but the candidate produced no observable motion. The pose-write chain audit then found why: `hkbBehaviorGraph::Update` never writes `poseLocal`; the buffer is allocated by `hkbCharacter::AllocatePose` (58466, from `setup.animationSkeleton`) inside the engine's CHARACTER-level activator 58453, which the first run's graph-level wrapper bypassed. The drive flow now uses 58453 with a pre-registered clean skip when the fresh setup's skeleton is still missing. The v2 run validated the fix (pose buffer allocated at 116 transforms, skeleton confirmed filled) but the pose content stays at the skeleton bind pose. Four more iterations mapped the entire drive chain at instruction level — the probe proved the update advances all 97 node states per step, the sampler (63581) and its entry (63589) were located, the graph+0x240 word was identified as the sample-transform count, and the per-frame pipeline (notification hub + 63602's record protocol) was decoded. A wrapper attempt through the engine's own 63587 crashed in its post-update name-registration stage (context only the manager provides) and was reverted. Strategic conclusion: the remaining gap — animation SELECTION during generation — is manager-driven feature design (the panel preview will choose animations itself), not a feasibility question; the drive is back to the crash-free v5 form with the INI off, and G2's motion criterion returns to design review.
- Add a G1 runtime experiment contract for the independent behavior graph: three stages (construct / construct+load / source-holder control), a self-owned holder with Simple-equivalent slots, factory and loader call boundaries, a 24-line observation budget and pre-registered pass/fail criteria. Designed only; no setting, code or run exists yet.
- Add a 1.6.1170 binary audit of `ConstructAnimationGraph`: five engine implementations share one allocator/build/assign template, REFR holders versus null holders, no project loading or manager registration, and a paired aligned destructor; a follow-up pass locates the loading chain (orchestrators 32881/32882/32893/32902 → loader 63585 → resolver 63844 → behavior-graph load 63845) and shows a fresh graph instance per load over refcount-shared project data (hkRefPtr assign helpers 57010/57011, hkbSymbolIdMap rebuilt). Activation, event routing, runtime isolation and no-Actor reuse remain unproven; G1 is still not passed.
- Add opt-in source behavior graph ownership baselines and a six-gate independent graph validation procedure; independent graph construction and playback remain unverified.
- Add opt-in request-correlated OAR, load, annotation/DAC, capture/calibration and mount evidence
  for reported torch/shield phase differences and the one-shot axe equip flash; no visual fix is claimed.
- Bound common body/arm/gear pose diagnostics to four windows and twenty-four snapshots per request,
  with one fixed source-bone snapshot from the existing capture pass.
- Add audited native IED equipped-weapon placement, gear-node placement and SDS shield-on-back
  queries for current source equipment/configuration, preserving native comparisons and false
  results with the supplied condition provider.
- Audit OAR named-preset conditions by traversing the expanded preset conditions like multi
  conditions, so packs that gate branches through `PRESET` (e.g. Gunslicer crossbow,
  Malignis/Ashe torch idles) reach preview selection instead of falling to vanilla bases.
- Log event-driven world clip snapshots and final accepted panel base/logical/variant/bound
  mappings; expose source Smooth markers and bounded candidate rejection detail only with
  detailed tracing enabled.

### Changed

- Exclude unlisted local `.cpp` files from the plugin build by keeping an explicit list of intended translation units (v45, local only).

- Publish one `OAR QUERY` candidate rejection per distinct cause (first provider/condition/root-false
  signature, at most twenty-four) plus one suppression line, instead of the first eight
  unsupported and two native-false details: a pack whose replacements all share one condition
  (RaySense cover animations) used to consume the budget before a later folder's reason appeared,
  which is why the first correlated log could not explain the wrong equip clip.
- Key those rejection details by OAR project folder as well as cause (forty-eight rows), and add one
  `OAR COND … movement-seen=yes|no movement-answered=yes|no` row per query: the r2 log showed every transition
  still selecting the same clips, so the next session has to distinguish a condition that was
  answered by the standing predicate from one the hook never saw.
- Correct standing `IsMovementDirection` semantics: direction 0 means None and answers true; other directions answer false, with negation applied once. Preserve authored Smooth gates and combat-state behavior.
- Report world mappings through OAR's current-clip API and accepted panel mappings by final binding
  index, removing logging-only full-table reverse scans and all-candidate copies.
- Buffer ordinary information logs with periodic flushing and keep verbose animation/geometry
  diagnostics opt-in through `DetailedAnimationTrace=false`; warnings/errors still flush immediately.
- Keep unfamiliar IED-provider conditions on their native answer instead of excluding the
  candidates: they read the source actor's own equipment and placement like the engine
  predicates the preview already forwards, and shield/placement packs branch on several.
- Keep torch-only idles on TOR_Idle and combine a right one-handed weapon body idle with an independently selected left torch arm pose. Torch-only equipment changes do not replay the right weapon draw/sheathe move.
- Answer Smooth non-combat preparation readiness by the engine's own question (on markers
  present) instead of also demanding a keyword-free field, because the off markers carry the
  same dispel keyword as their on counterparts; failed preparations log which markers are
  absent, inactive or holding the keyword.

- Cache only fixed preview base-action indices per ready animation project before inventory,
  and read selected replacement binding names on demand instead of copying the expanded catalogue.
- Change inventory previews to equipped non-combat idles independent of world actions and sheathing.
- Add an empty-hand choice between the default unarmed non-combat loop and direct vanilla MT_Idle.
- Hold equipped gear in the copied hands regardless of world sheathing, preserving source state.
- Prepare the known Smooth non-combat loop through original DAC and wait for observed markers,
  restoring controlled source effects when the inventory closes.
- Use the supported original DAC NG Plus binary unchanged; require no DAC rebuild or runtime PDB.
- Blend advancing outgoing and incoming poses using copied OAR durations while retaining
  vanilla MT_Idle placement calibration.
- Document the three-condition IED/SDS query boundary and the current combat-branch limits;
  game-time and full-pack compatibility remain unverified.

- Change CMake to one Release DLL target and remove test, packaging and legacy preset entry points.

### Removed

- Remove Actor-based preview creation, equipment mirroring and puppet diagnostics; use SceneGraph previews in every build.

### Fixed

- Preserve complete retired-copy ownership tracking and report the true process-total retention count instead of a truncated bookkeeping count; native memory remains intentionally retained (v46, local only).

- Fix opt-in candidate playback speed changing with render cadence by pacing fixed steps with elapsed display time and limiting stall catch-up to six steps per draw (v44, local only).

- Fix independent F7 source-timeline windows to receive fresh 20-second/800-line budgets and discard stale traces after source loss, replacement or world reset (v43, local only).

- Report up to eight missing preview node names while preserving total coverage counts.
- Report one bounded preview rejection reason per view so cleared retained tokens, source changes and controller mismatches can be distinguished.

- Gate the F7 timeline binding on the panel state (v41): the scene graph exists outside the pause menu, so the first binding sent F7 into the recapture branch and produced `command=1 ignored outside paused inventory`. `PanelMonitor::is_menu_open()` is now the discriminator — F7 opens the 20-second timeline window outside the panel and recaptures inside it.

- Bound the source timeline to an explicit window (v39, rebound to F7 in v40): the second run lasted 18 seconds and still lost the equip sequence because the source graph cycles `bVoiceReady`/`BlendJump`/`bIsInMT`/`iLeftHandEquipped` and the node count on a roughly 1.2-second period. F7 now opens a 20-second window when no panel is open (inside the panel F7 keeps its recapture meaning — F9 was dropped because the game reserves it for quick load), the window closes by itself, and the line budget is 800. That period is also a constraint for G3-B: replay must not treat those cyclic variables as equip events. See the [contract](docs/independent-behavior-graph.md#g3-a-contract).

- Settle the source timeline and drop the per-frame node hash (v38): the first run spent all 200 lines in 8.5 seconds on `TimeDelta` and a node hash that changed every frame, so the equip sequence was never reached. A variable is now written only after its new value survives three consecutive samples, and the node line keeps the count alone. Same read points, same read-only guarantee, same budget. The same run also showed that the candidate holds 652 word variables while the source holds 463, so G3 replay must map by name rather than index. See the [contract](docs/independent-behavior-graph.md#g3-a-contract).

- Verify the opt-in display stepping in game (v33, AE 1.6.1170): one uninterrupted 120-call window measured 120/120 sign-invariant candidate motion at 1e-6, 109/116 actual node assignments with `109+7+0=116` and zero duplicates, an unchanged source pose and continuous on-screen motion. The corrected gate readers report `active/children=true` and 116 output records at every checkpoint, so the v30 `children=0`/`numData=62016` readout is retired as a misread object. Window/source boundaries, loaded teardown and G3–G6 remain open.

- Fix opt-in independent-graph preview stepping by sharing the complete batch update and restoring its temporary world context. Reject stale source/controller views, reset measurement windows per request/recapture, and report corrected graph reads, sign-invariant candidate motion and actual bone assignments. Defaults stay off; focused offline regressions and Release builds pass, and the [v33 game run](docs/independent-behavior-graph.md#g2-v33-run) confirms the bounded display window.

- Fix preview equipment-removal context and combine one-handed weapon idles with a torch arm layer; improve bounded animation diagnosis.
- Preserve typed transitions across effect refresh and admit only proven historical IED dependencies. Remaining Smooth selection, axe flash and bow/crossbow visuals require game verification.
- Fix torch preview candidates being rejected solely for the core current-weather flag condition,
  preserving OAR's native weather answer and negation.
- Fix left-shield removal with a retained right weapon selecting Equip instead of Unequip; retain
  the former core equipment query context while its shield transition loads.
- Fix mixed-case Smooth annotation tags being rejected before native DAC processing; preserve
  whole-event safety checks and the original event spelling passed to DAC.
- Reject DAC marker effects with attached scripts by inspecting their contributing MGEF definitions,
  without mistaking VM handles for script attachments. Reject unavailable or unsupported definitions
  before dispatch, including compressed target records, and keep override provenance in the cache.

- Fix preview placement drifting between actions: calibrate the copy root from the vanilla
  MT_Idle base at capture, before the action pose is written, instead of from whichever action
  first drove joints. An Equip/Start entry used to calibrate against the weapon Loop and a plain
  entry against a per-session random standing replacement, so every capture froze a different
  root and the figure jumped between actions. The anchor probe showed clips carry no
  root/pelvis translation; the COM difference is authored pose, not placement.

- Fix previews repeating an idle entry motion and keep animation requests isolated per actor.

- Historical panel entry action mirroring (folder ordering superseded above; `oar-idle3`, round 18, 2026-10-17): the panel no longer re-asks
  OAR what to play when the inventory opens. The clip the player was IN at the open edge (its
  running binding index and local time) is recorded once and replayed from that phase as the
  entry action — a draw continues as Equip → Start → Loop, a started sequence member hands over
  to its Loop sibling, a sheathe lands on the standing idle. The sibling lookup matches the
  sequence FAMILY (combat/non-combat variants under one project folder do not cross), the entry
  sample excludes hand-layer clips, and a Start member without a registered Loop sibling dwells
  instead of re-asking the frozen phase every cycle. Framing calibration waits for the idle the
  move hands over to. Both Release configurations build; `snapshot_transform` 38 checks pass.
  Game verification remains pending.

- Historical idle sequence phase and entering stance (folder ordering superseded above; `oar-idle3`, round 17, 2026-10-17): decide the
  entering stance from the draw/sheathe moves in the recent world clips (entering during
  `1HM_Equip` used to answer sheathed and show MT_Idle; entering during `1HM_Unequip` used to
  answer drawn and show a weapon idle), and let the panel advance a pack's sibling-folder idle
  sequence by name (Start → Loop, and Equip → Start → Loop after an in-menu weapon swap) instead
  of re-asking OAR, whose phase conditions are frozen on the paused source's magic effects. Packs
  without the sibling layout keep the ordinary OAR path. Both Release configurations build;
  `snapshot_transform` 37 checks pass. Game verification remains pending.

- Panel animation scheduling (`oar-idle3`, round 16, 2026-10-17): record an in-menu weapon swap
  before the request reads the equipment (the sheathed proposal was discarded as stale), use the
  frozen world-clip preference only while it still belongs to the current weapon family, pin a
  mid-sequence dwell across playback acceptance and restart sequence progress on activation for
  every scope, integrate the measured crossfade with a real clock instead of a 10-frame-sampled
  1/60 s step, carry the weapon attachment hint on the loop/transition re-queries and make the
  prefix fallback truly longest, bound the name-catalogue cache, keep the raw placement when
  calibration cannot measure, and stop a same-equipment recapture from discarding an in-flight
  request. Both Release configurations build; `snapshot_transform` 35 checks pass. Game
  verification remains pending.

### Changed

- Panel idle selection and fast weapon changes (`oar-idle2`, 2026-10-16): support preview-local
  Random conditions, cached weighted variants, sequence/PlayOnce history committed on playback,
  and loop reevaluation settings. Each idle layer follows its own cycle. Equipment changes replace
  older pending requests immediately; the newest loaded target interrupts from the visible pose,
  and stale transitions cannot return to an old weapon idle. Retain unchanged clips and clocks on
  refresh. Three additional OAR getters are verified for both versions; no additional detours or
  display Actor are introduced. Game verification remains pending.

- Independent panel animation (`oar-preview1`, 2026-10-16): replace the global standing window and
  captured-running-clip fallback with exact-project, standing-base OAR queries on private clips.
  Correct the 3.1.6 evaluation RVA (the old address was GetReplacements), provide verified query
  profiles for 3.1.6 and 3.2.1, and guard candidates against unsupported conditions. The copy route
  loads its own bindings, owns variant state, coalesces gear refreshes and returns from finite weapon
  transitions to standing idle. Earlier window/captured-clip entries below describe superseded builds.
  Game validation of this new chain remains pending.

- Display route: the panel now draws the DETACHED COPY of the player's real third-person 3D, and
  no display puppet is spawned any more (the copy-route decision, 2026-10-15). The copy carries
  every body-morph and outfit-transform state other mods gave the player (OBody, outfit plugins),
  so the old clone-Actor route's per-plugin compatibility work and its OAR hook adaptations are
  no longer on the critical path. Every panel open now RECAPTURES the player's current 3D (the
  visible outfit may have changed since the last visit) and enables the draw on success - the
  panel shows the figure without pressing F7 first. F7/F8 stay as the manual recapture and
  draw-toggle overrides; F6 (R01 same-Actor direct draw) stays as a diagnostic, default off,
  and the puppet code itself is retained for those diagnostics. The figure's motion comes from
  the captured clip (the engine's own OAR-decided clip at capture time) with the procedural
  idle as fallback; hooking OAR for state-specific idles and per-equip animations on the copy
  is the next work package. Build identity: `CharacterPanel-scopy-copy-primary-98a8b87507+
  dirty-cl94faaed0c6-20261015T170011Z` (DLL SHA-256 `ff3045659365d28bbabbcec02bd6b7e76a70f8b889dff4c85da17a073ed46d4d`),
  deployed and hash-verified; game validation pending.

### Added

- Panel pose pre-condition on the OAR hooks (state-idle v3, user): the panel figure answers OAR
  as a STANDING figure in the player's weapon stance. While the panel is open, every condition
  OAR evaluates for the figure's original is answered with the standing pose - sneaking,
  swimming, attacking, running, in-air and mounted all answer no, and the weapon state answers
  from the weapon form the player actually holds (one-handed, two-handed, bow, dual-wield,
  spell, staff, torch, shield), so OAR selects that stance's idle by its own rules. Closed, the
  engine's original path answers again and nothing in the world changes. The form is decided by
  a pure rule over the two hands (`oar::weapon_form_from_hands`), and the panel's regression
  `panel pose answer` covers both the standing answers and the form split (17 checks).
  Not a sampled pool: an unsampled animation could never be played from one, and the replay of
  sampled clips was retired with the pool statistics. Build identity
  `CharacterPanel-scopy-copy-primary-ab128d95db+dirty…T182842Z` (DLL SHA-256
  `8731e5eaa3a2637d379609ad6859a400e4c59b46207888f4ce80bde25a76878c`), deployed and hash-verified;
  game validation pending (panel opens in sneak/swim/combat, weapon-form stances).

- State-idle capture (copy-primary, second package): the figure the panel shows now plays the
  state the player was in at capture. The capture prefers the strongest running clip whose name
  carries the engine's idle family marker ("idle", case-insensitive - sneakIdle, 1hm_idle, the
  hand-idle family) - the engine's own OAR-decided idle for the current state (jumping, sneaking,
  swimming, combat, weapon-drawn); with no idle-named clip running (mid-locomotion) it falls back
  to the strongest clip and logs the pick (`pick=idle-named|strongest-fallback`). The capture log
  now carries the engine actor state beside the clip (`state[sneaking=… swimming=… sitting=…
  weapon-drawn=… attack=… knock=… life=…]`), so a reader can tell which state the clip belongs to
  without inferring from the clip name. Build identity
  `CharacterPanel-scopy-copy-primary-98a8b87507+dirty…T170011Z` (DLL SHA-256
  `b504d7665084fe94e66e36936177a3c7672de34e64a9736a9ce2c23cd255a2f6`), deployed and hash-verified;
  game validation pending (sneak/swim/combat opens, mid-locomotion opens).

- A dedicated `src/animation/oar` integration module for the Nexus OAR binary. It verifies the
  DLL/PDB build identity and entry bytes before installing preview-scoped condition and
  numeric graph-variable hooks. Audited queries retain source identity/equipment while
  projecting supported standing-preview state; unsupported conditions are reported.

- OAR build profiles: the module now carries a verified profile for Open Animation
  Replacer 3.1.6 (the build installed in the test environment) beside the 3.2.1 one, and
  installs on whichever profile matches the loaded DLL's plugin version, PE
  timestamp/image size, CodeView identity and both entry bytes. The 3.1.6 RVAs and field
  offsets were read from the installed DLL and its PDB and confirmed by disassembly;
  the public condition API header is byte-identical between the two builds, and
  `Conditions.h` only gained a new condition class, so no interface the module calls
  changed. A refused install now names the rejected version fields.

- Preview projection for OAR's own decisions: `ScopedEvaluation` gained a `kProjected`
  mode (unknown conditions keep the engine's answer instead of making the query
  unsupported) and a null-clip scope that covers every clip of the scoped source. The
  puppet's paused graph step now runs inside that scope, so OAR's replacement decision
  for the figure is evaluated with the standing preview while identity and equipment stay
  the figure's. The panel reports `evaluated`/`overridden` counts, where a zero
  `evaluated` means the hook was not reached rather than that nothing was evaluated.

- Projection evidence instrumentation (F11): every projected answer is compared against the
  engine's own answer for that same evaluation, and the disagreements are tallied per
  condition, so `differing` and `top=[Name D/M, ...]` state directly whether the projection
  changed an input OAR decides on (a standing figure at rest is expected to differ rarely).
  F11 switches the projection arm; both arms record the clips they produced and the log
  prints `SCOPY OAR ab projected-clips=[...] plain-clips=[...] sets-equal=yes|no`, which
  answers "did the projection change the selection" without the operator timing anything.
  Game-tested on `oar-316`: hooks installed on 3.1.6, 186272 evaluations / 26877 overrides
  over 2640 paused steps, zero crashes; the figure stayed in its standing idle, which the
  differential now attributes to the engine's answers already being standing values.

### Fixed

- A two-handed weapon on the figure could crash the game (crash 2026-10-15 22:45:27, "BiPolar Blade",
  `BipedAnim` reading a null biped object). The hand reconcile compared each hand entry separately,
  but the engine reports a two-handed weapon in whichever entry it last touched and the reading flips
  between polls: the entry the player does not use for it read as "something to take off", so the
  figure was unequipped and re-equipped every frame (104 reactions in ~4 s of log), and the prune
  took the same weapon away while the hand poll was putting it back. The pair is now normalised with
  the two-handed weapon (both entries hold it), the removal and equip rules ask "is this item held"
  instead of "does this entry hold it", the prune is held off on a frame where the source's own hand
  reading just moved, and the strip takes a weapon reported in both entries away once. A churn guard
  bounds the damage a future logic gap can do: more than four hand actions in ten polls holds the
  hand reconcile off for ninety polls and says so, instead of hammering the equip manager every
  frame. New regression `two-handed hand state`.

- The first game test of that fix crashed on the first dressing instead (crash 2026-10-15
  23:02:27, "Crimson Tide", `CharacterPanel-scopy-2h-fix` build): the removal loop now asks
  `hand_removal_needed`, and an EMPTY hand has nothing to take off even when the player holds a
  weapon - the equip loop adds it below. The replaced guard asked "is the wanted item held", which
  an empty hand answers no, and falling through put the null item into `slot_for`'s inlined
  form-type check (`cmp byte ptr [rbx+0x1A], 0x29`, offset 0x1A is `TESForm::formType`, 0x29 is
  Weapon) at the `UnequipObject` call, with `RemoveItem(nullptr, ...)` waiting right behind it.
  `slot_for` now also tolerates a null object, mirroring `hand_two_handed_weapon`. The regression
  test `two-handed hand state` covers the removal decision, including the empty-hand case. Fixed
  build: `CharacterPanel-scopy-2h-fix2-f63f41ca49+dirty-cl94faaed0c6-20261015T151620Z` (DLL SHA-256
  `aa343c0bd5c5ace6904aaaa3cede639a2e81a8bd7a7f2b7c1273c12dac8b2dff`), deployed to
  `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` and hash-verified; game validation pending.

- The 2h-fix2 game test still churned and then crashed (2026-10-15 23:55:47, "Dwarven Battleaxe",
  engine `BipedAnim` null on a re-issued equip), because the both-hands DETECTION never fired for
  any two-handed weapon: it compared `GetEquipSlot()` against `DEFAULT_OBJECT::kEitherHandEquip`,
  but that default object is the ONE-HANDED either-hand slot (vanilla 0x00013f44), while every
  vanilla two-handed melee weapon and bow carries the two-handed slot 0x00013f45 (measured across
  all 2484 Skyrim.esm WEAP records: types 5/6/7 use 0x13f45 exclusively). The un-normalised pair
  kept reading as "take it off this hand / put it on the other" while the engine - and this
  install, which also legitimately equips two-handed weapons into the left-hand entry - recorded
  the weapon wherever it last touched it. Detection now reads the weapon's animation type
  (`kTwoHandSword`/`kTwoHandAxe`/`kBow`/`kCrossbow`; staves excluded - vanilla carries them on
  one-hand slots), the normalised pair is invariant under the entry flip so the reconcile does not
  act on it at all, the both-hands weapon is equipped ONCE through its own two-handed slot from the
  first pass (the left pass is skipped), and the equip check accepts it from either entry. The
  hand the weapon sits in is left as the engine recorded it, and no hand-specific animation is
  forced for the left-hand state - the draw-state push only says "a weapon is held". Fixed build:
  `CharacterPanel-scopy-2h-fix3-f63f41ca49+dirty-cl94faaed0c6-20261015T161051Z` (DLL SHA-256
  `8e29ce5aac60b3cb300cb4465414a7041c3fd63291180e7a67980a9484264194`), deployed and hash-verified;
  game validation pending.

- UBE transparency trial (`ube-alpha1`): both figure sources now draw opaque/cutout geometry before
  sorted alpha surfaces, read alpha testing from each mesh's NiAlphaProperty, and request native
  per-geometry blend setup. Blended surfaces use read-only depth. The studio restores the engine's
  material-state cache and all VS/PS constant-buffer bindings, including D3D11.1 subranges.
  Both Release configurations and local checks pass, including a WARP check of constant-buffer
  subrange restoration. **Game result: the brow and eye-shadow mesh-sized black patches are gone and
  the makeup renders correctly (user-confirmed 2026-10-15); rotation, head-equipment changes and the
  post-panel game view remain unconfirmed.** Package identity and reproduction steps are recorded in
  `docs/ube-transparency-test-2026-10-15.md`.

- Review fixes (2026-10-15): non-identity animation bindings now keep track and bone indices
  separate and sample only the animation's real track count. Recapturing a graph invalidates
  the puppet's cached target nodes even when the active generator is unchanged.
- New games now request a puppet, parking uses the current Actor's position across reloads,
  and post-dressing assembly must bind and settle before parking (with the existing timeout).
  Final-pose verification retries unavailable input without accepting an unverified mapping.
- Hand mirroring preserves left/right slots and dual-wield quantities, removes both old hands
  before replacement, and retains an item still wanted in the other hand. Two-handed weapons
  keep their two-handed equip slot.
- OBody shape reconciliation now detects target-only sliders and an empty source preset.
  Clearing stale morphs is reported as a change even when no replacement values are copied;
  morph keys belonging to other plugins remain untouched.

- AFP10g the record's own kit reaching the figure AT ALL (the root the AFP10b-f rounds kept chasing
  symptoms of): the puppet's base is a `CreateDuplicateForm` of the player's NPC record, and the
  duplicate carries the record's container - the reference's 3D load serves it into the inventory and
  the engine's best-fit equip dresses a no-AI figure with whatever it finds. The spawn now cuts the
  source the way the copy route has since its first session: both outfit fields nulled and the
  duplicate's container cleared (`TESContainer::ClearDataComponent`) before `PlaceObjectAtMe`, so
  nothing arrives for the birth strip, the dress strip or the prune to chase - those stay as guards
  against other sources (scripts, mods) and keep their naming logs.

- AFP10g the prune never running while the world ran: its cadence gate read the same counter the
  worn-sync block incremented and reset at the same threshold, so the prune's `>= Worn_Poll_Period`
  was unreachable outside the pause (the shared counter sat at most at 29 at the prune's check).
  The two reconcilers now hold independent counters; the ammo reconciliation below rides the worn
  cadence.

- AFP10g the figure's quiver following whatever the engine's best-fit picked from residual arrows:
  the quiver's model comes from an actor's CURRENT ammo, which neither the biped walk nor the hand
  poll carries, and the prune's blanket ammunition exemption kept the record's own stack alive as
  the only candidate. A new `equipment_sync_ammo` equips exactly the player's current ammo on the
  figure (added to the inventory first, the old one taken off by the same unequip-and-remove route
  as the hands), and the prune's exemption is narrowed to "the source's current ammo" - every other
  carried round is pruned like any unused item. New line `SCOPY PUPPET ammo wanted=… equipped=…
  removed=…`.

- AFP10g appearance gear silently dropping off the figure: `Body_Slots` was missing `kHair` (wigs),
  `kModBack` (cloaks, backpacks, back sheaths), `kModMisc1/2` and `kModPelvisPrimary/Secondary`
  (belts, skirt pieces), so anything a mod put there was neither mirrored nor stripped by the slot
  walk. The list now covers every armor-bearing slot except the shield (the hand poll's territory)
  and FX01 (effect attachments).

- AFP10g the last `GetInventoryChanges(true)`: the hand-equip path still asked for the figure's
  container without initialising it - the one read the AFP10f sweep missed. Every read initialises
  now.

- AFP10g the clone route dressing from a hardcoded player: `detach_graph` walked the player's worn
  set regardless of which actor the clone wraps. The source is stored at construction and the
  dressing walks it; for the current player-only use the behavior is identical, and a future
  non-player clone dresses from its own subject. The clone's spawn also nulls `sleepOutfit` next to
  `defaultOutfit`, matching the puppet's source cut.

### Fixed

- AFP10f the record's own starting kit surviving on the figure, and a weapon that was never replaced
  once equipped (both reported 2026-10-14). Two causes, both about who else equips and what "removed"
  means. The birth strip asked for the inventory with `GetInventoryChanges(true)` - no initialisation -
  and a freshly placed Actor's container does not exist yet, so it reported `items=0` on a shell that
  was measurably wearing an iron war axe (00013790, the record's kit); the kit then arrived, and the
  engine's own best-fit equip puts a carried item straight back on a figure that has no AI to decide
  otherwise. Strips and reads now initialise the container, and a new prune removes EVERY carried item
  the player is not wearing or holding (ammunition exempt: it is what a drawn bow renders its quiver
  from) - after a prune there is nothing left for the engine to equip. The hand replacement also
  removes the old weapon from the inventory instead of only unequipping it, and reports the removal
  itself rather than the equip API's opinion: the same session logged `removed=0` on every replacement
  while the old weapon stayed in the hand.

- AFP10e the panel figure not moving until F4 had been pressed (reported 2026-10-14): with the drive
  state the session started on, the graph was advanced but nothing was applied, so the figure stood
  still until the key walk reached an applying state. Fixing that by defaulting to the applying state
  is not possible either, because the two requirements conflict in time: the proof that the mapping is
  right needs a frame the engine generated AND applied, which only an UNFORCED running world provides,
  while a PAUSED panel produces no pose at all unless the generate is forced (`pose-empty`, AFP3). A
  new startup drive state resolves it per frame - forced while paused, unforced while the world runs -
  so the panel animates the moment the inventory opens and the proof still has its unforced frame. The
  F4 cycle keeps its five fixed-flag diagnostic states and now starts from this one.

- AFP10e the initial equipment going missing (reported 2026-10-14 as a regression: clothing could not
  be swapped the day before, but weapons could). The hand mirror skipped both slots whenever the
  player's own hands had not moved, and the strip added in AFP10c empties the puppet's hands without
  the player's hands changing at all - so after dressing, the weapon the strip had just taken off was
  never put back until the player swapped weapons by hand. The decision is now made against what the
  puppet actually HOLDS rather than against a cached form id (the same stateless shape the worn-body
  reconciliation already had), and dressing clears the cache so the next poll re-equips and re-pushes
  the draw state.

### Fixed

- AFP10d the panel drawing nothing with direct draw on (`SCOPY R01 fallback to the copy
  reason=not-framed` on every frame, reported 2026-10-14 as "F6 no longer brings up the panel"): the
  framing needed a bound, the live bound is measured from the skin palette, and the engine allocates
  that palette only when it sets a geometry up - at draw time. The shell arrived with 44 skinned
  geometries and `allocatedSize == 0` on every one of them (so `bound-nodes=0/0` with no null skin data
  and no null bones array), the palette bound was therefore invalid on every frame, `begin_frame`
  framed nothing, and the draw that would have allocated the palette never ran. The zeroed fade guard
  is what started it: the shell is never drawn by the world, and the 0.05 of the earlier rounds had
  been what let the engine touch it once. The framing now falls back to the tree's own node cloud
  (`figure_node_cloud_bound`, live node worlds, collision helpers excluded) and returns to the palette
  bound by itself as soon as a draw has allocated it; the frame line reports `bound-from=palette|nodes`.

- AFP10d the panel needed a key press to show anything: R01 direct draw now defaults ON, so opening the
  inventory shows the figure with no F7 capture and no F6 press. F6 still switches between the puppet's
  own 3D and the detached copy.

### Added

- AFP10d two more pieces of evidence for the equipment reports, because a picture cannot name a mesh: the
  dressing step logs the shell's geometry names (`SCOPY PUPPET shell names=[...]`) and a strip logs the
  names and form ids of the worn items it took off (`unequipped=[name(formid) ...]`), not only counts.

### Fixed

- AFP10c the figure's equipment stacking, and a bow/shield/axe nobody equipped (both reported
  2026-10-14): a freshly placed Actor is not empty - its base record carries what it was authored
  with, and this puppet's base is a copy of the PLAYER's NPC record - so the shell arrived wearing its
  own default items on top of whatever the mirror put on it. Those are also what the panel showed when
  it was opened during the assembly window, before the dressing step ran. The puppet is now stripped
  (every worn body item, both hands, every inventory entry) once when the reference is born and again
  before dressing, and the strip logs `items=/removed=/unequipped=/inherited=[name(formid) ...]` so
  whatever it came with is identifiable from the log rather than only from the picture. Worn-body
  equipping also stops adding a second copy of an item the target already carries, which was the other
  half of the stacking.

### Fixed

- AFP10b weapons appearing on the panel figure that the player was not holding (reported 2026-10-14 as
  "an axe, a shield and a bow"): the hand mirror only ever equipped. It pushed the player's hand item
  onto the puppet on every change and had no path that took one off, so every weapon the player drew
  during a session stayed on the figure for good - the axe from the first swap, the shield from the
  next, the bow after that. The mirror is now exact in both directions: the puppet's current hand item
  comes off first when it differs from the player's, and the equip line reports `equipped=`/`removed=`
  so the next round can tell the two apart.

- AFP10b the body-shape copy no longer carries other systems' morph state: the first AFP10 session
  copied every morph the player had (193 of them) and the puppet came up with a bow and a dagger
  attached, because the copy had also carried XPMSE's own "which weapon type is equipped" morphs
  (`XPMSEAABase_axeeqp`, `_boweqp`, `_dageqp`, `_1hmeqp`, `_2hmeqp`) onto an actor whose attachment
  nodes then believed those weapons were there. A morph store is shared by every system that uses one,
  so the copy is now restricted to OBody's own key - the key OBody writes its preset to, and where the
  body shape this route exists for actually lives. Everything else is counted and reported as
  `skipped=`, never written.

- AFP10b the assembly line printed once per frame after a successful early park: the window block
  re-entered on every frame below the frame cap, re-printing the line and re-parking the figure until
  the cap was reached. Parking now ends the window on every path.

### Fixed

- AFP10 the first seconds of every session: the puppet's assembly window pinned its fade to 0.05, which
  is a five-percent ghost standing inside the player's own body. The window cannot simply be removed -
  the reference has to be beside the player when the character graph loads, because the engine picks
  the character LOD by reference distance at load (run 104: a reference born at the park depth
  assembled a low-detail graph with missing parts and a stripped skin). So the window is now spent
  invisible: the fade guard is 0 (this route has no `numMatrices` gate that a zeroed BSFadeNode would
  starve - the puppet's binding is read from its tree), and the window CLOSES EARLY as soon as the
  shared census reports the shell assembled, fully bound and unchanged for ten frames, with a 20-frame
  floor so the reference is demonstrably beside the player at load and the old 240 frames only as the
  backstop for a shell that never reports ready. The shared `FigureAssembly` census moved into
  `figure_audit` and the copy route's settle gate now uses the same one.

### Added

- AFP10 clothing on the panel figure: the driver only ever looked at the player's two hands, so the
  puppet carried weapons and nothing else. Worn BIPED body items (armor, clothes, boots, jewelry) are
  now mirrored through a shared `equipment_mirror` module - the copy route's own worn-item walk moved
  there, so "what is the player wearing" has one answer - dressed while the reference is still beside
  the player (the order the copy route validated, because a biped change is applied asynchronously)
  and then reconciled every frame while the panel is open and every 30 frames while the world runs.
  The reconciliation is stateless: both inventories are read fresh, so a missed frame cannot
  desynchronise it and nothing survives a 3D generation change. Weapons stay with the hand poll.

- AFP10 the player's body shape on the puppet. OBody NG hands a freshly placed NPC a *random* preset
  every session (its log: `No preset defined for this actor, getting it randomly` -> `Preset Mia UBE
  will be applied to CharacterPanel Puppet`, a different one next session), which is why the panel
  figure's body never matched. Shape here is mesh morphs, not node scaling - all 639 `.osp` slider
  sets on this machine carry zero bone/scale definitions and BodySlide presets are realized as `.tri`
  vertex deltas - so the copy uses the same route OBody writes with: RaceMenu skee64's
  `IBodyMorphInterface`, reached through SKSE's interface exchange (`BodyMorph`, runtime version 5).
  The module mirrors that interface's vtable order locally, checks the reported version before the
  first call and refuses an older layout rather than guessing slots. The sync compares the source's
  morphs against the target's and only writes when they disagree, clearing the target's OBody keys
  first so the random preset cannot survive the copy, then re-applying the vertex buffers in the same
  call. It runs at dressing time and is re-checked on a slow panel cadence, so OBody re-shaping the
  shell or the player changing their own body is repaired rather than assumed away.

### Fixed

- AFP9j interactable shell beside the player: `Actor::SetPosition`'s second parameter is
  `a_updateCharController`, and passing false moved only the reference - the collision and activation
  target stayed beside the player (perceptible through the activation prompt while the figure itself
  was invisible), and the engine kept deriving the 3D from the controller's stale position, which is
  the root of the earlier "world and local writes are reverted every frame" finding. The park now
  requests the warp when the reference actually moves (`park-3d warp=1`), false otherwise so the
  per-frame re-assertion stays cheap. The historical route parked this way and measured interaction as
  unreachable.

### Fixed

- AFP9i intermittent "only the axe" frame: `NiSkinInstance::frameID` is the engine's "skin already
  computed for this render frame" gate, and when the engine had computed it BEFORE the studio
  placement and the applied pose were written, `SetupGeometry` kept those matrices - the skinned body
  was drawn at the older (parked) bone worlds and vanished while a rigid attachment like a weapon
  followed its node and stayed visible. The readback images show a complete, correctly framed figure
  and a lone axe alternating within one session. The batch now invalidates that id for the tree
  (`invalidate_skin_cache`, nothing written to `boneMatrices`) so the engine recomputes from the bone
  worlds this frame wrote - which also removes the one-frame lag on an applied pose.

### Fixed

- AFP9h framing reference: the studio placement was centred against `figure_skinned_bound`, which
  reads geometry `worldBound`s - a cache the engine refreshes on its own schedule, measured still
  carrying the pre-park world position while the bones had already moved to the anchor. Centring on
  that stale centre threw the figure ~2820 world units deeper than the anchor, which is why the first
  readback image showed it small, dark (far from the studio rig) and off-centre while `center-d`
  reported 0. Framing now measures `figure_skeleton_bound` (live bone world positions, palette slots
  filtered through the tree's own node set) and logs both bounds side by side. The park is also
  re-asserted on paused frames unless this frame's framing owns the transform, because the menu
  renders the world behind it and the engine's per-frame derivation otherwise puts the clone back
  beside the player the moment the panel opens.

### Added

- AFP9g forensics: the studio target can be read back to a TGA under `.../SKSE/CharacterPanel/`
  (`r01-panel-NNN.tga`, first drawn frame and every 300 after, capped) - the facility this project
  used before 172984a and dropped, which turned every later round into an external frame-capture
  conversation. Alongside it: an `R01 census` line (geometry/skinned counts, and the first skinned
  geometry's skin `frameID`/`num-matrices`/first-bone world and bound) and a `park-3d ref-now`
  read-back that settles whether `SetPosition` moves the reference at all. The park is now re-asserted
  every unpaused frame instead of every 60, because the engine re-derives the transform each frame and
  wins over a one-shot write.

### Fixed

- AFP9f transform layer: a live actor's subtree is re-derived from its root's `local` every frame, so
  a world-space write lasts one frame (measured: the park's world write was undone 1.3 s later, which
  is why the clone stayed beside the player), and the skin state is computed during the graph update,
  so a placement applied afterwards reaches rigid attachments only (the axe followed, the skinned body
  did not). Placement now writes the root's `local` together with the derived worlds, and R01 applies
  it in `begin_frame` BEFORE the puppet's graph update, restoring it in `end_frame` after the draw
  branch on every path.

- AFP9e visible clone beside the player: `PuppetDriver::park()` moved only the reference position, and
  this actor's 3D is never re-derived from it (measured: twenty unpaused seconds with the reference
  parked while the root's world stayed at the player), so restoring the fade left a fully visible
  clone standing next to the player - and it is also why the F6 figure projected arbitrarily, since
  the tree never left the player. The park now moves the 3D subtree by the same world offset,
  computed against the absolute target so repeated parks cannot stack, and logs
  `park-3d ref-t / world-t-before / world-t-after / moved-z` as the reconciliation line.

- AFP9c F6 off-screen figure: the R01 framing wrote the puppet root's `local` and cascaded, but a
  downward pass recomputes the children from the object's own world and never moves the object - so
  the figure stayed at its park depth (`restore world-d=(0,0,0)`) and every vertex projected far
  outside the frustum (NDC ≈ (236, 290), the user's RenderDoc reading). The framing is now a
  world-space delta applied downward (`figure_world_delta` reusing the copy route's own solve plus
  `apply_world_delta_downward`), which touches no local at all, and it asserts itself with a
  `SCOPY R01 frame … center-d=` line. The measured bound is refreshed before framing, the root's
  bound cache is written back on exit, and a unit case covers the delta.

- AFP9b black panel: the figure draw and the composite shared one D3D state-capture scope, so the
  composite bound the studio texture as a sampler source while it was still bound as the output
  target - D3D11 silently unbinds a resource used as both, and the panel sampled nothing while the
  studio target itself was correct (both routes affected; the user's RenderDoc reading was right).
  The composite now releases the output target before binding its source, the draw and the composite
  use separate capture scopes again, and the composite reads the bound source back once and logs
  `SCOPY COMPOSITE src/bound/match`. The audit's unreachable naming was also wrong (a negative bone
  table target is not unreachable - the name bridge writes those slots; 61 names were printed against
  a per-frame count of 6) and now repeats the write's own resolution order with a `reconciles=` check;
  the R01 restore check no longer fails on the worldBound cache the engine refreshes during pass
  setup (world drift was already 0), which also removes its per-frame log flood.

- AFP9 first-test crash (`EXCEPTION_ACCESS_VIOLATION` at `CharacterPanel.dll+0x2B78A`, a name read at
  `0x1010`): the coverage audit's palette walk dereferenced `NiSkinInstance::bones` entries as nodes
  while reading their names. The walk now collects pointers only - the shape the probe has run for
  weeks - and a slot is classified through the node the caller resolves itself in the actor's tree,
  which also reports whether the name resolved at all (`resolved=x/y`). The probe's private palette
  walk was deleted in favour of the shared one, so the memory rule exists once. Field logs are kept
  in `docs/diagnostics`; verdict in `docs/afp9-verdict-2026-10-13.md`.

### Added

- AFP9 package A: the application stage reconciles its coverage ledger every frame
  (output-slots = attempted + written + unreachable + unprocessed, the A2 mark carries it) and, when
  the gap changes, names the unreachable and unprocessed slots against the animation skeleton and
  classifies each by the puppet's own skin palette (necessary / helper / weapon-attachment; an
  unbound palette reports `unbound-palette`, never "nothing is necessary"). The audit names and
  classifies only - it does not extend the write past the mapping table, because the `Weapon*`
  slots AFP8 measured as graph-driven residue would move the weapons if written blindly.
- AFP9 package B, R01: an opt-in (F6) same-Actor direct-draw path. When enabled the panel draws the
  puppet's own `Get3D(false)` tree through the shared pass seam instead of the detached copy, with a
  provenance line (actor form, root, graph, generation), fixed-bone A4 (pre-pass) and A5 (post-draw)
  samples proving the draw does not overwrite the applied pose, and a temporary framing whose saved
  root local, world and worldBound are restored with a printed before/after reconciliation on every
  exit including failures. The puppet path falls back to the copy and logs the reason; physics and
  FaceGen stay out of scope (A3 = skipped).
- Shared audit helpers (`figure_audit.h/.cpp`): skin-palette names, collision-helper-filtered
  skinned bound, placement solve, and the slot-necessity classifier, unit-tested alongside the new
  coverage-ledger arithmetic in the focused regression target.

### Removed

- Remove retired M0 probe/prototype build targets, engine image dumping, S2 measurement controls,
  capture-time animation scans and the empirical track solver. Keep Actor assembly, animated
  scene-copy rendering and pending final-pose diagnostics; use F2 mark, F4 drive and F7/F8 capture/draw.
- Replace exploratory verification matrices with two focused regressions for snapshot placement
  and final-pose index-order calibration.

### Added

- Implement the same-Actor route's application stage: the behaviour graph's own final output pose
  written into the puppet's skeleton, which is the half the engine does not do while the panel owns
  the clock (HKX42 measured the locals frozen). It is deliberately strict. It writes only a mapping
  it has just verified against the engine's own applied node locals through the same scoring and
  decision the forensics print, and blocks with the reason when that fails - so verification happens
  while the world is RUNNING (the engine applies the pose then, which makes its locals the reference)
  and the write happens while the panel pauses the world; arming it while already paused reports
  `unverified-world-first` instead of guessing, and it never re-scores once it has started writing,
  because the locals would then be its own. Every node is resolved by name in the current Actor tree
  on the frame it writes, with the engine's bone table as a fallback; no node pointer is kept across
  frames. The write sets
  local transforms and recomputes the subtree's worlds once, the only path this codebase has measured
  reaching a draw. A2 reports what it did on every mark: the mapping, the generation, how many bones
  the last frame posed, how many pose indices the chosen mapping could not reach, and the worst change
  it made. F4's cycle carries `tick+apply` and `tick+apply+force`, with `step` as one declared dt plus one application,
  and the driver gains the single step that runs even while its gate is closed.

### Changed

- F6 is no longer documented as "gone": AFP9 binds R01 direct draw to it. F3 remains with the other
  mod that owns it. The package README's control table and the maintenance document carry the same
  contract.
- Move the AFP route's controls onto the available keys: F2 marks and F4 is
  the drive cycle; placement has no control entry. The earlier bindings were wrong or unwanted - F5 is the
  game's own quicksave (and this sink does not consume input), F11/F12 and NUMPAD1 were tried and are
  not wanted - and the scene-copy route gives up its old F2 idle / F3 rotate / F4 release bindings for
  them, keeping F7 (re-capture, which parks the previous copy) and F8 (draw it). F4 is designed to
  absorb the application stage rather than to need a fourth key: it becomes off -> tick -> tick+apply
  -> tick+apply+force -> step-once, which is R02's push/freeze control, R04's A/B pair and single-step on one key. The
  control contract now also states what deliberately has no key - birth, display and equipment
  mirroring are automatic, framing is a build-time preset, and physics and teardown belong to their
  own gates - because a small, declared control surface is what R00 asks for, not a key per behaviour.

### Added

- Tell the bones that block from the bones that do not. The calibration already names up to eight
  bones nothing reproduced, but R03/R04 need to know which of them a draw actually reads, so the
  outliers are checked against the skin palette `NiSkinInstance::bones` points at: a bone a target
  mesh reads cannot be skipped, while a helper branch or a cloth chain the panel does not draw can.
  `SCOPY FP necessary … necessary=N helper=N palettes=bound/skinned` carries it, and `palettes=0/N` -
  a figure the engine has never rendered, runs 49-52's shape - reports the classification as
  `unbound-palette` rather than as "nothing is necessary", which is the difference between an answer
  and a silence.


- Take the mark twice in one frame, which is the only way to ask whether our own tick moved the
  graph's output. `FinalPoseProbe::sample_before_tick` runs before the drivers tick and records A0
  (both output buffers' identity, element counts and reasons, and the node locals), and the report
  after the tick adds an `A0A1` line per subject: the pose change this frame produced, the node
  locals it moved, and `nodes-rebound` for a tree that was rebuilt between the two samples. A
  cross-mark delta cannot separate our tick from the engine having run in between; in a paused world
  only one of the two can move anything, which is exactly what R02 asks. A3 is written out as
  `physics=skipped` so the six observation points are complete until the physics round and an absent
  stage cannot read as a passing one.


- Split the studio's pass batch away from who owns the figure it draws
  (`src/render/studio/figure_passes.{h,cpp}`): a request (root, accumulator, context, target, states,
  light slots, whether to read the bound target back once) and the batch itself - bind and clear the
  studio target, walk the figure's geometries, generate passes through the UI3D accumulator, submit
  each one with the studio rig swapped in and restored, then put the engine's ledger expectation back.
  The returned statistics describe the completed batch; retired S2 callbacks are removed. `CharacterClone::draw` now
  keeps only what is specific to a detached graph it owns - ownership gate, `pose()`, the animation
  base stage, the light anchor, the census - which is exactly the split the same-Actor route
  needs to draw an Actor's own third-person 3D through the same proven path. Two orderings moved
  deliberately and are commented where they moved: the DIRTY_RENDERTARGET reset now happens inside the
  batch (later is only more thorough, and nothing between the old and new points generates a pass or
  changes a binding), and the set now precedes the census (which never read the flag).

### Fixed

- Compile the default (Actor-route) configuration again. `CharacterClone`'s per-frame summary named
  `SceneGraphCopy::instance().animation_driver()` unconditionally, while that class only exists in the
  `CHARACTER_PANEL_SCENE_COPY_EXPERIMENT` configuration and its header include is gated by the same
  macro - so the default build had not compiled since that log line was added. A stale `build/`
  directory, configured before several source files existed, had been hiding it: the first real build
  in that configuration surfaced it. The line is now branched by configuration (the experiment keeps
  its exact line, the default logs the same summary without the driver name), and
  `cmake --preset Release` + `--target CharacterPanel` compiles and links.

- Complete the R03 evidence the next session has to produce, and measure R01's other prerequisite
  before anything is drawn. The comparison is now over whole transforms in both spaces: `order=` line
  carries `beyond`, `max-scale` (the third space, measured and never gating, because a pose carries
  three scales where a NIF node carries one) and `outliers=`, which names up to eight of the bones
  that missed the bar - R03 asks which ones, since a helper branch can be skipped while a target
  mesh's own bone cannot. A second `world` line composes the same pose along the animation skeleton's
  own parent table and compares it with the nodes' worlds: the local and world spaces are asked for
  separately because they disagree exactly where the two hierarchies do, which is what a CME/helper
  branch is. Both rules are pure header code and the unit test now pins them - a shifted order is
  convicted and named, an absent node leaves the comparison instead of shifting it, a scale
  difference never fails a bone, a world translation offset is reported as max-position rather than
  as a failure, and a world *rotation* disagreement is counted and named even when every local
  matched. A `geoms` line reports the Actor's own geometry inventory read-only: how many geometries,
  how many skinned, how many have a shader property (and how many of those are `EffectShader` by
  class), and how many already have device-side `rendererData` - the field runs 49-52 died on, which
  a figure the paused world never rendered simply does not have.

- Report AFP1's first in-game verdict and give its one blocker a lever
  ([verdict](docs/history-animation.md#final-output)). The pose track is readable and
  fully validated in game (`reason=ok`, 116 elements, 0x30 elements, data at
  `tracks+448`), and its index order is the **animation skeleton** bridged by
  exact bone name: 105 of 109 comparable bones land inside one degree, against
  45 of 106 for the `boneNodes` order - two arrays, two orders, since
  `hkbCharacter::poseLocal` measured as `boneNodes`-ordered in HKX2. The live
  buffer must be identified by content, not by index: the live one sat in slot 0
  in every mark, but the two buffer objects themselves moved between marks. The
  blocker is the parked puppet: its graph generates an output with a **live but
  empty** pose track (`num-data=0`, `numAnimBones=0`) while the player's carries
  116, and 2103 manual ticks moved two bones by 0.040 degrees - so the park
  (v6.25, chosen so the shell is invisible and untouchable) had moved it out of
  the engine's animation scope, and HKX42's "the manual tick does not propagate
  the pose to the nodes" now reads as "there is no output to propagate". The
  empty count gets its own `reason=pose-empty` instead of reading as a bad
  count, and NUMPAD1 cycles the placement between that park and standing at the
  player with the fade pinned (inside the scope), with `placement=` on the mark
  line - the one variable that isolates whether the emptiness is the park or the
  graph itself.

- Open the same-Actor route (PRD 0.9 section 6.6) with its first instrument, AFP1. That route draws
  the puppet Actor's own third-person 3D and applies the behaviour graph's final output pose to that
  Actor's own skeleton while the panel pauses the world, so `src/character/final_pose_probe.{h,cpp}`
  measures the two things it rests on, read-only. One mark (F11) walks the graph's own output buffers
  (`BShkbAnimationGraph::generatorOutputs[0]` and `[1]`): the output object, the tracks blob, the
  pose track's header, its element size and its data span are each validated in turn and the step
  that stops the read is named, so a buffer this build does not understand reports which assumption
  broke instead of printing zeros. It then keeps a copy of the pose as plain numbers (read with
  `memcpy`, because the offset and alignment are exactly what is being validated), prints what that
  copy did since the previous mark, prints what the engine-applied node locals did, and scores both
  candidate index orders - the engine's `boneNodes` table and the animation skeleton bridged by
  exact bone name - against those node locals, one line per candidate. The bar is the HKX28 one
  degree, fixed before the package ships. The player is sampled beside the puppet as the reference,
  because the engine drives that graph and those nodes itself. `PuppetDriver` gains the Actor
  reference, the manual tick's gate (F12, R02's push/freeze control, which keeps the clock fresh so
  unfreezing delivers one step rather than a catch-up) and its tick count. Nothing here writes a
  skeleton, detaches a graph, disables an Actor, resets a bind pose, resamples a clip or draws:
  AFP2 is where the measured order and buffer become an application and a draw.

- Make the same-Actor route's mapping decision a tested rule instead of a printed number, and read the
  camera chain the studio draw actually renders through. `score_pose_order` and `decide_pose_order`
  are inline header rules over plain data (the pose copy and a per-index `NodeLocal` table, whose
  `present` flag keeps an index without a node from being dropped and shifting the whole comparison):
  they score one index order against the node locals the engine applied and refuse to let it drive a
  skeleton unless at least 8 bones were comparable and 90% of them landed inside the HKX28 one-degree
  bar, with `no-candidate` / `insufficient-compared` / `below-ratio` named as the block. AFP2's write
  gate is that same call, not a second copy of it, and the unit test now convicts a shifted order,
  blocks a perfect score over four bones, and proves a ratio tie goes to the candidate that compared
  more bones. The mark also reports the camera chain: `NiAccumulator::camera` (the camera the
  accumulator generating the studio's passes was started with) against `ui3d->camera` and the world
  camera, the camera's view axis and projection frustum, and where the subject's own 3D root sits -
  world position, distance along the view axis, distance off it, fade and bound. The pass submission
  takes no camera and writes no projection, so that identity is what decides whether the route that
  may not translate the Actor per frame adapts the camera instead; it is read, not moved to find out.

### Fixed

- Follow the clip search's own rules when scanning a node: an object is scanned as far as it is
  readable instead of only when a whole 0x200-byte window is, which is what had been skipping every
  0x78-byte `StateInfo` - the node a state's generator hangs from - so the walk burned its object
  budget on the state machine's incidental pointers and reported no clip at all. The walk also asks
  the engine for each object's class name now (only on objects whose first word already looks like a
  vtable) and takes `hkbClipGenerator` as the answer, rather than inferring it from the animation
  name; depth and object limits are wider, and two histograms (`classes=`, `rejections=`) plus a
  `capped=` flag say whether a future miss is a lost walk or an absent clip.

### Added

- Play one of the source character's own animations on the copy (S2 animation
  handoff, HKX6). The binding set's elements turned out to be 0x30-byte stubs
  whose animation field is not where any reading put it, so this route stops
  digging there and uses the fully typed `hkbClipGenerator` instead: a bounded
  walk of the behaviour graph follows a state machine's typed `states` and, for
  everything else, only pointers whose first word looks like a vtable, and
  believes a node only when its animation name looks like an animation file, its
  binding passes the structural checks, and its playback mode and speed are ones
  the engine defines. The clip whose name is the character's own idle is picked,
  its tracks are resolved onto the copy's nodes by bone name, and the ground
  truth is printed: the clip sampled at every phase against the pose the engine
  currently holds, with the spread between the best and the worst phase. Sampling
  goes through `SampleIndividualTransformTracks`, which takes no chunk cache, so
  the open question about spline-compressed animations does not have to be
  answered first. F2 now plays that clip from the studio's own clock and falls
  back to the procedural idle only when no clip was found, logging which of the
  two it chose and how many of the clip's tracks the copy can be driven with.

### Changed

- Pick the binding's offset from the element's own shape and confirm it with the
  animation's own fields (S2 animation handoff, HKX5). The raw dump settled what
  the fixed candidate table could not: a binding-set element starts with an
  `hkReferencedObject` header (vtable, `memSizeAndFlags = 0xffff`, reference
  count 1), then an empty array, then a second header of the same shape at +0x30
  - a by-value `hkaAnimationBinding` - which the previous round's table missed by
  exactly one slot. Candidates are therefore derived from what the bytes look
  like (a vtable means an object begins there, a readable heap pointer means one
  is pointed at) and judged harder: the animation's type has to be one the engine
  names, its clip length has to be plausible, and its own
  `numberOfTransformTracks` has to equal the binding's track table, which a
  wrapper object cannot pass. Each probe line names the animation type (spline or
  interleaved decides whether sampling needs a chunk cache), its duration and
  original frame count, its skeleton name, the first bones its tracks drive, and
  how many of those tracks resolve to nodes of the copy - the write coverage,
  measured without sampling anything. The character's own standing idle gets its
  own group, because the path-sorted list buries `idle.hkx` under weapon and
  object idles whose file names also start with "idle".

### Changed

- Locate the binding inside a binding-set element instead of guessing it (S2
  animation handoff, HKX4). The round before proved the element is neither an
  `hkaAnimationBinding` nor a pointer to one, and that the character's animation
  names are typed and already in the binding set's order, so this round tries a
  table of offsets with both readings and judges every row structurally: the
  animation has to look like a live object with a plausible clip length, and its
  tracks have to name bones this skeleton has - no more tracks than bones, since
  a transform track belongs to one bone. Every row reports why it was rejected
  (`unreadable`, `no-animation`, `tracks-unreadable`, `duration`, `track-bones`)
  and says whether the element's first word looks like a vtable at all, the first
  0x40 bytes of two elements are dumped raw so a layout no row matches can still
  be read off by hand, and the plain idles - file names starting with "idle",
  which the path-sorted list buries under weapon and object idles - get their own
  line with their indices.

### Changed

- Read the character's animation list and validate how its bindings can be read
  (S2 animation handoff, HKX3). The names are typed
  (`hkbCharacterStringData::animationNames`), so a binding index can be named
  without guessing, and the round reports how many names mention an idle and
  where they sit. The binding elements are not typed in this checkout, so both
  candidate layouts - the element being the binding, and the element starting
  with a pointer to one - are judged structurally on a spread of indices instead
  of being believed: the candidate's animation pointer has to look like a live
  engine object (its vtable inside the game's own image), its duration has to be
  a plausible clip length, and every one of its tracks has to name a bone the
  skeleton has. Every read goes through a committed-and-readable page check and
  nothing virtual is called on a candidate, so a misread pointer reports
  nonsense instead of crashing the game. The idle indices get a line of their
  own, where a name, a duration and a full track table can be read side by side.

### Changed

- Replay the source character's own pose into the copy and measure how far it
  lands from it (S2 animation handoff, HKX2). The engine already holds that pose
  in `hkbCharacter::poseLocal`, so the write path the sampling route will use
  can be verified without a clip, a binding set, or any structure whose order is
  unknown: bone to node by exact name, Havok's quaternion to `NiMatrix3`, the
  local write, and one downward world recompute shared with the studio's other
  drivers. Every capture poses the copy four ways - the pose indexed by the
  animation skeleton and by the engine's `boneNodes` table, each with the
  rotation and with its transpose - and prints the worst bone-to-source distance
  for each, next to a control that first poses the copy half a radian away from
  the source: without that control, a zero distance would only say the
  measurement is insensitive. The verdict names the candidate that reproduces
  the source, so the engine's own indexing and quaternion conventions are
  measured rather than assumed, and the captured pose is put back afterwards,
  with its own measurement as proof that the panel is as it was found. The
  conversion itself is unit-tested (a quarter turn about Z taking +X to +Y, a
  half turn about X taking +Y to -Y, the inverse undoing the rotation, and a
  pose sample read out of Havok's SSE quads with position, all four quaternion
  components and its scale).

### Changed

- Report how the engine's animation skeleton maps onto the copy, at capture
  (S2 animation handoff, step 1 of the HKX1 work package). F7 now also reads
  the source character's animation graphs read-only and prints what each one
  holds - whether it drives the captured third-person root, its bone-node table
  against the animation skeleton, `poseLocal`, the size of its binding set and
  the class of its root generator - then resolves every bone of the animation
  skeleton against the copy's nodes by exact name and prints
  `SCOPY ANIM gate verdict=PASS|PASS-AMBIGUOUS|INCOMPLETE|UNAVAILABLE` with the
  matched and ambiguous counts plus a sample of what did not resolve. The
  skeleton's own parent relation is reported but does not gate the mapping:
  this rig's spine hangs under CME UBody while its pelvis hangs under CME
  LBody, so those two hierarchies disagree by design, and bones resolve to
  nodes by exact name because loose matching is what drove the wrong skeleton
  in the S2 probe. The rules are pure functions with their own unit test
  (exact match only, a duplicated node name resolved once and flagged, the
  report list capped while the counts stay complete, a cyclic parent table
  terminating). Nothing is written to the source character, to the copy, or to
  the engine's graphs; sampling an animation into the copy is the next step.

### Changed

- Record the S2 animation direction change: the procedural idle proved that
  continuously writing the copy's bones drives it, but it has no ground truth,
  so "does it look right" was unfalsifiable and the pelvis feedback could only
  be answered by tuning numbers. The next work package drives the copy from
  real animation data instead - the engine's own Havok sampling pipeline
  (hkaAnimation::SampleTracks, hkaAnimationBinding's track-to-bone map,
  hkbClipGenerator, the character's animation skeleton, BShkbAnimationGraph's
  bone-node table) - compared numerically against the source character at the
  same phase rather than by eye. The idle stays as a fallback for bodies with
  no usable animation and is not to be tuned further. Plan, evidence pointers,
  failure surfaces and the A/B protocol: docs/s2-animation-handoff.md.

### Fixed

- Give the hips a translation channel and report each driven joint's travel.
  The first in-game look at the idle found the waist apparently motionless
  while everything else moved, and that reading was correct twice over: in
  this skeleton the pelvis hangs under CME LBody while the spine hangs under
  CME UBody, so the two are siblings and a pelvis rotation cannot carry the
  torso; and a rotation about a joint's own origin does not move that joint at
  all, which is all the hips had. The hips now shift sideways (+-0.9 studio
  units), tilt a little further, and every two seconds the log reports how far
  each driven joint has travelled from the captured pose next to the rotation
  its channels ask of it, so "does the waist move" is answered with numbers
  instead of an impression. A unit test now requires at least one translation
  channel, because a rotation-only table is exactly how this regressed.

### Changed

- Move the procedural idle to F2 and retire the S2 measurement probe from the
  product build. F6 is bound by another plugin in the target setup, and the
  probe had already answered its questions over four recorded rounds, so F2
  now toggles the idle while the probe becomes a diagnostic build behind
  -DCHARACTER_PANEL_S2_PROBE=ON (which puts it on F6 and leaves F2 to the
  idle there too). With the option off, the probe's hotkey and its per-frame
  measurement stages are compiled out, and the load banner lists only the keys
  the build actually binds.

### Added

- Give the copy a procedural idle (PRD FR-03's first driver): hips roll
  and turn, the spine counter-rolls and breathes, and the head looks
  around on its own slower cycle, driven from a steady clock we own because
  the engine's timer does not advance while the panel is paused. Channels
  are resolved by exact joint name against the copy's own skeleton and
  reported as `SCOPY IDLE bound N/M channels`, so a body whose skeleton
  differs says so instead of silently animating nothing. Every frame
  recomputes from the captured pose (CharacterClone::pose restores it
  first), so nothing accumulates and F6 off returns the figure to the
  captured pose on the next frame. It runs only while the panel is drawn,
  only writes the copy's nodes, and yields while the F2 probe is armed.
  Amplitudes and periods live in src/character/idle_driver.h and the axis
  convention (X lean, Y screen roll, Z turn; Z is up) is stated there.
- Share apply_world_delta_downward and add rotate_about_own_origin in
  snapshot_transform.h: the idle driver and the probe drive joints the same
  way, and the subtree recomputation (which the engine's dirty-update
  cascade does not perform inside the draw window) should exist once.
- Fourth in-game probe round (2026-10-09, S2P4) and the two answers it
  produced. The skin matrix buffer's 3x3 is the bone's world rotation times
  the graph's 0.35 scale - a skinning matrix with the scale baked in - which
  is why the previous round's ten unit-norm candidates all missed by ~2.0;
  the layout itself (3x4 row-major, translation at 3/7/11) was right all
  along. For the bone the probe swings, the slot matches that bone's CURRENT
  world transform, not the captured pose (residual 0.0002), so the engine
  rebuilds the skinning data from the nodes we write during the draw: the
  CPU half of R05 holds, and with the round's paired off/on observation
  (swings when armed, resets and stays still when not) the chain from node
  write to screen is closed. The same dump found the boundary that matters
  for any future write: slot i is NOT bones[i] in general (5/31 and 4/71
  self-matches; one skin's 71 slots hold only 16 distinct matrices), which is
  why writing bones[i]->world into slot i never moved anything in the
  earlier rounds. Analysis scripts and log:
  docs/diagnostics/analyze_tslot_*.py, docs/s2-anim-probe-evidence-2026-10-09.md.
- Dump the target skin's slots raw, once per F2 arm: every slot's twelve
  floats next to its bone's name and that bone's own world transform, for both
  matrix buffers. The third round compared ten engine-side candidates and
  every one of 1132 samples missed by 1.97-2.64, with the "nearest" rotating
  at random among eight of them - so the candidate set was never the problem
  and the reading layout (or the slot's indexing) is. Two columns of raw
  numbers settle what distances could not.
- Third in-game probe round (2026-10-09, S2P3): 566 measurement sets over one
  CBBE and one UBE capture, no crash. The operator's paired observation closes
  R06: with F2 on the figure swings, with F2 off it resets and stays still. The
  two matrix buffers classify identically, so a shader reading the other buffer
  is not what made the relation labels split 50/50; the slot's contents remain
  unidentified, which is what S2P4's raw dump is for.
- Sample both skin matrix buffers and classify the slot against ten
  engine-side candidates (bone world, captured pose, previous frame's swing,
  each transposed, the bind transform, and the world/bind products), printing
  the nearest two with their distances. The first two rounds compared a single
  candidate pair, so a slot holding some third quantity looked exactly like a
  coin flip: at maximum swing amplitude the labels split 50/50, which rules out
  the simple reading and leaves the buffer's contents unidentified. Reading the
  engine's second buffer covers the remaining cheap explanation, a shader that
  samples the other one on some frames. Measurement stays read-only; unit tests
  now assert a rotation and its transpose stay distinguishable.
- Second in-game probe round (2026-10-09, S2P2): 241 measurement sets over
  one CBBE and two UBE captures, no INVALID reading, no crash, every capture
  parked on F4. The swing now reads back as the designed amplitude
  (1.3-34.4 degrees against a 0.6 rad design), the witness moves in all 241
  sets, and the engine rewrites the sampled skin matrix slot during the draw
  in all 241 - so the engine-side rebuild is active on the drawn copy. The
  slot's contents still cannot be identified: at maximum swing amplitude the
  relation labels split 50/50, which rules out the simple "the slot follows
  the node write" reading, so the CPU half of R05 stays open. Log and
  analysis: docs/s2-anim-probe-evidence-2026-10-09.md.
- Measure where the probe joint sits in the skeleton and how much of the
  figure it can move. The parent chains prove the CBBE/UBE difference seen in
  the first round: the pelvis hangs under CME LBody while the spine hangs
  under CME UBody, both branching from CME Body, so the two branches are
  siblings and a pelvis swing cannot carry the torso. affected-geoms
  quantifies it per body (10/23 for the pelvis, 43/46 for the spine).

### Fixed

- Read the probe's orientation from the relative rotation's trace and its
  slot relation from the whole 3x3 rotation block. The first measurement
  build compared matrix row 0, which is exactly the row a world-X swing
  leaves untouched, so it reported orientation-delta=0 and "neither" for a
  swing the game visibly applied and left the CPU-buffer question
  unreadable. Unit tests now assert the X/Y/Z reading and that row 0 is
  invariant under an X swing.
- Drop the probe target when a capture replaces the drawn graph. It used to
  survive the re-capture, so after loading another save the probe kept
  swinging (and reporting numbers for) the parked copy while the panel drew
  the new one: 136 of 564 measurement lines in the 2026-10-09 session
  described a graph that was not on screen. The T2 line no longer reuses a
  stale pass name: it prints not-submitted when the target geometry's pass
  did not arrive.
- Pick the probe joint across every skin instead of the first one found, and
  report the chosen joint's parent chain and how many geometries the swing
  touches (affected-geoms). In the 2026-10-09 session the CBBE run picked
  the body skin's pelvis while the UBE run picked the face skin's spine, so
  the two bodies visibly swung different halves of the figure — a target
  selection artifact, not an anchor or per-body difference.

### Added

- Land the S2 probe's first in-game result (2026-10-09): with the probe
  armed, the copy's figure visibly follows the bone write on both CBBE and
  UBE bodies. The archived log has 141 of 141 measurement sets reporting
  probe-effective, witness displacements up to 14.1 units, and the engine
  rewriting the skin matrix slot during the draw in every one of them. What
  remains unproven is whether that slot follows our write and what the GPU
  binds; the HKX driver is not started. Log and analysis:
  docs/s2-anim-probe-evidence-2026-10-09.md.
- Add the T0-T3 measurement chain the S2 probe needed to be judgeable
  (PRD 0.6 §6.3): each frame records the baseline after the studio's base
  placement, the swing applied to one bone, the pass that submits the
  target geometry, and the state after the draw, then prints the four
  stages together with the frame number, the bone's world matrix and the
  skin matrix slot read BEFORE the swing. The slot is sampled read-only
  and compared against this frame's two candidate node rotations, so no
  verdict rests on a value the same code wrote. A witness on the swing
  axis is reported as INVALID instead of being read as "nothing moved".
- Give every probe round its own build identity: CMake now generates
  Plugin_Build_Identity from CHARACTER_PANEL_PROBE_REVISION plus both
  source baselines and the configure time, the manifest carries the same
  string, and the package step rejects a DLL that does not contain it.
  The archived S0 rounds all logged one hardcoded identity, so a stale
  deployment could not be told from a fresh build.

### Changed

- Run the S2 probe inside the draw window, after pose() and before pass
  generation. It used to run before draw(), whose first act is pose()
  restoring every captured node world, so the write was undone before the
  engine prepared anything and the probe's own logs predated the
  overwrite — the archived runs could not show whether the write ever
  reached the draw. The stage calls now live in CharacterClone::draw(),
  which is the only place that owns the order.
- Change node transforms only in this probe round: the skin matrix buffer
  rebuild is gone, because the read-back that once classified that buffer
  compared the slot against the value the same code had just written into
  it. write_bone_matrix stays for the later, evidence-gated step.
- Swing about world X and pick the witness furthest from that axis: Z is
  the bone's own long axis, so the old swing spun the witness in place and
  a flat displacement reading could not distinguish a failed write from a
  witness sitting on the rotation axis.
- Land the S0 scene-graph-copy route (CHARACTER_PANEL_SCENE_COPY_EXPERIMENT),
  game-validated on 2026-10-08: F7 copies and audits the player's third-person
  graph through native NiObject::Clone, F8 draws it into the studio, F3 rotates
  the copy without touching the world actor, F4 releases it. Both the copy/draw
  lifecycle test and the re-equip/appearance/physics test pass on CBBE and UBE
  bodies with no crash across capture, release, recapture, load and main-menu
  transitions; frozen hair and cloth in the copy is the documented S0 scope, not
  a failure. Validation record and per-question results:
  docs/scene-graph-copy-results-2026-10-08.md.
- Park every replaced or released snapshot for the process lifetime instead of
  destroying it. Freeing a native clone's graph crashed at every point tried
  (crashes 2026-10-08 23-01-11 in capture(), 23-07-35 at a clean frame boundary
  from the retired queue, 23-12-02 at the main-menu boundary), each time inside
  BSFadeNode's destructor through the tbb allocator; the copy path therefore
  holds its graphs open the way the retired Actor route held its shell alive.
- Prune non-character objects from a captured graph before framing or drawing:
  particle geometries, skeleton-driven collision helpers (3BCA_* parts,
  VirtualGround, CollisionStopper), script-spawned markers, blood and flash
  visuals, and the captured spill lights. A heavily modded player graph carried
  123 geometries and 106 passes, of which only the weapon ever reached the
  screen; after pruning it is 51 and 47 and the figure renders complete.
- Exclude skeleton-driven collision helpers from the studio framing bound: they
  share the body's skeleton and sit far enough from it to drag the bound and the
  centering off the figure, which is what pushed the UBE body out of frame.
- Add the scene-graph-copy test package target (CharacterPanelSceneCopyPackage,
  opt-in with CHARACTER_PANEL_SCENE_COPY_EXPERIMENT): it stages the DLL with its
  PDB, the S0 install/test README, and a JSON build manifest carrying both
  binaries' SHA-256, the repository HEAD, the actual extern/CommonLibSSE checkout
  and each worktree's dirty state, so a result returned from the validating
  machine can be tied to the artifact that produced it. The
  CHARACTER_PANEL_SCENE_COPY_EXPERIMENT option configures the experiment
  tree (there is no Release-scopy preset); the manifest takes its configuration from
  $<CONFIG> rather than assuming Release.
- Add a source-graph drift report (SCOPY SOURCE-DIFF) that separates a changed
  object set from per-node transform drift. The previous exact float comparison
  rejected captures whose nodes had moved by a few ULPs, which blocked the CBBE
  body across the pause; only a changed object set blocks a capture now.
- Log the first few F3 presses and the build's hotkey map in the SCOPY BUILD
  banner, so a stale deployment is distinguishable from a dropped key.

### Changed

- Record the M0 engineering-side acceptance (docs/m0-acceptance.md):
  7 criteria over runs 17-26 — 6 pass (visible composite with correct
  content, content tracking across items/zoom, toggle lifecycle without
  resource accumulation, non-destructive frames, FR-05 input rules,
  reproducible evidence chain), 1 partial (the save-load force-close
  MESSAGE trigger is code-ready but not yet game-verified — folded into
  phase 2's first round along with the formal PRD 5.3 timing). The
  panel-rect INI is deferred by user decision; phase 2 (the independent
  display instance P) is now the current phase.
- Land the visible panel (v4.6 confirmed by run 26): with the blend fix
  from the user's RenderDoc capture, the studio image now appears as the
  M0 opaque rectangle — exactly at 58-88% x 12-68% of the frame, world
  and character rendering normally around it, ~3600 composite draws
  across the session with no crash, and 3 close-capture TGAs alongside.
  Work package 3 (compositing) is complete; remaining M0 work is the
  formal A01 acceptance run and the fixed-pose paused scenario. Known
  M1 polish items: the rectangle currently overlaps the modded item card
  (card draws earlier, so the panel sits above it — PRD section 2
  ordering refinement), and the studio image is squeezed whole into the
  rect (framing is M1 camera work).
- Enable alpha blending on the composite quad (v4.6, run 26 pending,
  from the user's RenderDoc capture): the composite target is consumed
  through the UI alpha-composition pipeline, so the quad must be BLENDED
  over the existing layer content — the disabled-blend overwrite used
  since v4.1 broke the layer's composition semantics. The quad now uses
  standard non-premultiplied over (SrcAlpha/InvSrcAlpha), and a
  per-session diagnostic logs the engine's own blend state bound at
  composite time so a nonstandard convention can be copied verbatim from
  the log/RenderDoc instead of inferred.
- Move the composite into the replay hook (v4.5, run 25 pending): run 24
  falsified the culling diagnosis with its own diagnostic line — the
  rasterizer bound at composite time is cull=1 (CullNone), so the v4.3
  quad was never culled and the v4.4 winding "fix" is retracted. What
  survives: pixels rasterize into the last-replayed instance at
  end_frame, the item's pixels in that same instance are visible on
  screen, and the panel drawn microseconds later is not — the merge
  happens inside the original call (before end_frame), or format-28 is
  not the visible path at all. v4.5 draws inside the replay hook, right
  after the replay returns and the engine's OM is restored, into
  prev_rtv while the call-site instance is still bound — nothing
  engine-side runs between the passthrough's return and our draw, so a
  visible-image merge must read the instance after it. The falsified
  end_frame composite is removed; the capture stays as evidence.
- Fix the composite quad being silently culled (v4.4, run 24 pending):
  run 23 proved Draw was called every bracketed frame with replays while
  zero pixels rasterized. Root cause: the viewport transform flips NDC
  y, so the quad's original corner order is counter-clockwise in window
  space — back-facing — and the engine's CULL_BACK rasterizer state
  culled every quad since v4.1. v4.4 corrects the winding to clockwise,
  binds the composite's own rasterizer state (CullNone, ScissorOff) with
  save/restore, and logs the previously bound rasterizer's cull/scissor
  once per session as evidence.
- Move the panel composite to the bracket close (v4.2, run 22 pending):
  run 21 confirmed the captured call-site target is the format-28 UI
  composite, but the log showed five recaptures ~0.35 s apart — the
  engine rotates composite instances every menu frame, so the v4.1
  entry-time draw painted the previous frame's instance while the merge
  reads the current one. v4.2 draws at end_frame (right after the
  original menu draw returns, before the frame's UI merge) into the
  instance captured during THIS frame's replays, gated on
  lighting_replayed > 0 so a stale cross-frame capture is never used;
  v4.3 adds the missing ensure() call (v4.2 shipped without it and never
  drew at all). The rectangle is therefore expected on menu frames with
  replayed content; gameplay-path compositing is a later, separate
  evidence step.
- Reroute the v4 panel composite (v4.1, run 21 pending): run 20 proved
  the composite mechanism (shaders, state save/restore, draw, clean
  close-captures 053-057) but the rectangle never showed — the target
  bound at DrawInterfaceStart entry is a format-24 (R10G10B10A2) non-
  visible intermediate, not the format-28 UI composite the visible menu
  preview draws into. v4.1 captures the call-site RTV (AddRef, recapture
  on pointer change, released on panel close) during replays and binds
  that target for the entry composite; OM and viewport join the
  save/restore set. If format 28 is captured and the rectangle is still
  invisible, the next probe targets the merge pass's region gating, not
  another reroute.
- Add the v4 panel composite (handoff work package 3, built 2026-10-01,
  run 20 pending): while the panel is open, the DrawInterfaceStart entry
  draws the studio target (new SRV) as an opaque rectangle — SV_VertexID
  quad with runtime-compiled shaders, blend/depth off — into the bound
  target BEFORE the original menu draw, so the visible panel sits under
  every other UI element per the PRD composition order. Fixed panel
  rect (58-88% x 12-68% of the screen) with the whole studio image
  squeezed in (framing is M1 camera work); only the quad's own state is
  saved and restored; a feedback guard skips drawing over the studio
  target itself. Run 20 answers what target the entry binds, whether the
  rectangle shows under SkyUI widgets, world/menu cleanliness (A01
  pre-check), and the CS/ENB/ReShade relative behavior.
- Turn the M0 prototype from the F6 one-shot into the v3 persistent panel
  skeleton (handoff work packages 1+2, built 2026-10-01): F6 toggles the
  panel and, while open, every DrawInterfaceStart frame is bracketed so
  menu-scene lighting passes keep the studio target current every menu
  frame; the offscreen target is persistent (created from the call site
  description, self-recreated on resolution change, released on the
  render thread on panel close, and force-closed on save loading / new
  game per FR-06); the per-frame TGA readback is replaced by an opt-in
  F7 one-shot dump (PRD 5.3: no steady-state GPU readback);
  persistent-frame logging is throttled (per-geometry discovery lines,
  state-change summaries, slow heartbeat). Run 17 verifies the toggle,
  release, silent-gameplay-bracket, and reopen-replay mechanics; run 18
  adds a 31-second soak, effect-shader menu-pass discovery, and
  item-change tracking — but both sessions produced zero TGA because
  every F7 press came after panel close. v3.1 fixes the interaction
  instead of the operator: closing the panel with F6 now writes one
  evidence TGA from the studio target before the render-thread release
  (force-closes and content-free closes write nothing), the dump key is
  demoted to an optional mid-session grab, the content-summary cadence is
  fixed to state changes / ~30 s, and the hotkeys move F6/F7 to F7/F8
  (F6 is bound in the user's game). Run 19 passes the v3.1 checklist:
  four close-captures and one mid-session grab produced five TGAs whose
  readbacks show correct materials and occlusion across seven item
  geometries and an 8-pass cluster frame, over 28.5 s and 52 s soak
  stretches — closing handoff work packages 1+2.
- Retire the falsified accumulator-swap route in the M0 prototype and add
  the v2 pass-redirection frame: F6 now brackets one DrawInterfaceStart
  frame, the three RenderPassImmediately call-site hooks log every pass in
  the bracket, and menu-scene BSLightingShader passes (geometry descending
  from a UI3DSceneManager menuObjects root) are non-destructively replayed
  into a private kMAIN-format offscreen target that is read back to
  proto-pass-NNN.tga.
- Record the v2 run-9 session: menu-pass identification and call-site
  interception verified (IronShield pass, menu=true, site 1), but replays
  landed zero pixels. Runs 10-13 then peeled the onion: depth/stencil
  rejection falsified (state logged disabled), the shadow-state
  re-application proven (raw bind lost on the first call, honored on the
  second), the Community Shaders chain collision fixed (site 1 was
  interposed; the pre-patch E8 target is now captured and restored), the
  SRV stomp caught by snapshots and repaired — and the remaining dim-arc
  result showed the pre-call replay order itself was the problem. v2.5
  inverts the order: the passthrough draws first, then the hook binds the
  private target and calls once more, reusing the original's own pipeline
  state with no capture or restore machinery at all. Run 14 passes the M0
  gate: the Iron Shield renders completely and correctly shaded into the
  private target every armed frame, non-destructively, closing the M0 core
  evidence chain for the pass-redirection route. v2.6 adds a private
  depth buffer with an explicit depth-on state after the user spotted the
  handle rendered over the boss (depth test bypassed without a DSV); run
  15's target-creation failure (the engine's depth resource reports a
  typeless format) is fixed by normalizing to the typed depth format, and
  run 16 verifies the handle is correctly occluded.

### Added

- Add an opt-in engine evidence probe for the experimental studio renderer.
- Add the 2026-09-28 M0 capture report analyzing the first user-run probe
  session; verifies contract routine offsets on AE 1.6.1170 under Community
  Shaders and locates the engine's current-accumulator swap path.
- Change the probe capture hotkeys to F7 (code + menu) and F8 (menu only);
  F9 stays reserved for quick load.
- Add the opt-in M0 accumulator-swap prototype (CharacterPanelProto): one
  DrawInterfaceStart entry detour (Microsoft Detours) and an F6-armed single
  frame that swaps the UI3D secondary accumulator in as the engine's current
  accumulator, then restores and dumps a tone-mapped TGA readback. A first
  raw write_call-based hook recursed into the patched entry and crashed the
  main menu; the detour relocates the original prologue instead.
