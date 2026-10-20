# CharacterPanel

C++23 SKSE inventory preview using a copied [SceneGraph](docs/scene-graph.md), with independent animation state per source actor. Native DAC temporarily owns bounded phase effects on the source Actor during inventory, and the entry effects are restored at close. No second Actor is created.

With the pinned checkout and toolchain prepared as described in [Build](docs/build.md):

```powershell
cmake --preset Release
cmake --build --preset Release
```

Release is the only configure/build preset; CharacterPanel is the sole first-party target. Outputs are `build/Release/CharacterPanel.dll` and its matching PDB. In a paused inventory, **F7** queues recapture and **F8** toggles display.

- [Build](docs/build.md): VS2022/MSVC, pinned submodule/vcpkg setup, artifacts and configure-time identity.
- [SceneGraph](docs/scene-graph.md): preview ownership, capture/retarget, framing and lifetime constraints.
- [Animation](docs/animation.md): event timing, actual Actor effect queries, native variant decisions and pose composition.
- [Native DAC](docs/dac.md): original DAC binary/profile, inventory entry snapshots, UI event tasks and close restoration.
- [Maintenance and debugging](docs/code-maintenance.md): module/hash conventions and actual log navigation.
- [Historical reports](docs/history.md): complete archived report/requirements index.
- [Runtime notes](tools/scene_copy/README.txt): compact controls and game follow-up.

Smooth Moveset support is bounded: the [core phase prerequisites and remaining IED/combat branches](docs/dac.md#smooth-moveset-compatibility-boundary) are documented separately. Installing IED and its condition provider alone does not enable the pending placement-condition adapter; complete equipment-action/pack coverage is not established.

Local native build evidence exists; current CMake exposes no test/package/install route. Earlier two-case animation results are historical validation. Skyrim hook/loader/visual/lifetime behavior remains unverified; see [verification limits](docs/build.md#verification-and-limits).
