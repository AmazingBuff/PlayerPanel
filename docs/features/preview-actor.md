# Preview actor

Status: current

Canonical path: `docs/features/preview-actor.md`

Last verified against: working tree on 2026-09-21, baseline revision
`9bb543db3d61f60826671974cc828472c0f2d8c2`

M1 diagnostic module of the independent character panel: one non-persistent preview actor that
mirrors the player's supported appearance and currently worn equipment. The
[Preview panel](preview-panel.md) module renders this actor offscreen and hides its in-world copy;
this document owns the actor's lifecycle, not its rendering.

## Purpose

The plugin must show an independent character panel that displays the player's own character
(PRD [`independent-character-panel-prd.md`](../../../docs/independent-character-panel-prd.md),
FR-02/FR-03 and M1). Before any offscreen
rendering exists, the panel must prove that the engine can carry the cheapest half of that
contract: preparing a second actor that looks like the player and wears the same equipment without
touching the player, the player's inventory, the player's base record or the save game.

This module answers the first three M1 unknowns recorded by PRD §9:

1. how a preview actor is created from the player's own base record;
2. whether a non-persistent reference is isolated enough not to leak into the save game;
3. how the preview is destroyed again when the toggle or the session ends.

The observable result of this module alone was an in-world character standing `PreviewDistance` in
front of the player, created and destroyed by the configured hotkey, with every state transition
logged. That is no longer the user-visible result: the [Preview panel](preview-panel.md) module
renders this character offscreen and culls its in-world 3D root, so the character appears in the
panel and not in the world. Creation, dressing and destruction are unchanged by that module, and
animation remains a later phase.

## Scope and non-goals

In scope:

- creation and destruction of the preview reference and its duplicated base record;
- record-level appearance inherited from the player's own base record;
- worn-equipment synchronization from the player's own inventory changes;
- a game-thread frame tick that services preview requests;
- the `PreviewDistance` configuration value and the preview/master-switch semantics of the hotkey.

Explicitly unsupported in this batch (recorded here so they are never silently implied):

- **FaceGen overlays** — no tint-mask, overlay or head-part reconciliation is performed. The
  duplicate only points `faceNPC` at the player's base record, so any FaceGen data the engine
  derives from that pointer is whatever the engine itself produces.
- **RaceMenu morph injection** — RaceMenu's stored morph values are not read, transferred or
  applied. A preview created while RaceMenu morphs are active is expected to differ from the player.
- **Body-slide data** — no `body.tri`, slider or weight redistribution is copied; only the
  duplicated record's own height and weight apply.
- **Third-party skeletons** — no skeleton, XP32-style node or bone mapping is installed on the
  preview; it uses the engine's default actor skeleton.
- **Worn-item refinement** — no ammo-only, left-hand, shield-slot or per-slot reconciliation; the
  walk mirrors entries the player's own `InventoryEntryData::IsWorn()` reports as worn.
- **Rendering, cameras, animation, UI and inventory integration** — out of scope for this module;
  offscreen rendering with a private camera is now provided by [Preview panel](preview-panel.md),
  and the remaining items stay out of scope for the whole feature (see the Non-goals of the M1
  contracts).

## Architecture

```
hotkey (game thread, input sink)
    InputManager/InputHandler ──> PreviewActor::request_toggle()      (records a request only)

present (render thread)
    PresentHook::present_thunk ──> FrameHook::on_present()            (present listeners, then marks
                                                                       the frame and queues one task)

SKSE task queue (game thread)
    FrameHook::dispatch_frame() ──> FrameHook::tick() ──> PreviewActor::on_frame()
                                                              └─ PreviewActor::service_request()
                                                                   ├─ create()
                                                                   └─ destroy()

session messages (game thread)
    message_handler ──> PreviewActor::destroy()   (kPreLoadGame, kNewGame, kPostLoadGame)
```

Two threads meet in this module and only one of them touches the engine:

- The input sink and every SKSE message run on the game thread, so `request_toggle()` may only
  record a request; it performs no engine work.
