# Plan: render the panel character through the engine's own shading (2b)

Status: proposed — not implemented

Canonical path: `docs/plans/panel-render-2b.md`

Opened: 2026-09-27. Revised twice: `Inventory3DManager` rejected (item path, no skinning); then all
degenerate routes removed after the requirement was restated.

## The requirement

A BG3-like character panel: the player's character is drawn **inside the panel**, and inside that
panel the player can

1. **change the pose / animation**, and
2. **switch equipment**.

Everything below is derived from those two capabilities plus the M1 contract already in
`docs/features/preview-panel.md`. Any path that cannot deliver them is not listed as an option.

### What that pins down

- The character must **look like the character**: skin tone, hair dye, materials, morphs. A path that
  only gets the silhouette right is not a BG3-like panel, it is a paper doll.
- The character must be **skinned and animatable**. Posing is not "we redraw a static mesh"; it is the
  actor's animation moving its bones, and the panel must follow.
- Equipment changes must **take effect on the character in the panel**, which means the panel has to
  pick up 3D nodes that appear after the engine rebuilds the preview's worn 3D.
- The panel keeps FR-01: the world's camera, lighting, time of day and weather do not influence it,
  the panel fully occludes whatever the world drew behind it, and the world copy of the preview stays
  culled so the character is never shown twice.

## Why the current implementation is not that

`collect_panel_geometry` + `PanelGeometryPass` + `panel_geometry.hlsl` (path 2a) *can* follow poses and
equipment — it re-collects every frame and rebuilds its bone palette from live
`boneWorldTransforms` — but it cannot deliver the first bullet. A character's appearance in Skyrim is
produced by `BSLightingShader` and its material family, not by a diffuse texture: FaceGen materials
(`kFaceGen` 4, `kFaceGenRGBTint` 5) tint the sampled texture by the actor's skin tone — which is why
the head (a baked FaceGen texture) reads correctly while the body, hands and feet (shared skin
material plus tint) read as flat grey — and then the tint mask, the gloss path, vertex colours, the
environment map, two-sided lighting and the per-mesh alpha handling sit on top. RenderDoc confirms the
UVs are correct, which is the clearest possible statement that the geometry is fine and the *material
semantics* are what is wrong. Chasing those one at a time has no finish line.

## The path: accumulate the preview's geometries with the engine's own pass

Render the preview's `BSGeometry` nodes through the engine's render pass into the panel's own target,
with the panel's own camera and the panel's own lights. Shading, skinning, materials, morphs and
tinting are then the engine's, not ours.

### Precedent: honest status

**There is no published community mod that does exactly this** — take an actor's geometries,
`MakeRenderPass` them and draw them into the plugin's own target with the plugin's own camera. It has
not been found, and the plan should not pretend otherwise: Stage 1 is a research bet on engine
internals, not a copy of a known mod. What does exist, and is what the bet rests on:

1. **The per-pass draw entry is known, and a shipped mod already sits on it.**
   `BSBatchRenderer::RenderPassImmediately(BSRenderPass*, uint32_t technique, bool alphaTest, uint32_t
   renderFlags)` — Community Shaders hooks it in three features (`InteriorSun`, `LightLimitFix`,
   `TerrainBlending`) at `REL::RelocationID(100852, 107642) + REL::Relocate(0x29E, 0x28F)`. A published
   CS crash log shows the whole chain: `FinishAccumulating → RenderBatches → RenderPassImmediately →
   ApplyPassAlphaCullState`. So "a plugin controls how an individual pass is drawn" is established
   practice; what is **not** established is calling it on a pass we built ourselves.
2. **The engine already renders with other cameras into targets.** Water reflections
   (`RENDER_TARGET::kWATER_REFLECTIONS`), the local map (`kLOCAL_MAP`), and cubemaps
   (`RE::BSCubeMapCamera`, `cubemapRenderTargets`). Community Shaders' `DynamicCubemaps` reads the
   engine's cubemap capture — the pipeline is observable and hookable.
3. **"Live actor with poses and equipment" is itself proven.** `Show Player in Menus` has the engine
   render the real player in menus, and animation and worn equipment follow. It does not solve the
   rectangle; it does prove the requirement is satisfiable in this engine.

