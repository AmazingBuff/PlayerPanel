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
- [SceneGraph](docs/scene-graph.md): preview ownership, capture/retarget, framing and lifetime constraints.
- [Animation](docs/animation.md): owns the nested `src/animation/oar` and `src/animation/dac` integrations, event timing, native queries/variants and pose composition.
- [Fixed base indices](docs/animation.md#fixed-base-indices-and-project-readiness): prepare per-project idle/equip indices before inventory; native query stages and the [custom-weapon boundary](docs/animation.md#custom-weapons-and-new-behavior-entries).
- [OAR and native IED/SDS queries](docs/oar-hooks.md#native-ied-placement-conditions-2026-10-21): three explicitly checked schemas, matched-provider forwarding, current-hand guards and runtime limits.
- [Native DAC](docs/dac.md): original binary/profile, [winning MGEF script-attachment preflight](docs/dac.md#marker-script-attachment-preflight), inventory snapshots, UI tasks and close restoration.
- [Maintenance and debugging](docs/code-maintenance.md): module/hash conventions and actual log navigation.
- [Reported animation diagnostics](docs/diagnostics/display-issues-2026-10-22.md#8-2026-10-22-已报告问题的关联诊断): opt-in restart/repro procedure, four pose windows/twenty-four snapshots and one fixed fifteen-bone source snapshot per request.
- [Historical reports](docs/history.md): complete archived report/requirements index.
- [Runtime notes](tools/scene_copy/README.txt): compact controls and game follow-up.

Empty hands default to an unarmed non-combat loop. Set `UnarmedIdle=mt_idle` under `[Animation]` in `Data/SKSE/Plugins/CharacterPanel.ini` for direct vanilla standing; copy the [sample INI](config/CharacterPanel.ini) and restart the game. Equipped gear is held only in the copy; source draw/equipment/graph state stays read-only. See [idle/config policy](docs/animation.md#configuration).

Smooth Moveset support is bounded: the [core phase prerequisites and remaining IED/combat branches](docs/dac.md#smooth-moveset-compatibility-boundary) are documented separately. Two IED placement predicates and `SDS_IsShieldOnBackEnabled` have explicit schema/required-version checks. Other names from the same matched provider retain native current-source answers without those individual checks. All provider names still require the exact provider/embedded version and current-hand scope (archive label 1.0.3, embedded version 1.0.2.0); historical hands, mismatched checked schemas and unrelated providers reject. Native false remains false. The SDS branch needs its provider-side registration/prerequisites. Live registration, paused hint freshness and complete pack compatibility remain unverified.

Local Release evidence exists; CMake exposes no test/package/install target. The four [current focused drivers](docs/build.md#current-animation-checks-2026-10-22) recorded 2 DAC matching, 2 torch/shield policy, 2 accepted-mapping/indexed-read and 1 diagnostic-budget case. These are narrow engine-free results, not game integration tests; final documentation/commit work reuses them. Earlier attachment, IED, policy and binary checks remain separate evidence. Native loading/effects/hooks, timing, visuals, performance and lifetime remain unverified; see [verification limits](docs/build.md#verification-and-limits).