- The present hook runs on a render thread, so `FrameHook::on_present()` invokes the render-thread
  present listeners there (the panel draws in them) and may otherwise only mark a frame and queue a
  task. All engine work that touches the scene graph happens inside the queued task, which SKSE runs
  on the game thread.
- `PreviewActor` itself is therefore game-thread only and needs no lock; the single
  `std::atomic<bool>` in `FrameHook` is the only cross-thread state.

The request indirection also gives the toggled state a single owner: the hotkey never creates or
destroys anything directly, so a failed creation degrades to a logged state instead of an
interrupted engine call inside the input sink.

## Code map

| Path | Symbol | Responsibility |
| --- | --- | --- |
| `src/preview/preview_actor.h` | `PreviewActor` | Public preview state machine and entry points |
| `src/preview/preview_actor.h` | `PreviewActor::State` (`kIdle`, `kReady`, `kUnavailable`) | The only three preview states |
| `src/preview/preview_actor.cpp` | `PreviewActor::current_reference` | The borrowed live reference the panel consumes |
| `src/preview/preview_actor.cpp` | `PreviewActor::create` | Duplicate, place, hide, dress, show |
| `src/preview/preview_actor.cpp` | `PreviewActor::destroy` | Disable, delete and clear every retained field |
| `src/preview/preview_actor.cpp` | `PreviewActor::service_request` | Toggle decision, one attempt per request |
| `src/preview/preview_actor.cpp` | `sync_worn_equipment` | Walks the player's worn entries and re-dresses the preview |
| `src/preview/preview_actor.cpp` | `place_in_front` | Places and turns the preview at `PreviewDistance` |
| `src/render/frame_hook.h` | `FrameHook` | Present hook to game-thread frame tick |
| `src/render/frame_hook.h` | `FrameHook::Listener`, `Max_Frame_Listener_Count` | Game-thread listener type and capacity |
| `src/render/frame_hook.h` | `FrameHook::PresentListener`, `Max_Present_Listener_Count` | Render-thread listener type and capacity, used by the panel |
| `src/render/frame_hook.cpp` | `FrameHook::install`, `install_present` | Idempotent hook install plus listener registration |
| `src/render/frame_hook.cpp` | `FrameHook::on_present`, `dispatch_frame`, `tick` | Render-thread listener calls, frame marking and game-thread dispatch |
| `src/render/present_hook.h` | `PresentHook` | Existing swap chain Present hook; the only hook writer |
| `src/config/config.h` | `Config`, `Setting` | `enabled`, `hotkey`, `preview_distance` and INI access |
| `src/input/input.cpp` | `InputHandler::ProcessEvent` | Applies the master switch and reports a toggle request |
| `src/main.cpp` | `install_frame_hook`, `message_handler` | Installs the hook and releases the preview per session |

Engine and SKSE dependencies used, with the same paths the contract's evidence index records:

| Dependency | Path | Use |
| --- | --- | --- |
| `RE::TESForm::CreateDuplicateForm` | `extern/CommonLibSSE/include/RE/T/TESForm.h` | Copies the player's base record instead of mutating it |
| `RE::TESNPC::faceNPC` | `extern/CommonLibSSE/include/RE/T/TESNPC.h` | Makes the duplicate borrow the player's FaceGen data |
| `RE::TESObjectREFR::PlaceObjectAtMe` | `extern/CommonLibSSE/include/RE/T/TESObjectREFR.h` | Places the non-persistent reference |
| `RE::TESObjectREFR::Disable` / `Enable` / `SetDelete` | `extern/CommonLibSSE/include/RE/T/TESObjectREFR.h` | staged hide/dress/show and destruction |
| `RE::ObjectRefHandle` | `extern/CommonLibSSE/include/RE/B/BSPointerHandle.h` | The only retained reference state |
| `RE::Actor::AddWornItem` | `extern/CommonLibSSE/include/RE/A/Actor.h` | Re-dresses the preview |
| `RE::Actor::GetActorBase` | `extern/CommonLibSSE/include/RE/A/Actor.h` | Reads the player's base record |
| `RE::TESObjectREFR::GetInventoryChanges` | `extern/CommonLibSSE/include/RE/T/TESObjectREFR.h` | Reaches the player's own inventory changes without `GetInventory()` |
| `RE::InventoryChanges::entryList` | `extern/CommonLibSSE/include/RE/I/InventoryChanges.h` | Iterated directly, no inventory copy |
| `RE::InventoryEntryData::IsWorn` / `GetObject` | `extern/CommonLibSSE/include/RE/I/InventoryEntryData.h` | Worn-entry filter |
| `SKSE::TaskInterface::AddTask` | `extern/CommonLibSSE/include/SKSE/Interfaces.h` | Game-thread dispatch of the frame tick |