### Engine interfaces (confirmed in `extern/CommonLibSSE` and in this workspace)

- `BSShader::MakeRenderPass(BSShaderProperty*, BSGeometry*, uint32_t technique, uint8_t numLights,
  BSLight** lights)` — `RE/B/BSShader.h`, `RELOCATION_ID(100717, 107497)`. One pass per `BSGeometry`,
  with the light set supplied by the caller: that is how FR-01's lighting independence survives.
- `RE::BSRenderPass` — `RE/B/BSRenderPass.h`: `shader`, `shaderProperty`, `geometry`, `passEnum`,
  `accumulationHint`, `numLights`, `sceneLights`, …
- `RE::BSShaderAccumulator` — `RE/B/BSShaderAccumulator.h`: `Create(uint32_t)`,
  `GetCurrentAccumulator()`, `SetCurrentAccumulator()`, `StartAccumulating(NiCamera*)`,
  `FinishAccumulating()`.
- `BSGraphics::State::SetCameraData(const NiCamera*, uint32_t)` — `RE/S/State.h`; `State::camViewData`
  also carries the view/proj matrices directly (VR: array of two).
- `BSGraphics::Renderer::SetRenderTarget(slot, RENDER_TARGET, mode, bool)` — `RE/R/Renderer.h`.

### Why this covers skinned meshes and poses

A `BSRenderPass` is built **for a `BSGeometry`** and consumes that geometry's own `NiSkinInstance`,
whose bone matrices the actor's animation maintains every frame. We never compute skinning, so we
never have to reproduce it — and whatever pose the animation produces is what the panel draws. The
whole calibration apparatus that exists only to feed our own shader (vertex-layout calibration, skin
layout calibration, `build_palette`, UV/format guessing, per-material and tint resolution) dies with
it.

### How poses are driven

`RE::Actor` exposes the animation-graph entry points — `Precision` in this workspace calls
`actor->NotifyAnimationGraph("…")` and `actor->SetGraphVariableFloat("…", v)` on `RE::Actor*`
(`Precision/src/Hooks.cpp`, `PendingHit.cpp`). The preview is a live placed actor, so sending it a
graph event or setting a graph variable moves its bones, and the panel's next accumulation follows.
The world copy being culled (`SetAppCulled`) affects rendering only, not animation.

### How equipment is switched

`RE::ActorEquipManager::GetSingleton()` — `RE/A/ActorEquipManager.h` — with
`EquipObject(actor, object, extraData, count, slot, queueEquip, forceEquip, playSounds, applyNow)` and
`UnequipObject(...)`. Applied to the preview reference with `applyNow = true`, silent and unqueued,
the engine rebuilds its worn 3D. Because the geometry enumeration runs every frame, the new nodes are
picked up without a restart.

The known wrinkle: a rebuild is not instantaneous — the tree already has trouble in this window
(`the preview 3D is not available`, and the bone-matrix latch that waits for the first submission).
Panel-side, that needs a settle guard so a rebuild cannot flash an empty or half-dressed panel; the
existing `Max_World_Copy_Grace_Frames` latch is the precedent for how this plugin handles it.

## Why no other path is listed

- **`Inventory3DManager`** (item preview): `LoadInventoryItem` takes a `TESBoundObject`; its framing is
  object-shaped (`boundRadius`, `zoomDistance`, `itemScale`). It carries no skinned actor and no
  animation. Fails both capabilities.
- **Main scene + viewport/scissor** (let the engine draw the world and clip it to the panel): poses and
  equipment would work, but it is not a panel — the world behind the preview is inside the rectangle,
  and the panel stops being independent of the world's camera state. Degenerate; excluded.
- **Keep 2a and narrow it to silhouettes**: poses and equipment would work, appearance would not. Not
  the requirement; excluded.
- **Separate cell for the preview**: Skyrim renders the current world; there is no multi-scene, so this
  means changing the current cell or moving the player — invasive to save/state, and it inherits that
  cell's lighting and weather. Excluded.

