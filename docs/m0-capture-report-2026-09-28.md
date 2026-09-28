# M0 probe capture report — 2026-09-28

User-run capture session of `CharacterPanelProbe` v1.0.0 (build identity
`CharacterPanelProbe-probe-v1-2026-09-28`) on Skyrim AE 1.6.1170. Evidence:
five capture folders under
`Documents/My Games/Skyrim Special Edition/SKSE/CharacterPanelProbe/`
(capture-000..004) plus `CharacterPanelProbe.log`, session 21:58–22:02.
This report retires the "user scenarios remain unverified" note in
[m0-diagnostics.md](m0-diagnostics.md) for the Community-Shaders-enabled
scenario; the vanilla-renderer scenario was not run this session.

## Session summary

- Runtime gate passed (`runtime_supported=true`, version 1-6-1170-0).
- Window 2560x1440; inventory open and game paused for all F7 captures.
- **Community Shaders was loaded** (`CommunityShaders.dll` at
  `0x7FFDBE270000`), so this session covers scenario 2 of the diagnostics
  document.
- Captures 000–003 are F7 (code + menu) taken inside the inventory; capture
  004 is F8 (menu metadata only) taken after closing the inventory.
- All five captures finished with `metadata_incomplete=false`. The F7
  captures report `capture_status=partial` solely because eight leaf
  routines have no unwind info and are capped at 256 bytes by design; the
  F8 capture is `complete=true`.
- Captures 000–003 are byte-identical across all 6751 code bytes and all
  metadata (only `capture_index` differs): addresses were stable across
  four minutes and repeated triggers.
- The probe log shows no errors and the session ended normally.

## Key findings for the blocked M0 contract

1. **The current accumulator is a plain global pointer slot.**
   `BSShaderAccumulator::GetCurrentAccumulator` sits at module offset
   `0x1480B20` and `SetCurrentAccumulator` at `0x1480B30` — 16 bytes apart,
   so the getter is a ~16-byte leaf function (typical `mov rax, [global]`).
   Swapping in a private accumulator, accumulating, and restoring requires
   no vtable patching and is the engine's native switch path. This directly
   addresses the second half of the M0 blocker (submitting an accumulator
   into a private color/depth target).

2. **The inventory renders through the secondary accumulator, not the
   primary.** Both accumulators share one dynamic type (identical vtable
   slots 0x25/0x26/0x27/0x2A/0x2B) and are idle at capture time
   (`current_active=false`), but only the secondary has a camera bound (the
   shared `ui3d.camera`) and `render_mode=12`; the primary has
   `camera=null` and `render_mode=0`. Any M0 prototype that swaps
   accumulators should target the secondary slot and compare against it.

3. **Community Shaders 1.9.1 does not redirect any monitored path.** All
   15 contract routines and all 13 captured vtable-slot functions resolve
   inside `SkyrimSE.exe`; none point into `CommunityShaders.dll`. M0
   evidence gathered under CS remains valid for the stock pipeline.

4. **The `CullingContext` constructor is at most 112 bytes.** It was
   captured partial (256-byte leaf cap) at offset `0x14BF2B0`, but the next
   function, `BSCullingProcess::Process`, starts at `0x14BF320`, bounding
   the real size. A small helper — independent construction is feasible.

## Verified routine offsets (SkyrimSE 1.6.1170, module-relative)

All fifteen contract routines resolved inside `SkyrimSE.exe` with sane
bounds; fourteen match CommonLibSSE-NG 9.1.0 source bindings exactly.