## Interfaces

### `PreviewActor`

- `static PreviewActor& instance()` — the module singleton; there is no file-scope mutable state.
- `void request_toggle()` — records a show/hide request. The caller has already applied the master
  switch, so this function never reads configuration.
- `static void on_frame()` — frame-hook listener. Static so its address is a plain function
  pointer; it services a pending request and guards the state against an unexpected throw.
- `void destroy()` — disables and deletes the preview reference, clears every retained field and
  drops a pending request.
- `[[nodiscard]] State state() const` — the current state: `kIdle`, `kReady` or `kUnavailable`.
- `[[nodiscard]] RE::TESObjectREFR* current_reference() const` — the live preview reference, or
  `nullptr` unless the state is `kReady` and the handle still resolves. Borrowed for immediate use;
  this is how the panel consumes the actor without becoming a second owner.

### `FrameHook`

- `bool install(Listener listener)` — registers the game-thread tick listener and installs the
  present hook once. Returns `false` (with a logged warning) while the swap chain does not exist
  yet; the caller retries on a later game message. Calling it again with another listener registers
  that listener and never writes a second hook.
- `bool install_present(PresentListener listener)` — registers a listener that runs on the render
  thread inside the present callback, before the game-thread task is queued. It must not block and
  must return without submitting any draw when it has nothing to draw.
- `FrameHook::Listener` is `void (*)()`, i.e. the game-thread listener runs with no arguments and
  must not retain engine pointers beyond the call. `FrameHook::PresentListener` is
  `void (*)(REX::W32::IDXGISwapChain*)` and additionally receives the swap chain.

### Configuration