Consequence: if the engine's pass cannot be driven off-frame, that is a product-level decision to
reopen (the requirement cannot be met by re-rendering), not a signal to fall back to a degraded panel.

## Stages

### Stage 0 — a runnable spike, then the gate

Given there is no precedent, the gate has to be *executable*, not observational. One debug build, one
trip into the game:

1. bind the panel's own target and our own camera (`SetRenderTarget`, `SetCameraData` / `camViewData`);
2. pick one skinned body geometry of the preview and `MakeRenderPass` it with our own lights;
3. draw it with `BSBatchRenderer::RenderPassImmediately(pass, technique, alphaTest, renderFlags)` —
   the entry Community Shaders already hooks;
4. dump the target to a TGA (the module already dumps textures) and look at it.

**Gate.** Pixels of that geometry in the target → Stage 1. Nothing, or the wrong geometry → stop and
reopen the requirement; do not fall back to a degraded panel.

RenderDoc still has a supporting job: capture a normal world frame and record, for that same geometry,
the `technique` / `passEnum` actually used and whether the light array handed to `MakeRenderPass` is the
scene's (confirming we may substitute our own). Those numbers feed the spike, not the other way round.

### Stage 1 — prototype

Save state → bind the panel target → set the camera → `Create`/`SetCurrentAccumulator`/
`StartAccumulating` → one `MakeRenderPass` per `BSGeometry` of the preview, with our lights →
`FinishAccumulating` → restore. Prototype must show, in order: correct materials; a live pose change
via `NotifyAnimationGraph`; and an equipment change via `EquipObject`/`UnequipObject` followed by a
rebuild without flashing.

Unknowns the prototype resolves: which accumulator type `Create` needs; the right `technique` per
material (Stage 0 answers it); whether `FinishAccumulating` honours a target we bound; whether the
preview's world transforms need an explicit update before accumulating.

### Stage 2 — land it

Keep the composite, inset, chrome, drag, layout and resolution behaviour as they are; express the
panel camera as the accumulator's camera; then delete the retired machinery — `collect_panel_geometry`'s
calibration, `PanelGeometryPass`, `panel_geometry.hlsl` — in its own change.

## Acceptance criteria

1. Skin, hair and clothing match the world character's in the same frame; no plugin-side material,
   tint or lighting constants.
2. A graph event sent to the preview changes the pose in the panel on the next frame.
3. `EquipObject` / `UnequipObject` on the preview changes what the panel shows, with no empty or
   half-dressed frame during the 3D rebuild.
4. Panel lighting is ours, so time of day and weather cannot influence it; the panel still occludes the
   world behind it and still composites inside the skin's inset.
5. The world copy stays culled; opening/closing still goes through `PanelMenu` only.
6. Still drags, still sizes identically on 16:9 / 21:9 / 32:9; no per-frame allocation growth; no new
   steady-state log lines.

## Non-goals

- Choosing *which* poses and which equipment the panel offers, or building that UI: this plan covers
  the rendering and the two mechanisms, not the inventory/pose browser.
- Inventory-menu integration or moving the vanilla item preview.
- Changes to the SWF skin contract, the configuration keys or the layout rules.
- Reproducing `BSLightingShader` in our own HLSL — the thing this plan removes.

## Risks

- The accumulator contract (the `Create` argument, the `technique`, whether `FinishAccumulating`
  honours a bound target) is only observable; Stage 0 is a real gate.
- The pass/accumulator addresses are runtime-versioned through CommonLibSSE
  (`RELOCATION_ID(100717/100718, …)`); the prototype must be exercised on every shipped runtime,
  including VR (`camViewData` is a two-element array there).
- 3D rebuild timing is already a sore spot in this module; the settle guard has to be part of Stage 1,
  not a follow-up.

## Files likely touched

`src/render/panel/panel_passes.*` (geometry pass replaced by the accumulator hand-off),
`panel_renderer.cpp` (target/camera/composite ordering), `panel_target.*`, `panel_camera.*` (expressed
as a `NiCamera` / view-proj for the accumulator), and the deletion set named in Stage 2.
`preview_actor.*` grows the pose and equipment entry points. `panel_menu.*`, `input/*`, `config/*` and
the SWF tooling stay untouched.