| Routine | Offset | Bytes | Status | CLib match |
|---|---|---|---|---|
| `BSCullingProcess::CullingContext` ctor | 0x14BF2B0 | 256 (capped) | partial | ID 100211, yes |
| `BSCullingProcess::Process` | 0x14BF320 | 485 | complete | ID 100213, yes |
| `BSShaderAccumulator` ctor | 0x14B1CF0 | 717 | complete | ID 99920, yes |
| `GetCurrentAccumulator` | 0x1480B20 | 256 (leaf-capped) | partial | ID 98997, yes |
| `SetCurrentAccumulator` | 0x1480B30 | 256 (leaf-capped) | partial | ID 98998, yes |
| `Renderer::SubmitAccumulator` | 0x14A90F0 | 144 | complete | ID 99789, yes |
| `Renderer::StartAccumulating` | 0x14A9190 | 130 | complete | ID 99790, yes |
| `Renderer::FinishAccumulatingPostResolveDepth` | 0x14A9220 | 256 (leaf-capped) | partial | ID 99791, yes |
| `Inventory3DManager::Begin3D` | 0x927410 | 322 | complete | ID 50881, yes |
| `Inventory3DManager::Render` | 0x927560 | 181 | complete | ID 50882, yes |
| `Inventory3DManager::End3D` | 0x927620 | 541 | complete | ID 50883, yes |
| `UI3DSceneManager::AttachChild` | 0x9736C0 | 160 | complete | ID 51859, yes |
| `UI3DSceneManager::DetachChild` | 0x973790 | 169 | complete | ID 51861, yes |
| `NiObject::Clone` | 0xD17E90 | 199 | complete | ID 68835, yes |
| `MenuManager::DrawInterfaceStart` | 0xFA4F00 | 1013 | complete | ID 79947, not bound by CLib; capture self-validates (unwind bounds, executable PE section) |

vtable-slot captures: culler slots 0x16/0x17/0x18 and accumulator slots
0x25/0x26/0x27/0x2A/0x2B for both accumulators all resolved into
`SkyrimSE.exe`. Camera frustum for the menu view: near/far 15/20480, plane
extents match 2560x1440, world translate at origin — reproducible.

## Anomalies and caveats

- **`culler.cull_mode=0xA8E372F8` is not a valid `BSCPCullingType`**
  (valid range 0–4). The neighboring base-class fields read sane values
  (camera equals `ui3d.camera`, `visible_set=null`,
  `update_accumulate=false`), so the `NiCullingProcess` base layout is
  trustworthy; the suspect field is `BSCullingProcess::cullMode`, declared
  at offset 0x30198 right after the `roomSharedMap` BSTHashMap. The garbage
  value looks like the low 32 bits of a heap pointer, consistent with the
  map's real size differing from the declared layout on 1.6.1170. **Treat
  BSCullingProcess extension offsets beyond 0x128 as unverified until
  cross-checked against a second source (e.g. CGIDB/NGX layout dumps).
  The probe's reads stay bounded and this did not affect other evidence.**
- `current_active=false` / `update_accumulate=false` are idle-state
  snapshots taken while the game was paused; they do not describe behavior
  mid-pass.
- `renderer.main_desc` is by design
  `unavailable_without_stable_texture_reference` for a read-only probe.
- Scenario 1 (CS disabled) was not run this session; given finding 3 it is
  low value but still listed in the diagnostics document.

## Next steps toward M0

1. Prototype the accumulator swap using the verified offsets:
   `SetCurrentAccumulator(private) → cull+accumulate into an offscreen
   target → restore`, without a Present hook first.
2. Cross-check `BSCullingProcess` extension layout before constructing an
   independent culler; optionally widen the single 256-byte leaf cap for
   the `CullingContext` ctor (its true end is already bounded at +112).
3. Bind `MenuManager::DrawInterfaceStart` in a follow-up probe iteration
   only if menu-draw interception becomes relevant to the design.

## Addendum: BSCullingProcess extension layout cross-check (2026-09-28)

Cross-checked against CommonLibSSE-NG 9.1.0 history and headers:

- The `roomSharedMap` BSTHashMap at 0x30160 replaced six unknown qwords in
  a Sept-2025 upstream commit (16112def7); the declared table is 0x28
  bytes (0x18 standard parent + 0x10 heap-allocator storage), making the
  next member `portalGraphEntry` sit at 0x30190 and `cullMode` at 0x30198
  — matching the current header.
- The probe's own build passed the header's
  `static_assert(sizeof(BSCullingProcess) == 0x301F8)`, so the mismatch is
  in the runtime class, not the compile-time layout.
- Conclusion: the culler captured by UI3DSceneManager on 1.6.1170 is
  either a class larger than 0x301F8 (shifted tail members) or
  `cullMode` at 0x30198 holds engine-internal data on this runtime. Until
  a second source (NGX/CGIDB dump or a targeted probe read of the
  cullModeStack region) confirms it, all BSCullingProcess members beyond
  the NiCullingProcess base (0x128) must be treated as unverified for
  1.6.1170. The M0 accumulator-swap path (finding 1) does not depend on
  any of these offsets.
