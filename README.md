# CharacterPanel

C++23 SKSE inventory preview using a copied [SceneGraph](docs/scene-graph.md), with equipment-driven non-combat idles independent of source world actions. Native DAC temporarily owns bounded phase effects on the source Actor during inventory, and the entry effects are restored at close. No second Actor is created.

With the pinned checkout and toolchain prepared as described in [Build](docs/build.md):

```powershell
cmake --preset Release
cmake --build --preset Release
```

Release is the only configure/build preset; CharacterPanel is the sole first-party target. Outputs are `build/Release/CharacterPanel.dll` and its matching PDB. In a paused inventory, **F7** queues recapture and **F8** toggles display.

- [Build](docs/build.md): VS2022/MSVC, pinned submodule/vcpkg setup, artifacts and configure-time identity.
- [SceneGraph](docs/scene-graph.md): preview ownership, capture/retarget, framing and lifetime constraints.
- [Animation](docs/animation.md): event timing, actual Actor effect queries, native variant decisions and pose composition.
- [Fixed base indices](docs/animation.md#fixed-base-indices-and-project-readiness): prepare per-project idle/equip indices before inventory; native query stages and the [custom-weapon boundary](docs/animation.md#custom-weapons-and-new-behavior-entries).
- [OAR and native IED/SDS queries](docs/oar-hooks.md#native-ied-placement-conditions-2026-10-21): three audited native predicates, provider/schema requirements and runtime limits.
- [Native DAC](docs/dac.md): original binary/profile, [winning MGEF script-attachment preflight](docs/dac.md#marker-script-attachment-preflight), inventory snapshots, UI tasks and close restoration.
- [Maintenance and debugging](docs/code-maintenance.md): module/hash conventions and actual log navigation.
- [Historical reports](docs/history.md): complete archived report/requirements index.
- [Runtime notes](tools/scene_copy/README.txt): compact controls and game follow-up.

Empty hands default to an unarmed non-combat loop. Set `UnarmedIdle=mt_idle` under `[Animation]` in `Data/SKSE/Plugins/CharacterPanel.ini` for direct vanilla standing; copy the [sample INI](config/CharacterPanel.ini) and restart the game. Equipped gear is held only in the copy; source draw/equipment/graph state stays read-only. See [idle/config policy](docs/animation.md#configuration).

Smooth Moveset support is bounded: the [core phase prerequisites and remaining IED/combat branches](docs/dac.md#smooth-moveset-compatibility-boundary) are documented separately. Two IED placement predicates and `SDS_IsShieldOnBackEnabled` now support current source equipment/configuration with the supplied provider (archive label 1.0.3, embedded version 1.0.2.0). Native false remains false; historical-hand queries and unknown provider/schema versions reject. The SDS branch needs the provider's own SDS registration/prerequisites. Real registration, paused IED refresh and complete equipment-action/pack coverage remain unverified.

Local native build evidence exists; current CMake exposes no test/package/install route. The two direct-compiled attachment cases check script-free markers and a scripted override; the separate [two IED cases](docs/build.md#ied-condition-verification) check native-result preservation and historical-scope rejection. Earlier policy/preparation and native binary checks remain separate evidence. Game file visibility, winning-definition/ID mapping and Skyrim hook/loader/visual/lifetime behavior remain unverified; see [verification limits](docs/build.md#verification-and-limits).
