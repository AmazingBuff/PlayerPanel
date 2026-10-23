# CharacterPanel

C++23 SKSE inventory preview using a copied [SceneGraph](docs/scene-graph.md), with equipment-driven non-combat idles independent of source world actions. Native DAC temporarily owns bounded phase effects on the source Actor during inventory, and the entry effects are restored at close. No second Actor is created.

With the pinned checkout and toolchain prepared as described in [Build](docs/build.md):

```powershell
cmake --preset Release
cmake --build --preset Release
```

Release is the only configure/build preset; CharacterPanel is the sole first-party target. Outputs are `build/Release/CharacterPanel.dll` and its matching PDB. In a paused inventory, **F7** queues recapture and **F8** toggles display.

Default animation diagnostics report accepted panel mappings and paused-source WORLD snapshots without reverse animation-table scans. `[Diagnostics] DetailedAnimationTrace=false` keeps verbose probes off; enable it and restart only for detailed investigation. Info flushes about once per second; warnings/errors immediately. See [mapping fields and timing limits](docs/animation.md#accepted-mappings-and-world-snapshots).

- [Build](docs/build.md): VS2022/MSVC, pinned submodule/vcpkg setup, artifacts and configure-time identity.
- [Independent behavior graph validation](docs/independent-behavior-graph.md): six gated experiments and the default-off `BehaviorGraphProbe` source ownership baseline. No independent graph is created or driven; G1 has not passed and G2–G6 are locked. The 1.6.1170 [binary audit](docs/behavior-graph-construction-audit.md) confirms the factory, holder and paired-destruction facts, locates the project loading chain (orchestrators 32881/32882/32893/32902 → loader 63585 → resolver 63844 → behavior-graph load 63845) and shows the engine's own model: a fresh graph instance per load over refcount-shared project data. Activation, event routing, runtime isolation and no-Actor reuse remain unproven.
- [SceneGraph](docs/scene-graph.md): preview ownership, capture/retarget, framing and lifetime constraints.
- [Equipment context and torch composition](docs/animation.md#equipment-context-and-torch-composition-2026-10-23): retained Unequip context, independent torch arm ownership and explicit fallback.
- [Animation](docs/animation.md): owns the nested `src/animation/oar` and `src/animation/dac` integrations, event timing, native queries/variants and pose composition.
- [Fixed base indices](docs/animation.md#fixed-base-indices-and-project-readiness): prepare per-project idle/equip indices before inventory; native query stages and the [custom-weapon boundary](docs/animation.md#custom-weapons-and-new-behavior-entries).
- [OAR and native IED/SDS queries](docs/oar-hooks.md#native-ied-placement-conditions-2026-10-21): three explicitly checked schemas, matched-provider forwarding, dependency-specific historical-hand guards and runtime limits.
- [Native DAC](docs/dac.md): original binary/profile, [winning MGEF script-attachment preflight](docs/dac.md#marker-script-attachment-preflight), inventory snapshots, UI tasks and close restoration.
- [Maintenance and debugging](docs/code-maintenance.md): module/hash conventions and actual log navigation.
- [Reported animation diagnostics](docs/diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机): current handover for the shield/axe/torch/bow transition report, both archived session logs, previous-hand removal context, one-handed body plus left torch arm, corrected direction-0 semantics, raw XYZ scales and parameterized rejection evidence and the next game procedure. The earlier [correlated-diagnostic budget](docs/diagnostics/display-issues-2026-10-22.md#8-2026-10-22-已报告问题的关联诊断) still defines the pose windows (four/twenty-four snapshots per request) and one fixed fifteen-bone source snapshot.
- [Historical research](docs/history.md): rendering/animation summaries and a per-source index for recovering 144 superseded reports/logs from the committed baseline.
- [Runtime notes](tools/scene_copy/README.txt): compact controls and game follow-up.

Empty hands default to an unarmed non-combat loop. Set `UnarmedIdle=mt_idle` under `[Animation]` in `Data/SKSE/Plugins/CharacterPanel.ini` for direct vanilla standing; copy the [sample INI](config/CharacterPanel.ini) and restart the game. Equipped gear is held only in the copy; source draw/equipment/graph state stays read-only. See [idle/config policy](docs/animation.md#configuration).

Smooth Moveset support is bounded: the [core phase prerequisites and remaining IED/combat branches](docs/dac.md#smooth-moveset-compatibility-boundary) are documented separately. Two IED placement predicates and `SDS_IsShieldOnBackEnabled` have explicit schema/required-version checks. Other names from the same matched provider retain native current-source answers without those individual checks. All provider names require the exact provider/embedded version (archive label 1.0.3, embedded version 1.0.2.0). Historical settings/explicit-node predicates are admitted under checked schemas; equipped hints require an unchanged requested hand. Changed-hand equipped hints, unknown historical predicates, mismatched checked schemas and unrelated providers reject. Native false remains false. The SDS branch needs its provider-side registration/prerequisites. Live registration, paused hint freshness and complete pack compatibility remain unverified.

Current local verification runs exactly [two animation repair scenarios](docs/build.md#animation-repair-checks-2026-10-23) for equipment/IED/direction policy and one-handed torch routing/full-left-arm composition/removal, plus Release compilation and bounded source/document checks. Controller/load timing and native behavior are source-reviewed, not executed by those cases. Older DAC, torch/shield, mapping, budget, attachment, IED and cache results remain historical evidence. CMake adds no test/package/install target. Native loading/effects/hooks, timing, visuals, performance and lifetime remain unverified.

The subsequent [behavior graph baseline change](docs/build.md#behavior-graph-baseline-checks-2026-10-23) uses Release compilation and bounded source/document/ownership checks, with no new behavioral cases. `[Diagnostics] BehaviorGraphProbe=false` is independent of detailed animation tracing; enable it and restart only to collect at most sixteen grouped source-baseline lines per actor/request. The [G1 binary audit](docs/behavior-graph-construction-audit.md) confirms the 1.6.1170 factory, holder arguments and paired aligned destruction on an unpacked copy, but project loading, linking, activation, native construction, isolation and both planned behavioral experiments remain unexecuted. An AE 1.6.1170 installation and an MO2 instance exist on this machine; this round did not deploy or launch the game.