INI file: `<game>/Data/SKSE/Plugins/PlayerPanel.ini`, section `General`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Enabled` | bool | `true` | Master switch. When `false`, the hotkey is ignored entirely |
| `Hotkey` | long (`0`..`254`) | `0x76` (F7) | Virtual-key code of the preview toggle; `0` disables the key |
| `PreviewDistance` | double | `150.0` | Game-unit offset in front of the player where the preview is placed |

Preview visibility is a runtime state and is never written to the INI: pressing the hotkey cannot
change what the next game session starts with.

## Invariants

- **The player is never mutated.** The preview owns a duplicated base record; the player's `TESNPC`,
  its sub-arrays (`headParts`, `tintLayers`) and its container are only read. No equip/unequip event
  is dispatched, replayed or synthesized for the player.
- **No engine pointer survives a frame.** The placed reference is retained as
  `RE::ObjectRefHandle` only. Each use re-resolves the handle with `get()` and treats an empty
  result as "no preview", never as a live one. A `RE::Actor*` is never stored.
- **Staged visibility.** A new reference is placed, immediately disabled, moved and dressed, and
  only then enabled, so no partially dressed frame can be rendered. Every path that reaches a
  dressed state also calls `Enable`.
- **The world copy is culled elsewhere.** `PreviewActor` itself only stages `Disable`/`Enable`
  around setup. Hiding the preview while the panel shows it is the panel's job: the panel culls the
  preview's in-world 3D root with the engine's own `SetAppCulled` switch from its game-thread
  prepare step. See [Preview panel](preview-panel.md).
- **One request, one attempt.** `service_request` consumes the pending request before acting, so a
  failure cannot re-arm itself; creation is never retried in a loop. A new attempt needs a new
  hotkey press, which FR-01 of the PRD explicitly allows after a failure.
- **States are honest.** Only `kIdle`, `kReady` and `kUnavailable` exist, and every transition logs
  one line with a human-readable reason. `kUnavailable` means the last attempt failed and nothing
  is on screen.
- **Thread ownership.** `request_toggle`, `on_frame`, `destroy` and `create` run on the game thread.
  The render thread only marks a frame; at most one dispatch task is in flight at a time, so the
  task queue cannot be flooded by a fast render thread.
- **Ticks are cheap.** `PreviewActor::on_frame` returns immediately unless a request is pending, so
  a frame without a request performs no engine query and no work.

## Failure modes

| Situation | Behaviour | Verification entry point |
| --- | --- | --- |
| No player character (main menu, during load) | `kUnavailable`, reason `no player character in this session`; nothing is created | `PreviewActor::create` guard; the logged `Preview state unavailable` line |
| Player base record missing | `kUnavailable`, reason `the player base record is unavailable` | `PreviewActor::create` guard; logged reason |
| `CreateDuplicateForm` returns nothing or a non-`TESNPC` form | `kUnavailable`, reason `duplicating the player base record failed` | `PreviewActor::create` duplicate check; logged reason |
| `PlaceObjectAtMe` fails | `kUnavailable`, reason `placing the preview reference failed` | `PreviewActor::create` placement check; logged reason |
| Placed reference is not an actor or has no handle | Reference released, then `kUnavailable`, reason `the placed reference is not a usable actor` | `PreviewActor::create` actor/handle check plus `PreviewActor::destroy`; logged reason |
| Player container changes unavailable | Reference released, then `kUnavailable`, reason `worn equipment is unavailable for synchronization`. This case is deliberately not softened into an undressed preview: a silently naked preview would misreport M1 | `sync_worn_equipment` null check; logged warning plus reason |
| Individual worn item refused by `AddWornItem` | Logged once as a count; the preview is still enabled, because a partial wardrobe is a cosmetic defect, not a functional failure | Warning `Preview could not wear N of M worn items` in `sync_worn_equipment`; manual checklist step 2 |
| Handle no longer resolves at destruction | Logged as a warning, the handle is cleared, state becomes `kIdle`; the stale handle is never revived | Warning in `PreviewActor::destroy`; manual checklist steps 4 and 7 |
| Session change (`kPreLoadGame`, `kNewGame`, `kPostLoadGame`) | `destroy()` runs and the panel is asked to release, so neither a preview nor a panel D3D object survives a load and no pending request survives it either | `message_handler` call sites; manual checklist step 7 |
| Listener throws inside the frame tick | Logged; `PreviewActor::on_frame` additionally resets the preview to `kIdle` so the plugin cannot be left in a half-created state | `FrameHook::tick` and `PreviewActor::on_frame` guards; log line `Preview toggle failed; the preview state was reset` |
| Swap chain not ready when the hook is installed | `FrameHook::ensure_installed` warns and returns `false`; the next session message retries the idempotent install | Warning in `FrameHook::ensure_installed`; `install_frame_hook` call sites in `message_handler` |

Known limitations of this batch, carried as open M1 questions rather than hidden behaviour:

- **Duplicated base records are not deleted.** `destroy()` releases the reference but leaves the
  duplicated `TESNPC` to the engine, because base-record ownership and lifetime are themselves one
  of the unverified items (PRD §9 item 1). If repeated toggles accumulate duplicate records, that
  observation is part of this batch's result and must be resolved before rendering work.
- **Deep-copy depth is unverified.** Whether the engine's duplication carries height, weight, sex
  and head parts exactly is an engine behaviour the pinned headers declare but do not define. The
  feature document therefore claims record-level inheritance, not fidelity.
- **`AddWornItem` semantics are unverified.** Whether every worn category (weapons, shields, ammo)
  ends up visibly equipped on a non-persistent actor is observable in game only.
- **`PreviewDistance` granularity.** `SimpleIni`'s double values are stored with `%f`
  (`SetDoubleValue`/`GetDoubleValue`), i.e. six decimals, and are parsed with `strtod` in the
  process locale. Values that do not round-trip or do not parse fall back to `150.0`.
- **Non-persistent does not prove save isolation.** The manual checklist below is what decides it;
  nothing in this document asserts it from code inspection.

## Dependencies

- `CommonLibSSE-NG` v8.0.1 (`extern/CommonLibSSE` submodule) — all engine and SKSE interfaces used
  here are declared by the pinned headers.
- `spdlog` (via the project logging alias `logger`) — state and failure logging.
- `SimpleIni` — INI storage for `Enabled`, `Hotkey` and `PreviewDistance`.
- No new dependency is introduced. `vcpkg.json` no longer declares `directxtk` and `rapidcsv`,
  which no CMake target referenced.

## Tests and verification

This project has no automated test target (`BUILD_TESTS` is `OFF`) and the host that builds it has
no Skyrim installation, so the compile-time checks below are the only automated evidence.

Configure and build (the exact commands run for this module; `cmake --preset Release` cannot be
used on the build host because the manifest's `builtin-baseline` commit is absent from the local
vcpkg clone, so the manifest install is disabled and the sibling project's installed tree is
reused):

```powershell
python build/lcw_run.py "C:/env/cmake/bin/cmake" -S . -B build -G "Visual Studio 17 2022" -A x64 -T "v143,version=14.44.35207" -DCMAKE_TOOLCHAIN_FILE="C:/env/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static-md -DVCPKG_INSTALLED_DIR="E:/SkyrimTools/Proj/HighlightLootableCorpses/build/vcpkg_installed" -DVCPKG_MANIFEST_INSTALL=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_FLAGS=/EHsc /MP /W4 /WX"
python build/lcw_run.py "C:/env/cmake/bin/cmake" --build build --config Release --parallel 2
```

Expected result: exit code 0 and `build/Release/PlayerPanel.dll`. `src` is collected with
`file(GLOB_RECURSE)` in `CMakeLists.txt` and has no `CONFIGURE_DEPENDS`, so **re-run the configure
step after adding or removing a source file**, before building. `PlayerPanel` compiles with
`/W4 /WX`, so any warning in first-party code fails the build; warnings from
`extern/CommonLibSSE` or `build/_deps` are pre-existing and are not part of this module's bar. If
MSBuild aborts with `C1076`/`C3859` (out of memory), rebuild with `--parallel 1`.

Static checks on the source of this module:

- `FrameHook::install` is the only caller of `PresentHook::install`, and `FrameHook` guards with
  `m_installed`, so a second `install` cannot write a second hook.
- `sync_worn_equipment` contains no `GetInventory()` call and no container write; it reads
  `InventoryChanges::entryList` and calls `AddWornItem` only for entries whose `IsWorn()` is true.
- `Config` carries no field initializers and `Setting`'s constructor and INI validation are defined
  in `src/config/config.cpp`, so no member is initialized outside a `.cpp` constructor.
- `README.md` and the workspace design-document index link this document and the feature index,
  and no repository file points at a feature page that does not exist.

Manual in-game checklist (not executed on this host; reported as `UNVERIFIED`):

1. Install the plugin DLL and let the game reach the main menu, then load a save in an exterior
   cell.
2. Press the configured hotkey (F7 by default). Expect the panel window to appear — its own checklist
   is in [Preview panel](preview-panel.md) — and **no second character to appear in the world**: the
   preview's in-world copy is culled while the panel shows it. A character that appears in the world
   instead of (or as well as) in the window means the visibility change did not take effect.
3. Look at the plugin log (`Documents/My Games/Skyrim Special Edition/SKSE/PlayerPanel.log`).
   Expect one `Preview state ready:` line and no `unavailable` line.
4. Press the hotkey again. Expect the window to disappear and a `Preview state idle:` line.
5. Repeat step 2 and step 3 ten times. Expect no accumulation: each cycle leaves exactly one preview
   (shown in the window, not in the world) while visible and none while hidden, and the log shows no
   failed transitions.
6. Equip a different armour set, then toggle the preview on. Expect the character in the window to
   wear the new set (equipment is mirrored at creation time, not continuously).
7. Save the game, return to the main menu and load that save. Expect no window and no preview after
   loading, no preview character in the saved world, and `Preview state idle:` logged at
   `kPreLoadGame`.
8. Toggle the preview ten times, then save. Expect the save file size not to grow with the number
   of toggles.
9. Set `Enabled=false` in `PlayerPanel.ini`, restart, and press the hotkey. Expect nothing to
   happen; `Enabled=true` must be restored for the hotkey to work again.
10. Set `PreviewDistance=300.0`, restart, and toggle. The panel framing is derived from the preview's
    world bound, so where the preview stands is no longer visible to the user: the check is that the
    preview is still created (log line) and that the panel still frames the whole body.
11. Walk into a spot where the player is against a wall and toggle. Expect the preview to be created
    and the panel to show it anyway (its placement is a fixed offset, not a collision check).

## Safe modification guidance

- Extend `PreviewActor::State` only with a new logged transition; never add a silent state, and keep
  `kUnavailable` meaning "nothing is on screen".
- Do not call engine functions from `request_toggle`, `FrameHook::on_present` or the input sink. Add
  the request or the flag there and do the work in the frame tick, which runs on the game thread.
- Never store a `RE::Actor*`, `RE::TESObjectREFR*` or `RE::NiAVObject*` in `PreviewActor` or capture
  one in a queued task. Resolve `m_handle` at the point of use.
- Keep the creation order place → disable → dress → enable; enabling before dressing shows a
  partially dressed frame, and re-enabling with `a_resetInventory=true` discards the copied items.
- Do not replace the direct `InventoryChanges::entryList` walk with `GetInventory()`: that call can
  initialize and merge the player's inventory changes and returns copies, which would both mutate
  player state and slow the operation down.
- `PresentHook` owns the only hook write. Add per-frame behaviour through `FrameHook::install` with
  another listener instead of installing a hook of your own.
- Registration beyond `Max_Frame_Listener_Count` is refused and logged; raise the constant
  deliberately if a module genuinely needs a slot.
- The panel renders this actor as a **consumer**: it reads `current_reference()` and never creates,
  dresses or destroys anything. Keep this module the actor lifecycle owner and keep creation out of
  the render path.
- Do not add a second `SetAppCulled` call site. Hiding the world copy belongs to the panel's
  game-thread prepare step, and one owner is what keeps that switch auditable. See
  [Preview panel](preview-panel.md).

## Synchronized files

Changes to this module must update these files together:

- `src/preview/preview_actor.h` and `src/preview/preview_actor.cpp` — the state machine and engine
  calls.
- `src/render/frame_hook.h` and `src/render/frame_hook.cpp` — the frame tick that services requests.
- `src/config/config.h` and `src/config/config.cpp` — `PreviewDistance`, its default and the
  `Enabled`/`Hotkey` master-switch semantics.
- `src/input/input.cpp` — the master switch and the toggle request.
- `src/main.cpp` — hook installation and the per-session destruction.
- `docs/features/README.md` — the feature index entry for this document.
- `CHANGELOG.md` — the user-visible entry under `Unreleased`.
- `vcpkg.json` — only if the dependency set changes; this module needs no dependency.

## Related history

- PRD [`independent-character-panel-prd.md`](../../../docs/independent-character-panel-prd.md):
  FR-01/FR-02/FR-03, FR-07, M1 acceptance, §9 items 1-3, §10 reference table, §11 rows 1-3.
- Local CommonLibSSE-NG references:
  [`commonlibsse-ng-reference.md`](../../../docs/commonlibsse-ng-reference.md) §4.3
  (form/reference/handle ownership) and §5.1 (inventory queries are not free of side effects), and
  [`commonlibsse-ng-recipes.md`](../../../docs/commonlibsse-ng-recipes.md) §6 (deferred reference
  handling with `ObjectRefHandle` and `TaskInterface::AddTask`).
- Contract `.lcw/task-runs/m1-preview-actor/implementation-contract-v1.md`: this batch,
  revision 1 — preview actor creation, isolated appearance/equipment sync and destruction.
- The repository had no feature document before this one, and the project
  [`README.md`](../../README.md) together with the workspace design-document index previously
  pointed at a feature page that never existed. Both now link the
  [feature documentation index](README.md) and this document instead.
