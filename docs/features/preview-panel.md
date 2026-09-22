# Preview panel

Status: current

Canonical path: `docs/features/preview-panel.md`

Last verified against: working tree on 2026-09-21, baseline revision
`9bb543db3d61f60826671974cc828472c0f2d8c2`

M1 rendering module of the independent character panel: a hotkey-toggled on-screen window that
renders the preview character, its face and its worn equipment offscreen with a dedicated camera and
fixed lighting, composites the result over the panel rectangle, sizes itself from the render height
so it reads the same at every aspect ratio, takes its chrome — its backdrop *and* its frame — from a
swappable Scaleform SWF loaded through a registered menu, and is dragged with the game's own menu
cursor.

## Purpose

The plugin must show the player's own character in an independent panel rather than in the world
(PRD [`independent-character-panel-prd.md`](../../../docs/independent-character-panel-prd.md),
FR-01/FR-02 and M1). The [Preview actor](preview-actor.md) module proved that the engine can carry a
second, non-persistent actor that mirrors the player's appearance and worn equipment. This module
closes the M1 contract:

1. the preview is rendered into a **private** colour + depth target with its own camera, so the
   engine's camera, lighting, time of day and weather cannot influence it;
2. the result is composited into a window that **fully occludes** whatever the world drew behind it —
   in the fallback the plugin's opaque fill does that, and with a skin the skin's own opaque backdrop
   does, because the character is alpha-blended over the chrome the engine has already drawn rather
   than painted as an opaque rectangle;
3. the in-world copy of the preview is made invisible through the engine's own visibility switch, so
   the same character is never shown twice;
4. the window is a **placeable** window: its size and margins are fractions of the render height, so
   it keeps the same relative size and right-edge distance on 16:9, 21:9 and 32:9 screens, its
   position is stored as a fraction of the free space so it survives a resolution change, and it can
   be dragged with the mouse like a game UI.

Animation, display poses and inventory integration stay out of scope; the panel shows one static,
dressed character in one rectangle. The panel's *chrome* — its frame and background — is no longer the
plugin's own art by default: it comes from a Scaleform SWF at `Interface\PlayerPanel\panel.swf` that a
UI reskin mod replaces the same way it replaces a vanilla menu's movie, with no plugin change and no
effect on the character or on game logic. A skin therefore supplies both halves of the panel's look: an
**opaque backdrop** that fills its stage and a **frame** at least as thick as the configured inset, and
the character is composited over that backdrop and inside that frame. A skin with a transparent
backdrop shows the world through the panel; that is the skin's responsibility, not a plugin defect.

## Scope and non-goals

In scope:

- a private offscreen target (`R8G8B8A8_UNORM` colour plus `D32_FLOAT` depth) sized from the resolved
  panel rectangle;
- a panel camera with its own forward-Z perspective projection, its own `LESS` depth state and a
  two-axis automatic framing fit;
- geometry collection for the preview's static and skinned meshes, extended with the per-mesh
  material needed for shading;
- purpose-written HLSL for the static and skinned vertex paths, the panel pixel shader and the
  composite;
- a resolution-relative panel rectangle derived from the render height and the configured aspect, with
  a normalised stored position and the right-anchored, vertically centred default;
- a registered Scaleform menu (`PanelMenu`, one `IMenu` subclass registered once with `RE::UI::Register`
  under one stable name) opened and closed through the engine's UI message queue in step with the
  preview, so the engine drives the menu cursor and the menu's input context;
- the panel's chrome loaded from an overridable SWF path through `BSScaleformManager::LoadMovie`, with
  the plugin scaling the movie into the panel rectangle with `GFxMovieView::SetViewport` so a skin
  needs no ActionScript;
- mouse dragging of the panel against `RE::MenuCursor`, in render pixels, with the grab offset and the
  screen clamp preserved;
- a deliberately minimal built-in layout as the fallback: the opaque fill plus one proportional
  hairline border, used only while no movie is available;
- the character composite inset by a **skin inset** of its own, a configured fraction of the panel
  height with an absolute pixel floor, so the SWF's frame band is never overdrawn and the skin's
  backdrop stays visible around the character;
- the skin's own opaque backdrop and frame, drawn by the movie and left in place by the composite: in
  skin mode the private target is cleared fully transparent and the character is composited with
  source-alpha blending over the chrome the engine drew in its UI pass, so no fragment the character
  does not cover is written at all;
- a checked-in standard-library generator for the default skin SWF, and its generated artifact;
- the configuration key migration and its validation;
- the world-copy visibility change: the preview's in-world 3D root is culled with the engine's own
  switch.

Explicitly unsupported (recorded so they are never silently implied):

- **Script, game logic and font rendering inside a skin** — a skin only supplies art. The
  generated default skin contains no action tags at all, the plugin positions and scales the movie
  itself, and neither the plugin nor the panel requires any script in a skin. No text drawing, no font
  rendering, no title bar and no menu hierarchy are implemented.
- **A plugin-supplied backdrop behind a skin** — the composite never fills the rectangle while a skin
  owns the chrome, because any fill it painted would cover the movie the engine had already drawn. The
  panel's opacity in skin mode is the skin's own opaque backdrop; a skin that omits one shows the world
  through the panel.
- **Per-pixel coverage over the character** — the composite blends the character over the skin, so a
  skin can only be seen behind and around the character, never on top of it.
- **Automatic shadow, blur, rounded corners or nine-slice scaling in the plugin** — a skin's artwork is
  its own business; the plugin supplies no decoration beyond the built-in fallback's fill and hairline.
- **A replica of the vanilla UI look in the plugin's own art** — the panel's *built-in* chrome stays the
  deliberately minimal opaque fill plus one hairline border, whose colour is the named constant
  `Panel_Border_Color` in `src/render/panel/panel_renderer.cpp`. Community-style reskinning is served
  by the SWF path instead of by plugin art, which is exactly why that path is the public interface. No
  rounded corners, no glow, no drop shadow, no gradient and no Bethesda artwork are shipped or
  extracted.
- **Animation, idle selection and display poses** — the panel shows whatever pose the preview's 3D
  currently has; nothing drives it.
- **Character rotation, zoom or scroll inside the panel** — dragging is the only interaction. A future
  rotation gesture cannot simply take the left button again: the whole panel surface is the drag
  handle today, so a rotation affordance needs a different gesture (a second button, a modifier or a
  dedicated region) and that is a new decision, not a silent overload.
- **Inventory-menu integration, input focus and repositioning of the vanilla item preview** — not
  touched.
- **Hand-pushed input contexts and manual control freezing** — the panel registers exactly one menu
  and then lets the engine do the work: it calls neither `PushInputContext` nor `PopInputContext` and
  never calls `ToggleControls`. The menu declares `kUsesCursor`, and the engine's own UI input context
  is what keeps the camera from rotating while the panel is open.
- **Pausing the game while the panel is open** — `kPausesGame` is deliberately not set, so the world
  keeps running.
- **Becoming the top-most modal menu** — `kModal` is not set, so gameplay keeps running behind the
  panel.
- **New engine hooks** — the panel runs inside the existing Present hook; no second hook and no
  engine render-pass change exists.
- **FaceGen tint, RaceMenu morphs and body-slide** — unchanged from [Preview actor](preview-actor.md);
  this module changes how existing geometry is shaded, not which geometry exists.
- **A fix for the pre-existing `D9025` warning** — that compile option is outside this batch's
  authority.

## Architecture

```
game thread
    InputHandler::ProcessEvent ──> PanelRenderer::on_left_button   (records the button edge only)
                                └─> PreviewActor::request_toggle() (hotkey, records only)

    FrameHook::tick() ──> PreviewActor::on_frame()   (services the toggle request)
                       └─> PanelRenderer::on_frame() (prepare: menu show/hide, chrome mode, movie
                                                      viewport, layout, drag from MenuCursor,
                                                      camera, geometry)

    engine UI tick ──> UIMessageQueue ──> PanelMenu  (the engine creates the menu, loads its movie,
                                                      shows and hides it, advances and draws it)

render thread (inside the swap-chain Present callback)
    PresentHook::present_thunk ──> FrameHook::on_present()
                                     ├─ present listeners ──> PanelRenderer::on_present() (draw)
                                     └─ mark frame, queue one game-thread task

session messages (game thread)
    message_handler ──> ShaderManager::compile() (early attempt; usually skipped, no device yet)
                        + PanelMenu::install() + PanelRenderer::set_panel_open(false)
                        + PanelRenderer::request_release() + PanelMenu::set_open(false)
                        + PreviewActor::destroy()

PanelRenderer::draw()
    ├─ ensure device objects: ShaderManager::compile() (guaranteed attempt), then the states and the
    │  constant buffers                                                       render-thread, on demand
    ├─ resolve the rectangle, rebuild the target when device or size changed
    ├─ choose the chrome's clear and inset: opaque fill and the built-in hairline, or a fully
    │  transparent clear and the configured skin inset
    ├─ PanelGeometryPass::draw()   offscreen pass into the private target
    ├─ PanelCompositePass::draw()  the built-in fill and hairline when no skin is loaded, with blending
    │                              disabled; otherwise only the character, alpha-blended over the movie
    │                              the engine already drew and inset by the skin inset, with blending
    │                              enabled
    └─ everything wrapped in one D3D11StateCapture
```

The three threads and roles are split by ownership, not by convenience:

- **The game thread collects and owns player input.** `PanelRenderer::on_frame` asks `PreviewActor`
  for the current reference, shows or hides the panel's menu, resolves the rectangle, sets the movie's
  viewport, reads `RE::MenuCursor` for the drag, builds the camera from the preview's world bound and
  heading, and walks the preview's 3D with `collect_panel_geometry`. That traversal reads the live
  scene graph, which the game thread owns, and it is the only place the world copy is culled. Every
  call that touches the menu or Scaleform happens here, on the game thread, and nowhere else.
- **The render thread draws.** `PanelRenderer::on_present` runs inside the Present callback, where
  the D3D11 device and immediate context belong. Every device object, every state change, every
  draw call and the deferred release happen there and nowhere else.
- **The input sink only records.** `InputHandler::ProcessEvent` is an engine callback, so it performs
  no engine mutating call, no device call and no window call: it records the left-button edge and a
  hotkey request, and the frame tick turns them into a drag and a toggle. The panel does not consume
  the events, so the game keeps seeing the same mouse input it always did.
- **The handoff is a buffer swap.** `m_prepare_draws` is game-thread only, `m_render_draws` is
  render-thread only, and `m_draws` is the guarded slot the two swap through. Each of the three
  keeps its capacity, so a steady-state frame allocates nothing; `m_mutex` is held only for the two
  swaps, never across the scene-graph walk or across a D3D call.
- **The layout is a pure function.** `resolve_panel_layout` computes the rectangle from the render
  size and the configuration alone and reads no engine state, so the game thread (hit test, drag) and
  the render thread (viewport, target rebuild) agree without sharing a value.

Constant-buffer budget: the panel binds only constant-buffer slots 0 and 1 of the vertex stage and
slot 0 of the pixel stage, which is exactly the set `D3D11StateCapture` restores. The skinning
palette therefore shares the per-draw buffer with the per-draw transform, and the composite's inset
parameters ride in the composite pass's existing pixel-stage slot 0; a wider slot would not be
restored, and the ported capture set must not shrink. The composite pass owns both of its blend
states — the disabled one for the built-in chrome and the source-alpha one for a skin — created with
its other resources, released with them, and bound inside the caller's existing state-capture pair.

Shader compilation is a lifecycle step of its own, not part of a pass. `ShaderManager::compile()` is
attempted when the data is loaded, where the D3D11 device usually does not exist yet and the attempt
therefore only reports that it was skipped, and the attempt that counts happens in
`PanelRenderer::ensure_device_objects` on the render thread, once the renderer has a device and before
either pass' `init()` reads the manager's accessors. `compile()` is idempotent, so the second attempt
only does work until the first success, and the first success logs exactly `All panel shaders compiled`
once. Its failure is reported once, is remembered for that device so
it is not retried once per frame, and leaves the frame skipped rather than submitting work with null
shaders. Nothing in the build reports a missing call: the embedded HLSL becomes unreferenced and the
linker drops it silently, which is why the check that this step is reachable is the embedded-shader
marker in the built `PlayerPanel.dll` (see "Tests and verification").

## Code map

| Path | Symbol | Responsibility |
| --- | --- | --- |
| `src/render/panel/panel_renderer.h` | `PanelRenderer` | The panel singleton: prepare/draw split, lazy init, target rebuild, release, drag state, built-in-chrome fallback |
| `src/render/panel/panel_renderer.h` | `PanelLayout`, `resolve_panel_layout` | The panel rectangle as a pure function of the render size and the configuration |
| `src/render/panel/panel_renderer.h` | `resolve_border_thickness` | The built-in fallback's hairline border thickness in render pixels, derived from the panel height |
| `src/render/panel/panel_renderer.h` | `resolve_skin_inset_thickness` | The skin mode's character inset in render pixels: its own configured fraction of the panel height, floored in pixels and capped to a quarter of the tighter axis |
| `src/render/panel/panel_menu.h` | `PanelMenu`, `PanelChrome`, `Panel_Menu_Name` | The one registered `IMenu`: the menu name, the creator, the movie load, the viewport, the engine cursor read and the chrome verdict |
| `src/render/panel/panel_menu.cpp` | `PanelMenu::install`, `set_open`, `chrome_mode`, `set_viewport`, `read_menu_cursor` | Registration with `RE::UI::Register`, the show/hide messages, the caller-chosen SWF path and the `MenuCursor` conversion |
| `src/render/panel/panel_renderer.cpp` | `PanelRenderer::prepare` | Game-thread menu show/hide, chrome mode, movie viewport, layout, drag, camera, and the single world-copy cull |
| `src/render/panel/panel_renderer.cpp` | `PanelRenderer::apply_panel_input`, `commit_panel_position` | Hit test against the engine menu cursor, grab offset, normalised position write |
| `src/render/panel/panel_renderer.cpp` | `PanelRenderer::set_panel_open` | Opening/closing the panel's claim on player input by showing or hiding its menu; idempotent |
| `src/render/panel/panel_renderer.cpp` | `PanelRenderer::draw` | Render-thread draw, per-frame back-buffer acquire/release, state capture, rectangle, chrome-dependent clear and inset, deferred release |
| `src/render/panel/panel_renderer.cpp` | `PanelRenderer::ensure_device_objects` | The renderer's device path: compiles the panel's shaders, latches a failed compilation for that device so it is not retried once per frame, then builds both passes |
| `src/render/panel/panel_renderer.cpp` | `Panel_Background_Color`, `Panel_Skin_Clear_Color`, `Panel_Border_Color`, `Panel_Border_Thickness_Fraction`, `Min_Panel_Skin_Inset_Pixels`, `Max_Chrome_Pending_Frames`, `Min_Panel_Pixels` | The built-in fallback chrome, the transparent skin-mode clear, the skin inset's pixel floor, the fallback grace and the size floor |
| `src/render/panel/panel_target.h` | `PanelTarget` | Private colour + depth textures and their RTV/DSV/SRV |
| `src/render/panel/panel_camera.h` | `PanelCamera`, `PanelCameraFrame` | Forward-Z perspective camera and the two-axis automatic body framing |
| `src/render/panel/panel_passes.h` | `PanelGeometryPass` | Offscreen pass: own depth state, input-layout cache, callback-style draw loop, caller-chosen clear colour |
| `src/render/panel/panel_passes.h` | `PanelCompositePass` | Built-in fill and hairline with blending disabled, or the character alpha-blended over the skin's movie and inset by the skin inset; owns both blend states |
| `src/render/panel/panel_geometry.h` | `PanelDraw` | One collected mesh: buffers, layout, skinning and resolved material |
| `src/render/panel/panel_geometry.h` | `PanelSkinLayout` | Calibrated weight/index layout of a skinned vertex buffer |
| `src/render/panel/panel_geometry.h` | `Max_Palette_Bones`, `Panel_Uv_Format` | Palette budget (128) and the assumed UV attribute format |
| `src/render/panel/panel_geometry.cpp` | `collect_panel_geometry`, `resolve_material` | Static/skinned collection with position and skin-layout self-calibration |
| `src/render/dx11/d3d11_util.h` | `D3D11StateCapture` | Saves and restores every D3D state a private pass overwrites |
| `src/render/dx11/d3d11_util.h` | `compile_shader` | Runtime `D3DCompile` of one entry point |
| `src/render/shader_manager.h` | `ShaderManager` | One-shot compilation of the embedded panel HLSL |
| `src/render/shaders/panel_geometry.hlsl` | `vs_static_main`, `vs_skinned_main`, `ps_panel_main` | Panel geometry shading |
| `src/render/shaders/panel_composite.hlsl` | `vs_main`, `ps_background_main`, `ps_panel_main` | Panel composite: the built-in opaque fill and hairline, or the character's colour with its own straight alpha for the skin path |
| `src/render/frame_hook.h` | `FrameHook::install_present`, `Max_Present_Listener_Count` | Render-thread present listener registration |
| `src/render/frame_hook.h` | `FrameHook::install`, `Max_Frame_Listener_Count` | Game-thread tick listener registration, used by the preview actor and the panel |
| `src/input/input.h` | `InputManager::install` | Input sink registration; the class owns no cursor behaviour at all |
| `src/input/input.cpp` | `InputHandler::ProcessEvent` | Forwards the left-button edge to the panel and applies the hotkey and master switch |
| `src/preview/preview_actor.h` | `PreviewActor::current_reference` | The borrowed preview reference the panel consumes |
| `src/config/config.h` | `Config`, `Panel_Swf_Path`, `Setting::set_panel_position` | The panel keys, the single documented skin path, the skin inset fraction and the single writer of the stored position |
| `src/main.cpp` | `install_frame_hook`, `release_panel_state` | Listener registration, menu registration and the per-session restore order |
| `cmake/embed_shaders.cmake` | — | Turns each `.hlsl` into a C++ raw string literal at configure time |
| `CMakeLists.txt` | — | The HLSL embedding step and the `d3dcompiler` link |
| `tools/make_panel_swf.py` | `build_swf`, `parse_swf` | The standard-library skin generator and its structural re-parse |
| `assets/PlayerPanel/panel.swf` | — | The generated default skin: an opaque stage-filling backdrop plus the frame band. The user installs it at the documented path |

Engine and SKSE dependencies used:

| Dependency | Path | Use |
| --- | --- | --- |
| `RE::BSGraphics::Renderer::GetSingleton` / `GetRuntimeData` | `extern/CommonLibSSE/include/RE/R/Renderer.h` | The engine's D3D11 device (`forwarder`) and immediate context |
| `RE::BSGraphics::Renderer::GetScreenSize` | `extern/CommonLibSSE/include/RE/R/Renderer.h` | Render resolution the panel rectangle is derived from |
| `RE::UI::Register` | `extern/CommonLibSSE/include/RE/U/UI.h` | Registers the one menu name and its creator, once |
| `RE::UI::GetMenu`, `RE::UI::GameIsPaused` | `extern/CommonLibSSE/include/RE/U/UI.h` | Reaches the created menu by its registered name, and gates the hotkey while the game is paused |
| `RE::UIMessageQueue::AddMessage` | `extern/CommonLibSSE/include/RE/U/UIMessageQueue.h` | Queues the engine's own `kShow`/`kHide` for the panel menu |
| `RE::IMenu`, `RE::UI_MENU_FLAGS::kUsesCursor` | `extern/CommonLibSSE/include/RE/I/IMenu.h` | The menu base class, the `uiMovie` the engine renders, and the cursor declaration that hands input to the engine |
| `RE::BSScaleformManager::LoadMovie` | `extern/CommonLibSSE/include/RE/B/BSScaleformManager.h` | Loads the movie from the caller-chosen path; the loader resolves its argument against `Interface/` and appends the extension itself |
| `RE::GFxMovieView::SetViewport` | `extern/CommonLibSSE/include/RE/G/GFxMovieView.h` | Engine-implemented: scales the movie's stage into the panel rectangle, so a skin needs no ActionScript |
| `RE::MenuCursor::GetRuntimeData` | `extern/CommonLibSSE/include/RE/M/MenuCursor.h` | `cursorPosX/Y` and `screenWidthX/Y`, converted into the render pixels the drag uses |
| `RE::ButtonEvent` | `extern/CommonLibSSE/include/RE/B/ButtonEvent.h` | The left-button edge behind the drag |
| `RE::TESObjectREFR::GetCurrent3D` | `extern/CommonLibSSE/include/RE/T/TESObjectREFR.h` | The preview's 3D root for bound, walk and cull |
| `RE::NiAVObject::SetAppCulled` / `worldBound` | `extern/CommonLibSSE/include/RE/N/NiAVObject.h` | World-copy visibility and framing |
| `RE::BSShaderProperty::GetBaseTexture` / `QMaterialAlpha` | `extern/CommonLibSSE/include/RE/B/BSShaderProperty.h` | Diffuse texture and material alpha |
| `RE::NiSourceTexture::rendererTexture` | `extern/CommonLibSSE/include/RE/N/NiSourceTexture.h` | Reaches the GPU texture object |
| `RE::BSGraphics::Texture::resourceView` | `extern/CommonLibSSE/include/RE/N/NiSourceTexture.h` | The diffuse shader-resource view |
| `RE::BSGraphics::VertexDesc` | `extern/CommonLibSSE/include/RE/V/VertexDesc.h` | Per-mesh attribute offsets and flags |
| `RE::BSVisit::TraverseScenegraphGeometries` | `extern/CommonLibSSE/include/RE/B/BSVisit.h` | Scene-graph traversal |
| `RE::NiSkinInstance` / `NiSkinPartition` | `extern/CommonLibSSE/include/RE/N/NiSkinInstance.h` | Skinned buffers and the bone palette |

## Interfaces

### `PanelRenderer`

- `static PanelRenderer& instance()` — the module singleton; there is no file-scope mutable state.
- `static void on_frame()` — game-thread frame listener. Shows or hides the panel's menu, resolves the
  chrome mode and the layout, sets the movie viewport, applies the drag from the engine menu cursor,
  collects the frame or clears it; never throws out (an unexpected throw drops the frame and logs).
- `static void on_present(REX::W32::IDXGISwapChain*)` — render-thread present listener. Submits no
  draw call at all when no frame is prepared.
- `void request_release()` — asks the render thread to release every D3D object it owns; the release
  is serviced on the next present so all device calls stay on one thread.
- `static void on_left_button(bool)` — the game-thread input recording point, called by the input sink.
  It records the button edge alone; the cursor position is never recorded, because it is read from the
  engine's own menu cursor each frame. It touches no engine state and no device.
- `void set_panel_open(bool)` — opens or closes the panel's claim on player input by showing or hiding
  its menu; idempotent, and it also clears the drag and the pending press.

### `PanelMenu`

- `static void install()` — registers `Panel_Menu_Name` and the creator with `RE::UI::Register`, once
  and idempotently, so every session message may call it.
- `static void set_open(bool)` — queues the engine's own `kShow` or `kHide` for the menu through
  `RE::UIMessageQueue::AddMessage`.
- `[[nodiscard]] static PanelChrome chrome_mode()` — `e_pending` until the engine has created the menu,
  then `e_swf` when its movie loaded and `e_built_in` when it did not.
- `static void set_viewport(uint32_t buffer_width, uint32_t buffer_height, uint32_t left, uint32_t top,
  uint32_t width, uint32_t height)` — `GFxMovieView::SetViewport`, scaling the movie's stage into the
  panel rectangle in render pixels.
- `[[nodiscard]] static bool read_menu_cursor(uint32_t render_width, uint32_t render_height, float&,
  float&)` — the engine menu cursor converted into render pixels; `false` while the engine has no
  cursor or extents, in which case the panel must not move.
- The constructor sets `menuFlags` to `kUsesCursor` and loads the movie from `Config::panel_swf_path`
  with `BSScaleformManager::LoadMovie`; it never sets `kPausesGame` or `kModal`.

### `resolve_panel_layout` / `resolve_border_thickness` / `resolve_skin_inset_thickness`

- `bool resolve_panel_layout(Config const&, uint32_t screen_width, uint32_t screen_height,
  PanelLayout&)` — pure; `false` means "degenerate render size or configuration, skip the frame".
  `PanelLayout` carries the rectangle in render pixels.
- `uint32_t resolve_border_thickness(uint32_t panel_height)` — pure; the built-in chrome's hairline
  thickness in render pixels, never zero. It is the built-in decoration only.
- `uint32_t resolve_skin_inset_thickness(Config const&, uint32_t panel_width, uint32_t panel_height)` —
  pure; the character's inset in skin mode, in render pixels. It is its own value, never the built-in
  hairline: `PanelSkinInsetFraction` of the panel height, floored at `Min_Panel_Skin_Inset_Pixels` and
  capped at a quarter of the panel's tighter axis so the character always keeps room.

### `PanelGeometryPass` / `PanelCompositePass`

- `bool init(REX::W32::ID3D11Device*)` / `void release()` — device-object lifetime; `init` picks the
  precompiled shaders out of `ShaderManager` and creates only the states and buffers of that pass. The
  composite's two blend states are parts of that pass's own resource set.
- `void PanelGeometryPass::draw(...)` — binds the private target, clears it with the caller's colour
  (opaque for the built-in chrome, fully transparent with a skin) and draws every mesh.
- `void PanelCompositePass::draw(...)` — with the built-in chrome, fills the panel rectangle with the
  opaque background and then draws the offscreen colour over it with its hairline border, blending
  disabled. With a skin, it draws only the character, keeps its straight alpha, clips the inset band
  away and blends with source alpha over inverse source alpha, so the movie the engine drew stays
  visible wherever the character has no coverage.

### `InputManager`

- `static void install()` — registers the engine input sink, once.
- The class owns no cursor behaviour: the engine drives the cursor while the panel's own menu is open,
  so the plugin contains no `ShowCursor`/`SetCursorPos`/`GetCursorPos`/`ClientToScreen`/`ScreenToClient`
  call and registers no per-frame cursor step at all.

### `FrameHook` (unchanged interface, one fewer registration)

- `bool install(Listener)` — game-thread tick listener. `Max_Frame_Listener_Count` is 4 and the plugin
  registers 2 (preview actor, panel); registration order is the invocation order.
- `bool install_present(PresentListener a_listener)` — registers a render-thread listener that runs
  inside the Present callback. The panel draws in it; a listener must return without drawing when it
  has nothing to draw.

### Configuration

INI file: `<game>/Data/SKSE/Plugins/PlayerPanel.ini`, section `General`. Existing keys
(`Enabled`, `Hotkey`, `PreviewDistance`) are unchanged; the panel keys are:

| Key | Type | Default | Accepted range | Meaning |
| --- | --- | --- | --- | --- |
| `PanelHeightFraction` | double | `0.66` | `0.05`..`1.0` | Panel height as a fraction of the render height |
| `PanelAspect` | double | `0.667` | `0.1`..`10.0` | Panel width divided by panel height |
| `PanelMarginFraction` | double | `0.022` | `0`..`0.5` | Right-edge gap as a fraction of the render height |
| `PanelPositionX` | double | `-1.0` (unset) | `-1`, or `0`..`1` | Panel left edge as a fraction of the free horizontal space; `-1` means "not placed yet" |
| `PanelPositionY` | double | `-1.0` (unset) | `-1`, or `0`..`1` | Panel top edge as a fraction of the free vertical space; `-1` means "not placed yet" |
| `PanelSwfPath` | string | `Interface\PlayerPanel\panel.swf` | non-empty, shorter than 128 characters | Path of the SWF the panel's chrome is loaded from; replacing the file at the default value is the supported way to reskin the panel, and another path only exercises the same plumbing with a different file |
| `PanelSkinInsetFraction` | double | `0.03` | `0.005`..`0.25` | The character's inset from the panel rectangle while a skin owns the chrome, as a fraction of the panel height. It is its own value and never the built-in hairline, floored at 6 render pixels; a skin must keep its frame at least this thick |
| `CameraFov` | double | `35.0` | `1`..`179` | Vertical field of view of the panel camera, in degrees |
| `CameraDistance` | double | `0.0` | `0`..`100000` | Camera distance in game units; `0` means automatic two-axis framing |
| `AlphaTestThreshold` | double | `0.5` | `0`..`1` | Alpha cutout threshold multiplied by the material alpha |

Every value outside its range (or non-finite) falls back to the documented default rather than being
clamped, so a typo stays visible. All this keys round-trip through `Setting::load` and
`Setting::save`, and the stored panel position is written by `Setting::set_panel_position`, which
clamps both axes to `0`..`1` so only a fraction can ever reach the file.

**Migration.** The three absolute pixel keys this module used before — `PanelWidth`, `PanelHeight`
and `PanelRightMargin` — are superseded and are no longer read for anything. `PanelCursorSensitivity`
was retired the same way when the panel switched to the engine's own menu cursor, because there is no
plugin-side cursor left for a tuning constant to scale. The presence of any of those four keys in an
existing INI is reported once in the log at load, so a stale configuration is visible instead of
being silently obeyed; the keys themselves are left in the file, because deleting a user's
configuration is not this module's decision. `PanelHeightFraction = 0.66`, `PanelAspect = 0.667` and
`PanelMarginFraction = 0.022` reproduce roughly the old `480` × `720` at a `24` pixel margin on a
1080p screen.

## Invariants

- **The panel never owns the preview.** `PanelRenderer` reads `PreviewActor::current_reference()`
  each frame and never creates, dresses or destroys the character. `PreviewActor` stays the only
  lifecycle owner.
- **No frame, no draw call.** When no preview is ready, `prepare` clears the frame and `draw` returns
  before touching the device. A hidden panel therefore issues no `Draw`, `DrawIndexed`, clear or
  state call at all.
- **The size is a function of the render height.** `resolve_panel_layout` derives the height from
  `PanelHeightFraction` and the render height, the width from that height and `PanelAspect`, and the
  margin from `PanelMarginFraction` and the render height. The screen width enters only as an upper
  clamp, which is what keeps the panel the same relative size on 16:9, 21:9 and 32:9 screens.
- **The stored position is normalised.** Both axes are fractions of the free space on that axis, and
  the single writer (`Setting::set_panel_position`) clamps them to `0`..`1`. No absolute pixel
  position is ever written to the INI.
- **Sizing and position survive a resolution change.** Because both are fractions, a resolution change
  only changes the pixels they resolve to; the panel keeps its relative size, its relative right-edge
  distance and its relative spot.
- **The cursor is the engine's own.** The menu declares `kUsesCursor`, so the engine drives its own
  menu cursor while the panel is open. The drag's hit test reads `RE::MenuCursor`'s `cursorPosX/Y`,
  converted into render pixels with its own `screenWidthX/Y`; the plugin integrates no cursor, calls no
  user32 entry point and derives nothing from mouse deltas.
- **The engine's input context holds the camera.** The plugin pushes no input context and freezes no
  control group: the engine's own UI input context, taken while a `kUsesCursor` menu is shown, is what
  keeps the camera from rotating. `ToggleControls` appears nowhere in the module.
- **The menu is the panel's whole input claim.** Showing it and hiding it are the only two things that
  change player input, and both are the engine's own messages. The menu never pauses the game
  (`kPausesGame` is unset) and is never modal (`kModal` is unset).
- **Every exit path restores.** Closing the panel, and `kPreLoadGame` / `kNewGame` / `kPostLoadGame`,
  hide the menu, so the engine gives the cursor and the input context back, and release the panel's D3D
  objects. A released or abandoned drag cannot leave the plugin believing a drag is still active:
  `set_panel_open(false)` clears the drag and the pending press, and a release is tracked even while
  the game is paused.
- **A missing movie degrades to the built-in chrome, never to nothing.** `PanelMenu::chrome_mode`
  reports `e_built_in` when the movie did not load, and `prepare` logs that once and paints the
  built-in fill and hairline; the fallback is reachable by deleting the file, with no code change.
- **Borrowed GPU resources only, kept alive.** Collected draws borrow vertex and index buffers from
  engine objects and hold them with `RE::NiPointer<RE::BSGeometry>` / `RE::NiPointer<RE::NiSkinInstance>`
  for the duration of the frame. No raw engine pointer is cached across frames.
- **Own camera, own depth.** The panel uses `XMMatrixLookAtLH` + `XMMatrixPerspectiveFovLH` with a
  near→0/far→1 forward-Z range and a `D3D11_COMPARISON_LESS` depth state. The engine's reverse-Z
  (GREATER) state is never reused.
- **Framing fits both axes.** The automatic distance is the larger of the vertical and the horizontal
  fit for the panel's own aspect, so a portrait panel does not clip arms or weapons at the sides.
  `CameraDistance` above zero still overrides the fit entirely.
- **No `NORMAL` vertex input.** Neither panel shader declares a `NORMAL` semantic; the lighting
  normal is `cross(ddx(world), ddy(world))`, flipped towards the eye, so shading cannot depend on the
  engine's packed vertex normals.
- **Alpha cutout is one multiplication.** The material alpha (or `1.0` without a shader property)
  multiplies the sampled texture alpha and is compared against the configured threshold.
- **One visibility change.** `PanelRenderer::prepare` is the only place in the change set that calls
  `SetAppCulled`; it culls the preview's 3D root, the engine's own per-object visibility switch.
- **One border, one slot, two blend states.** The composite paints at most one hairline border, and its
  parameters, including the chrome flag that selects skin or built-in mode, ride in the composite
  pass's existing pixel-stage slot 0, so the constant-buffer set the engine state capture restores does
  not grow. The pass owns both blend states it may bind, and both are created with its other resources
  and released with them.
- **The skin supplies the panel's backdrop and frame.** In skin mode the composite never fills the
  rectangle and never paints the band: the panel's opacity and its frame are the movie's, drawn by the
  engine in its UI pass. The generated default skin draws an opaque stage-filling backdrop and a frame
  band on top of it, and the generator re-parses its own output to prove it declares an opaque fill.
- **Skin mode writes only where the character is.** With a skin the private target is cleared with zero
  alpha, the geometry pass writes the character's own straight alpha, and the composite binds source
  alpha over inverse source alpha with the destination alpha preserved. A fragment with no character
  coverage therefore contributes nothing and repeated frames cannot accumulate coverage.
- **The built-in path is unchanged and opaque.** The built-in chrome clears the target with the opaque
  background colour, blends with blending disabled and paints its hairline band; it is the same
  appearance the panel had before a skin existed, and it is the whole fallback.
- **The two insets are separate values.** The built-in hairline comes from
  `Panel_Border_Thickness_Fraction`; the skin inset comes from `PanelSkinInsetFraction`. In each mode
  the character is composited inset by exactly the value that mode resolved —
  `resolve_border_thickness(layout.height)` or
  `resolve_skin_inset_thickness(config, layout.width, layout.height)` render pixels — and in skin mode
  the shader discards that same band instead of painting it. A skin's frame, authored at least that
  thick, is therefore never overdrawn.
- **The shaders are compiled before any pass is built, and that compilation is a lifecycle step.**
  `ShaderManager::compile()` is attempted at data load and, for certain, inside
  `PanelRenderer::ensure_device_objects`, which returns before either pass' `init()` until it has
  succeeded. A failure is logged once, remembered for that device, and skips the draw instead of being
  retried every frame; a different device, or an explicit panel release, is a fresh attempt. Success
  emits exactly the log line `All panel shaders compiled`. The build
  cannot detect a missing call — the embedded HLSL would simply be unreferenced and dropped by the
  linker — so the embedded-shader marker in the built DLL is what proves the step is reachable.
- **State is restored.** Every draw path runs inside exactly one `D3D11StateCapture`, and the capture
  set covers render targets, depth-stencil view, viewport, scissor rectangle, pixel-shader resource
  views and pixel-shader constant buffers (plus blend, depth, rasterizer, input layout, topology,
  vertex/index buffers, shaders and vertex constant buffers).
- **No swap-chain reference is held between frames.** `PanelRenderer::draw` acquires the back buffer and
  creates its render-target view for that single call, and releases both before returning; neither is
  a member. Holding either would keep a direct or indirect reference to a swap-chain buffer and make
  the engine's own `ResizeBuffers` fail with `DXGI_ERROR_INVALID_CALL` on a resolution change or a
  windowed/fullscreen toggle, which surfaces to the player as a black or frozen frame.
- **Device work stays on one thread.** Device objects are created, used and released only on the
  render thread; every call that touches the menu, Scaleform or the engine cursor happens only on the
  game thread.
- **No growing per-frame allocation.** Collection reuses one vector and the handoff ping-pongs fixed
  buffers; the draw loop reuses a stack constant buffer and a capped input-layout cache. The one
  per-frame D3D object is the back-buffer render-target view, which exists only for the frame that
  uses it and is destroyed before the next one.

## Failure modes

| Situation | Behaviour | Verification entry point |
| --- | --- | --- |
| No preview ready (idle, unavailable, main menu) | The frame is cleared, `draw` returns before any D3D call, and the panel's menu is hidden so the engine gives the cursor and the input context back | `PanelRenderer::prepare` early return, `PanelRenderer::set_panel_open(false)`, `PanelRenderer::draw` guarded swap |
| Preview reference no longer resolves | `current_reference` returns `nullptr`, so the panel behaves exactly as "no preview" | `PreviewActor::current_reference` handle check |
| Degenerate render size (zero width or height) | `resolve_panel_layout` returns `false`, the frame is skipped and no zero-sized target is built | `resolve_panel_layout` first guard; `PanelRenderer::draw` layout guard |
| Non-finite sizing configuration (only reachable if a future caller builds a `Config` by hand, because the INI boundary already rejects it) | `scale_to_pixels` treats the non-finite fraction as zero, so that dimension falls back to the absolute minimum instead of an undefined value being converted to pixels | `scale_to_pixels` guard |
| Computed size below the minimum or beyond the screen | The size is clamped up to `Min_Panel_Pixels` and down to the screen, so the rectangle always fits | `resolve_panel_layout` clamps |
| Stored position outside `0`..`1` | Clamped at the single writer, so the panel stays on screen and only a fraction reaches the INI | `Setting::set_panel_position`; `resolve_panel_layout` clamp |
| A press outside the panel | No drag starts; the panel ignores the press entirely | `PanelRenderer::apply_panel_input` hit test |
| A press inside the panel | A drag starts and keeps the grab offset, so the panel does not jump under the cursor | `PanelRenderer::apply_panel_input` grab-offset capture |
| Button released, or the panel closed mid-drag | The drag ends and no drag state survives; the physical button state keeps being tracked while the panel is hidden | `PanelRenderer::apply_panel_input` release branch; `PanelRenderer::set_panel_open` reset |
| Button released while the game is paused or the master switch is off | Still observed, because the input sink forwards the mouse before those gates; a dropped release would latch a drag | `InputHandler::ProcessEvent` ordering |
| The skin movie is missing or fails to load | The menu still exists but reports `e_built_in`; the plugin logs that once and paints the built-in fill and hairline, so the panel stays fully usable by deleting the SWF | `PanelMenu::PanelMenu` load verdict, `PanelRenderer::prepare` chrome branch |
| A skin's backdrop is not opaque | The world shows through the panel's backdrop; the character itself stays correct. This is the skin's responsibility, not a plugin defect: the plugin paints no fill of its own while a skin owns the chrome | `ps_panel_main` skin branch in `src/render/shaders/panel_composite.hlsl` (no fill), `PanelRenderer::draw` clear choice |
| `PanelSkinInsetFraction` is out of range or non-finite | The key falls back to the documented default rather than being clamped, and the resolver then floors the value in pixels, so the frame never collapses to a hairline | `is_usable_panel_skin_inset_fraction` in `src/config/config.cpp`; `resolve_skin_inset_thickness` |
| The engine has not created the menu yet | The panel draws nothing for up to `Max_Chrome_Pending_Frames` frames, so the built-in chrome never flashes over a skin that is about to appear | `PanelRenderer::prepare` pending branch, `Max_Chrome_Pending_Frames` |
| The menu is never created (registration refused, UI singleton absent) | The grace expires, the built-in chrome is used, and the fallback is logged once rather than once per frame | `PanelRenderer::prepare` fallback branch, `m_chrome_fallback_logged` |
| The engine menu cursor or its screen extents are unavailable | `read_menu_cursor` returns `false`, so the hit test is skipped and the panel stays where it is instead of jumping; the panel still draws | `PanelMenu::read_menu_cursor` guards |
| The configured skin path is empty or unusable | The menu loads no movie, reports `e_built_in`, and the built-in chrome is used | `to_movie_name` guard in `src/render/panel/panel_menu.cpp` |
| Preview's world bound non-finite or out of range | Framing falls back to a fixed body-height target around the reference position | `resolve_framing` range check in `src/render/panel/panel_camera.cpp` |
| Preview heading or field of view not finite, or the panel aspect unusable | `PanelCamera::build` returns `false` and the frame is dropped instead of drawing a degenerate matrix | `PanelCamera::build` guards |
| A mesh has no UV attribute or no diffuse texture | The input layout binds no UV input and no SRV is bound; the pixel shader shades the flat albedo | `PanelGeometryPass::acquire_layout` and the `has_uv` branch of the draw loop |
| Position format or skin layout cannot be calibrated | That mesh is skipped and logged rather than drawn with a guessed layout | `collect_panel_geometry` self-calibration paths |
| `CreateInputLayout` fails for one mesh | That mesh is skipped, the rest of the frame still draws | `PanelGeometryPass::acquire_layout` failure branch |
| Target creation fails | `PanelTarget::init` releases every partial object and the frame is skipped | `PanelTarget::init` release-on-failure |
| Swap chain back buffer cannot be acquired (device loss, mid-resize) | The failure is logged once per streak, not once per frame; the panel is skipped for that frame and the engine is left untouched | `PanelRenderer::draw` acquisition guard and `m_back_buffer_error_logged` |
| Swap chain resized (resolution change, windowed/fullscreen toggle) | The panel holds no back-buffer reference, so the engine's `ResizeBuffers` can succeed; the panel rebuilds its target, re-derives its rectangle from the new render height and keeps its relative size and spot | `PanelRenderer::draw` acquire/release pair, `PanelTarget::matches`, `resolve_panel_layout` |
| Device changes | All panel device objects and the target are released and rebuilt lazily | `PanelRenderer::ensure_device_objects` |
| Session change (`kPreLoadGame`, `kNewGame`, `kPostLoadGame`) | The panel menu is hidden, the panel's D3D objects are released and the menu registration is re-asserted, so nothing the panel changed crosses a load | `release_panel_state` call sites in `src/main.cpp` |
| Shader compilation fails | `ShaderManager::compile` logs and reports failure; the panel draws nothing instead of drawing garbage | `ShaderManager::compile` failure branch |
| A listener throws inside the frame tick | The frame is dropped and logged; `FrameHook` keeps the other listeners running | `FrameHook::tick` / `FrameHook::on_present` guards and `PanelRenderer::on_frame` catch |
| A mouse event throws inside the input sink | The event is dropped and logged; the sink keeps returning `kContinue` | `InputHandler::ProcessEvent` guard |

Known limitations of this batch:

- **Whether Skyrim's Scaleform player accepts the generated Shape-only SWF is unverified.** The
  generator proves the file's structure and that it carries no action tag, but no Scaleform player
  exists on this host to load it. If the file were rejected, the panel would still draw through the
  built-in fallback — which is exactly why that fallback is required — and the mechanism would still
  be testable by pointing `PanelSwfPath` at another SWF. This is a runtime checklist item.
- **Whether the engine preserves the plugin's movie viewport is unverified.** `GFxMovieView::SetViewport`
  is engine-implemented, and the pinned headers do not say whether the menu manager re-applies its own
  viewport while the panel's menu is drawn. If the engine overwrote it, the movie would be positioned
  by the engine rather than by the panel rectangle, and positioning would have to move to the built-in
  chrome; that is a contract revision, not a silent edit.
- **The engine menu cursor's unit is inferred from its own screen extents.** The conversion assumes
  `cursorPosX/Y` is in the same unit as `screenWidthX/Y`, which the runtime data's layout implies but
  which only the target machine can confirm. A wrong scale would show as a cursor whose travel does not
  match the panel's travel, not as a broken panel.
- **The skin's visible area is the backdrop plus the frame band.** The composite writes only where the
  character has coverage, so a skin is visible around the character's silhouette and through any
  partly transparent part of it; the inset band itself is discarded and shows the skin's frame. A skin
  that wants art *on top of* the character cannot have it — that would need the engine to draw the
  character into its own UI layer, which is a contract revision, not a configuration change.
- **A skin with a transparent backdrop shows the world through the panel.** The plugin deliberately
  paints no fill while a skin owns the chrome, because any fill it painted would cover the movie the
  engine had already drawn. The panel's opacity is therefore the skin's own responsibility, and the
  shipped default skin draws an opaque backdrop for exactly that reason.
- **The skin inset is a fraction of the panel height applied to both axes.** That keeps it visually
  constant across resolutions, and it matches the default skin's stage aspect (320×480, the same as the
  default `PanelAspect`), so the frame band scales uniformly at the default settings. At a very
  different aspect the movie is stretched into the panel rectangle non-uniformly, so a skin's own band
  would be thicker on one pair of sides than the other; that is the skin's own authoring concern and
  the plugin's inset stays square.
- **The UV attribute format is inferred.** `VertexDesc::GetSize` proves the attribute is four bytes
  but not whether it is two half-floats or two normalized shorts. `Panel_Uv_Format` defaults to
  `R16G16_FLOAT`; if a character's textures sample wrongly in game, this is the first constant to
  revisit, and the manual checklist has a step for it.
- **Hiding the world copy may suppress its skinning update.** Whether `SetAppCulled(true)` stops the
  engine from updating the bone world matrices is unverified. If the panel character freezes, the
  documented fallback is to move the reference out of view instead of culling it — still in exactly
  one place — and that change is a contract revision, not a silent edit.
- **Texture alpha and cutout thresholds are empirical.** Character textures rarely agree on what
  "transparent" means; `AlphaTestThreshold` is the single knob the user can turn, and its default
  `0.5` is a starting point rather than a measured value.
- **Lighting is fixed by construction.** One directional light plus ambient reads clearly but does
  not match the cell's lighting; that is the contract's intent (PRD FR-05).
- **The built-in fallback's look is deliberately minimal.** One opaque fill and one hairline border.
  It is what the panel draws when no skin is available; a skin's own look is its own business, and the
  shipped default skin is a placeholder whose value is proving the mechanism — an opaque backdrop, a
  frame band at the configured inset, and no script — rather than its artistry.
- **Empty-frame behaviour.** A frame whose meshes are all skipped (calibration failures, for
  example) still draws the chrome rather than nothing: with the built-in chrome the window content is
  the opaque background colour, and with a skin the composite writes nothing and the skin's backdrop is
  what remains. A blank panel is more honest than a flicker.

## Dependencies

- `CommonLibSSE-NG` v8.0.1 (`extern/CommonLibSSE` submodule) — all engine interfaces used here are
  declared by the pinned headers.
- `DirectXMath` — the camera matrices (already reachable through `CommonLibSSE-NG`, which includes
  `<DirectXMath.h>`).
- `spdlog` (via the project logging alias `logger`) — state and failure logging.
- `SimpleIni` — INI storage for the panel keys.
- `d3dcompiler` — added to the existing target's link libraries because the embedded HLSL is
  compiled at runtime. This is not a new package: no `vcpkg.json` entry is added or changed.
- `user32` — still linked by the target (it backs the engine's own input handling and the mirrored
  `REX::W32` bindings), but the panel makes no user32 call of its own any more: the engine drives the
  cursor.
- `Scaleform` — reached only through `CommonLibSSE-NG`'s `RE::` wrappers (`BSScaleformManager`,
  `GFxMovieView`, `IMenu`, `MenuCursor`, `UIMessageQueue`); nothing is linked directly and no new
  package is added. The skin generator is a plain Python standard-library script and is not part of the
  build.
- No other dependency, and no CMake change beyond the HLSL embedding step and the `d3dcompiler` link.

## Tests and verification

This project has no automated test target (`BUILD_TESTS` is `OFF`) and the host that builds it has
no Skyrim installation, so the compile-time checks below are the only automated evidence.

Configure and build (the exact commands run for this module; `cmake --preset Release` cannot be used
on the build host because the manifest's `builtin-baseline` commit is absent from the local vcpkg
clone, so the manifest install is disabled and the sibling project's installed tree is reused):

```powershell
python build/lcw_run.py "C:/env/cmake/bin/cmake" -S . -B build -G "Visual Studio 17 2022" -A x64 -T "v143,version=14.44.35207" -DCMAKE_TOOLCHAIN_FILE="C:/env/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static-md -DVCPKG_INSTALLED_DIR="D:/code/cpp/skyrim/Highlight-Lootable-Corpses/build/vcpkg_installed" -DVCPKG_MANIFEST_INSTALL=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_FLAGS=/EHsc /MP /W4 /WX"
python build/lcw_run.py "C:/env/cmake/bin/cmake" --build build --config Release --parallel 2
python tools/make_panel_swf.py --verify --selftest
```

The third command needs no compiler and no build: the standard-library generator rewrites the default
skin artifact, re-parses the bytes it wrote and also proves that its parser rejects deliberately
corrupt buffers.

`build/lcw_run.py` is a machine-local, git-ignored helper that removes the duplicate case spelling of
the proxy variables the host session exports; without it MSBuild's `CL.exe` task aborts with `MSB6001`
before any compiler runs. The build must go through it.

Expected result: exit code 0, `build/Release/PlayerPanel.dll`, and the generated headers
`build/src/render/shader_sources.h` and `build/src/plugin.h`. `src` is collected with
`file(GLOB_RECURSE)` in `CMakeLists.txt` and has no `CONFIGURE_DEPENDS`, so **re-run the configure
step after adding or removing a source file**, before building. `PlayerPanel` compiles with
`/W4 /WX`, so any warning in first-party code fails the build; the only attributable warning is the
pre-existing `D9025` command-line warning (`/Ob2` overridden by `/Ob3`, from a compile option this
batch may not touch). If MSBuild aborts with `C1076`/`C3859` (out of memory), rebuild with
`--parallel 1`.

Static checks on the source of this module:

- `resolve_panel_layout` is the only place the panel size and position are computed, and it reads the
  screen width for the clamp alone: the height comes from `panel_height_fraction`, the width from that
  height and `panel_aspect`, and the margin from `panel_margin_fraction`.
- The only writer of a stored position is `Setting::set_panel_position`, which clamps to `0`..`1`, and
  both keys are read by `Setting::load` and written by `Setting::save` along with every other key.
- `PanelWidth`, `PanelHeight`, `PanelRightMargin` and `PanelCursorSensitivity` no longer appear as
  configuration reads, and their presence in an INI is reported once at load.
- `MenuCursor` is read in exactly one place (`PanelMenu::read_menu_cursor`), converted with its own
  screen extents, and compared against the panel rectangle; no mouse delta is integrated anywhere.
- `PushInputContext`, `PopInputContext`, `ToggleControls`, `GetControlsState` and `SetControlsState`
  appear nowhere in `src/`; the only input change is the menu's `kUsesCursor` flag plus the engine's
  own show and hide messages.
- `UI::Register` appears once, under one stable menu name, and `kPausesGame` appears nowhere.
- A search for `ShowCursor`, `SetCursorPos`, `GetCursorPos`, `ClientToScreen` and `ScreenToClient`
  across `src/` returns nothing: the plugin marshals no cursor.
- The SWF path is the single constant `Panel_Swf_Path`, and it is the value the menu turns into the
  movie loader's argument; `PanelSwfPath` only overrides it.
- The skin inset is a separate value from the built-in hairline: `resolve_skin_inset_thickness` reads
  only `Config::panel_skin_inset_fraction` and never `Panel_Border_Thickness_Fraction`, and the key is
  validated by `is_usable_panel_skin_inset_fraction`, read by `Setting::load` and written by
  `Setting::save` like every other panel key.
- The chrome mode selects both the clear and the blend state: skin mode clears with `Panel_Skin_Clear_Color`
  (zero alpha) and binds the composite pass's source-alpha blend state, while built-in mode clears with
  `Panel_Background_Color` and binds its blending-disabled state. No new constant-buffer slot exists;
  the inset still rides in the composite pass's existing pixel-stage slot 0.
- The composite shader keeps the sampled character alpha in its skin branch and pins it to one in its
  built-in branch, and it is the only place the inset band is discarded.
- No shader in `src/render/shaders/panel_geometry.hlsl` declares a `NORMAL` input, and the pixel
  shader derives its normal from `ddx`/`ddy` of the world position.
- Every panel draw path is wrapped by exactly one `D3D11StateCapture`, and the capture set in
  `src/render/dx11/d3d11_util.h` is not smaller than the sibling repository's.
- `SetAppCulled` appears in exactly one first-party call site (`PanelRenderer::prepare`).
- `Config` carries no field initializers and every member is initialized in a `.cpp` constructor.
- `python tools/make_panel_swf.py --verify --selftest` rewrites the artifact, re-parses it and proves
  the parser rejects deliberately corrupt buffers, all with a zero exit code.
- A byte search over `build/Release/PlayerPanel.dll` for the embedded-shader marker `ps_panel_main` —
  the HLSL entry-point name that occurs inside the embedded source text — finds at least one
  hit. This is the check that shader compilation is *reachable*, not merely compiled: before the
  wiring existed the same search found zero hits, because an unreferenced embedded source is dropped
  by the linker and no build warning or error reports it. A green build alone never proved the panel
  could draw. (`R"shdr(` is *not* a byte marker: the raw-string delimiter is a source-level token and
  never survives into the object code, so a byte search for it finds nothing both before and after.)

Manual in-game checklist (not executed on this host; reported as `UNVERIFIED`):

1. Install `build/Release/PlayerPanel.dll` under `Data/SKSE/Plugins/` and copy the generated
   `assets/PlayerPanel/panel.swf` to `Data/Interface/PlayerPanel/panel.swf`, let the game reach the
   main menu, then load a save in an exterior cell.
2. Press the configured hotkey (F7 by default). Expect a right-anchored, vertically centred window
   showing the player's own character with a recognisable face, hair and worn equipment, lit clearly
   and evenly, framed by chrome that came from the SWF.
3. **Default skin.** Confirm the frame around the character is the shipped skin's own band and not the
   built-in look: the outer band is the generated skin's warm band (`0xC8A86B`), it reads as a frame a
   few percent of the panel height thick rather than a hairline, the plugin log contains the
   skin-loaded line naming `PlayerPanel/panel`, and the built-in neutral grey (`0x616168`) fill appears
   nowhere.
4. **Skin backdrop behind the character.** Look at the panel around the character: the area inside the
   frame but outside the character's silhouette must be the skin's own backdrop — the generated default
   skin's near-black `0x0D0D10` — rather than the world behind the player. Terrain, water or sky showing
   through any part of the panel the character does not cover means the skin's backdrop is transparent,
   because the plugin paints no fill of its own while a skin owns the chrome.
5. **Replaced skin.** Quit the game, replace `Data/Interface/PlayerPanel/panel.swf` with a visibly
   different SWF (for example one whose backdrop and frame use strongly different colours), restart and
   press the hotkey again. Expect both the backdrop and the frame to change with no rebuild, no INI edit
   and no other file touched.
6. **Missing-file fallback.** Delete `Data/Interface/PlayerPanel/panel.swf`, restart and press the
   hotkey. Expect the panel to appear with its built-in dark fill and neutral grey hairline, and the log
   to contain exactly one line reporting that no usable skin movie was found.
7. **Alternate path.** Put a second SWF somewhere else under `Interface/`, set
   `PanelSwfPath=Interface\<folder>\<file>.swf`, restart and confirm the panel loads that file instead.
8. **Skin inset.** Set `PanelSkinInsetFraction=0.12`, restart and confirm the character becomes visibly
   smaller inside a much thicker frame, with the skin's band filling the wider outer ring and the
   character never drawing over it. Set it to `0.004` (below the accepted range) and confirm the log
   shows the default is used and the panel still reads as a frame rather than a hairline.
9. **Engine cursor and drag.** With the panel visible, confirm the game's own cursor is drawn, press the
   left mouse button over the panel and move the mouse: the panel must follow without jumping, the
   cursor must stay where the panel was grabbed, and the camera must not rotate. Release: the drag must
   end and look must work again. Pressing outside the panel must not move it.
10. **Screen bounds.** Drag the panel hard against each screen edge and into each corner. Expect it to
    stop at the edge instead of leaving the screen, at any resolution.
11. **Close-time cleanup.** Note where the cursor sits, open the panel, drag it, close it with the
    hotkey, then move and look around. Expect the cursor to be gone (or back to whatever it was) and
    movement and mouse look to be normal immediately. A cursor that stays visible with look still held
    means a restore path was missed.
12. **No pause.** With the panel open, confirm the world keeps running — weather, water and NPCs — and
    that the panel does not pause the game.
13. **Aspect ratios.** Repeat step 2 at 16:9, 21:9 and 32:9 (or at whatever set of aspect ratios the
    display offers). Expect the window to keep the same relative size and the same relative distance
    from the right edge at every one of them, with only the amount of visible world changing. A window
    that grows, shrinks or drifts towards or away from the edge when the aspect ratio changes means
    the size is still coming from the screen width.
14. **World copy hidden.** Confirm the character does **not** also stand in the world in front of the
    player. Walk around and confirm no second character follows the player.
15. **UV format.** Look closely at the face and at any worn armour. Textures must map onto the
    surface, not smear diagonally, not collapse to one colour and not jump between vertices. A
    diagonal smear points at `Panel_Uv_Format` in `src/render/panel/panel_geometry.h`.
16. **Framing.** Confirm the whole body and, if one is equipped, the weapon are inside the frame and
    are not clipped at the sides. Set `PanelAspect=0.5`, restart and repeat: a portrait panel must
    still frame the whole character rather than cutting the arms off at the edges.
17. **Alpha cutout.** Confirm hair reads as hair and not as solid blocks, and that eye lashes and
    cloth fringes are not opaque slabs. Raise and lower `AlphaTestThreshold` between `0.2` and `0.8`,
    restart, and confirm the hair silhouette changes accordingly. A body part that vanishes entirely
    means the threshold is too high for that texture.
18. **Position persistence.** Drag the panel to a distinctive spot, close and reopen it, and confirm it
    returns to the dragged spot. Then save the game, quit, restart and reopen the panel: expect the
    same spot again. Change the render resolution and confirm the panel keeps its *relative* spot
    rather than its pixel position.
19. **The rest of the keyboard is unaffected.** With the panel open, confirm movement keys still move
    the player and the map, journal and inventory still open and respond. Only looking around with the
    mouse is held by the panel's menu, and it must work again the moment the panel closes.
20. Press the hotkey again. Expect the window to vanish completely with no leftover rectangle and no
    change to the rest of the screen.
21. **Repeated open and close.** Repeat the open, drag and close from steps 2, 9 and 11 ten times.
    Expect no accumulation of windows, no frame-time creep and no duplicated menu registration in the
    log; the plugin log must contain no D3D error and no repeated failure line.
22. **Engine-state restoration.** With the panel visible, look at the world around the window: the
    sky, terrain, water and shadows must render normally, not black, not shifted and not one frame
    stale. Open the map, open and close the inventory, and look through a bow scope; none of those
    screens may inherit a panel render target, viewport or constant buffer. A world that renders into
    a corner or loses its lighting means a state was not restored.
23. **A menu open while the panel is visible.** Open the inventory or the map with the panel still
    shown. Expect the menu and its cursor to work normally and the panel to still be there when the
    menu closes.
24. Open and close the inventory while the panel is shown. Expect the panel and the game to keep
    working.
25. **Save and load with the panel open.** Save the game with the panel open, return to the main menu
    and load that save. Expect no window, no preview and a normal cursor after loading, and the game to
    keep working.
26. **Tuning.** Change `PanelHeightFraction`, `PanelAspect` and `PanelMarginFraction` one at a time
    (for example `0.8`, `1.0`, `0.06`), restart, and confirm the window resizes, changes shape and
    moves away from the edge. Set `CameraDistance=250.0` and confirm the framing becomes fixed instead
    of automatic.
27. Change the render resolution with the panel open, including to a different aspect ratio, and
    confirm the panel keeps its relative size and position and that both the panel and the world keep
    rendering.
28. **Swap-chain survival.** Change the render resolution (or toggle windowed/fullscreen) while the
    panel is open. Expect the engine's own resize to succeed — no black frame, no freeze — and both
    the panel and the world to keep rendering correctly afterwards. A black or frozen frame here is
    the signature of a swap-chain back-buffer reference held by the panel, which must never be the
    cause.
29. **Migration report.** Put `PanelWidth=480`, `PanelHeight=720`, `PanelRightMargin=24` and
    `PanelCursorSensitivity=2.0` in an existing `PlayerPanel.ini`, restart, and confirm the log reports
    once that those keys are ignored and that the panel is sized from the fraction keys instead.
30. **Shader compilation.** With the DLL installed and the game loaded into a save, open the plugin log
    and confirm it contains the line `All panel shaders compiled` exactly once (a `Shader precompile
    skipped` line before it is expected whenever the D3D11 device did not exist yet at data load, and
    must not be followed by a repeat of that skip). No such success line, or one of the failure lines
    around it, means the panel's shaders never compiled and the panel cannot draw.

## Safe modification guidance

- Add per-frame behaviour as a `FrameHook` listener, not as a new hook. `PresentHook` owns the only
  hook write. Game-thread work registers with `install`, render-thread D3D work with
  `install_present`. Game-thread listeners run in registration order and the render-thread listeners
  inside the Present callback, so a listener that needs another listener's output must be registered
  after it.
- Keep the split: never touch the device or the immediate context from the game thread, never walk the
  scene graph from the render thread, and never touch the menu, Scaleform or the engine cursor from the
  render thread. `PanelRenderer::prepare` and `PanelRenderer::draw` are the two halves and the mutex
  must not be held across either.
- Keep `resolve_panel_layout` pure and the only place the rectangle is computed. Both the drag and the
  composite depend on the two agreeing; a second copy of that arithmetic will drift.
- Add new constant buffers only in slots the capture set restores; if a pass genuinely needs a wider
  slot, widen `D3D11StateCapture` in the same change so the capture set never shrinks.
- Keep `SetAppCulled` in one place. If a new module also needs to hide the world copy, it must go
  through this one call site, not add a second policy.
- Extend the panel by adding a pass or a shader pair, not by growing `PanelRenderer::draw` into a
  general renderer. Each pass owns its states and creates them in its own `init`.
- When adding a shader, add it to `RENDER_SHADER_EMBED` in `CMakeLists.txt` **and** to
  `ShaderManager::compile`; the embedding step only makes the source available.
- Never cache a raw engine pointer or a GPU buffer across frames; hold borrowed GPU buffers with
  `RE::NiPointer` exactly as `PanelDraw` does.
- Never keep the swap-chain back buffer or a view created from it across frames. Acquire them inside
  the draw call that uses them and release them before it returns; a retained reference makes the
  engine's own `ResizeBuffers` fail with `DXGI_ERROR_INVALID_CALL` on a resolution change or a
  windowed/fullscreen toggle.
- Make no cursor call from the plugin at all. The engine drives the cursor because the panel's menu
  declares `kUsesCursor`; adding a `ShowCursor`, `SetCursorPos`, `GetCursorPos`, `ClientToScreen` or
  `ScreenToClient` call would put the plugin in competition with the engine's own menu cursor, which is
  exactly what this design removed.
- Never push or pop an input context by hand and never call `ToggleControls`. The menu's flags are the
  whole of the panel's input claim and showing or hiding the menu is the whole of its lifetime.
- Keep the input sink free of engine mutations. It records; the frame tick acts. And keep the mouse
  observation ahead of the pause and master-switch gates, because a release that is dropped latches a
  drag.
- New configuration keys need a default, an accepted range, a fallback in `Setting::load`, a
  `Setting::save` line and a row in the table above. A position-like key must stay a fraction and be
  clamped by `Setting::set_panel_position`, never stored as pixels.
- The built-in fill and hairline are the fallback's whole decoration. The panel's chrome is the SWF
  path, not those constants, and the two insets are deliberately kept apart:
  `Panel_Border_Thickness_Fraction` is the built-in hairline only, and the `PanelSkinInsetFraction` key
  is the skin inset and the number a skin's frame is authored against. Do not make one stand in for the
  other, and do not let the skin path paint a fill or a band: any fill the composite painted while a
  skin owns the chrome would cover the movie the engine drew underneath.
- Keep skin mode non-covering. The composite keeps the character's straight alpha and blends source
  alpha over inverse source alpha with the destination alpha preserved, so a fragment with no coverage
  writes nothing and repeated frames cannot accumulate. A blend state that writes colour for a zero
  alpha, or that accumulates the destination alpha, is a defect. Create and bind such a state inside
  the composite pass and inside the existing state-capture pair, and never widen the constant-buffer
  slots the capture restores.
- Keep the skin path a single constant and keep the loader argument derived from it in one place
  (`to_movie_name` in `src/render/panel/panel_menu.cpp`). A second place that builds a movie path would
  let the documented override and the loaded file drift apart.
- Extend `tools/make_panel_swf.py` rather than hand-editing `assets/PlayerPanel/panel.swf`: the artifact
  is regenerated and re-parsed by the script, and a hand-edited binary would carry no structural proof.
  Keep the emitted tag set inside `ALLOWED_TAGS`, so the no-action-tag property stays checkable rather
  than merely intended, and keep at least one fully opaque fill style — the parser rejects a skin with
  no opaque fill, because a translucent skin would show the world through the panel. The frame band must
  stay at least as thick as `Default_Panel_Skin_Inset_Fraction`, which the band's own fraction mirrors.

## Synchronized files

Changes to this module must update these files together:

- `src/render/panel/` — the renderer, the registered menu, the target, camera, passes, geometry
  collector and the drag state.
- `src/render/dx11/` — device access, state capture and shader compilation.
- `src/render/shader_manager.h` and `src/render/shader_manager.cpp` — the compiled panel shader set.
- `src/render/shaders/panel_geometry.hlsl` and `src/render/shaders/panel_composite.hlsl` — the panel
  HLSL, embedded through `cmake/embed_shaders.cmake`.
- `CMakeLists.txt` — the HLSL embedding step and the `d3dcompiler` link.
- `src/render/frame_hook.h` and `src/render/frame_hook.cpp` — the listener registrations.
- `src/preview/preview_actor.h` and `src/preview/preview_actor.cpp` — `current_reference()`.
- `src/input/input.h` and `src/input/input.cpp` — the input sink and the left-button observation.
- `src/config/config.h` and `src/config/config.cpp` — the panel keys, the skin path, the skin inset
  fraction and the stored position.
- `src/main.cpp` — listener registration, menu registration and the per-session restore.
- `tools/make_panel_swf.py` and `assets/PlayerPanel/panel.swf` — the skin generator and its generated
  artifact; regenerate the artifact with the script after any change to the emitted movie.
- `docs/features/preview-actor.md` — its statements about the preview being visible in the world.
- `docs/features/README.md` — the feature index entry for this document.
- `CHANGELOG.md` — the user-visible entry under `Unreleased`.

## Related history

- PRD [`independent-character-panel-prd.md`](../../../docs/independent-character-panel-prd.md):
  FR-01, FR-05, FR-07, M1 acceptance, §9 items 3-5, §10 reference table.
- [Preview actor](preview-actor.md): the lifecycle module this panel consumes; its world-visibility
  statements were corrected in this batch.
- Sibling repository `D:/code/cpp/skyrim/Highlight-Lootable-Corpses` at commit `7a7c51e` (GPL-3.0,
  same author): the ported D3D11 substrate (`src/render/dx11/d3d11_util.*`,
  `src/render/shader_manager.*`, `cmake/embed_shaders.cmake`) and the geometry collector
  (`src/render/geometry/render_geometry.*`). Each ported first-party file carries a header comment
  naming that repository, path and commit.
- Contract `.lcw/task-runs/m1-panel-window/implementation-contract-v1.md`: the batch that made the
  panel render — the hotkey panel that renders the preview character offscreen with its own camera and
  lighting, previously at a fixed pixel size.
- Contract `.lcw/task-runs/m1-panel-layout-input/implementation-contract-v1.md`: the previous batch —
  resolution-relative sizing, the normalised stored position, UI-like dragging with the plugin's own
  virtual cursor and the frozen look group, the two-axis camera fit, the one hairline border, and the
  configuration migration. Skinning was deferred there with its SWF reason, and the local
  CommonLibSSE-NG `MenuCursor` headers were read as exposing no way for a plugin to drive the engine
  cursor; the next batch revisited that and reached the opposite conclusion, because the engine drives
  the cursor itself while one of its own cursor-using menus is open.
- Contract `.lcw/task-runs/m1-panel-swf-menu/implementation-contract-v1.md`: this batch, revision 1 —
  the panel becomes a registered Scaleform menu whose chrome comes from an overridable
  `Interface\PlayerPanel\panel.swf`, the engine's own menu cursor replaces the plugin's virtual cursor
  and its native-cursor marshalling, the engine's UI input context replaces the manual look freeze, the
  built-in chrome becomes the logged fallback, and the checked-in standard-library generator produces
  and re-parses the default skin. Superseded by revision 2.
- Contract `.lcw/task-runs/m1-panel-swf-menu/implementation-contract-v2.md`: revision 2 —
  revision 1's review found that its layering left a skin almost nothing to show: the character
  composite was fully opaque and the inset it discarded was the built-in hairline, so a skin could only
  ever appear as a one- or two-pixel band. Revision 2 clears the offscreen target fully transparent in
  skin mode, keeps the character's straight alpha and blends it over the chrome the engine has already
  drawn, gives the composite pass its own source-alpha blend state, moves the skin inset into its own
  validated `PanelSkinInsetFraction` configuration value with an absolute pixel floor, and states the
  authoring contract that a skin supplies an opaque backdrop and a frame at least that thick. The
  built-in mode is unchanged. Superseded by revision 4.
- Contract `.lcw/task-runs/m1-panel-swf-menu/implementation-contract-v3.md`: revision 3 — the review of
  revision 2's result found that `ShaderManager::compile()` had no caller anywhere in the repository,
  so every shader accessor the passes read returned null and the panel never drew, with nothing in the
  build to report it. Revision 3 wired the compilation at data load with a guaranteed attempt inside
  `PanelRenderer::ensure_device_objects`, had a failure logged once without looping, and added the
  embedded-shader marker byte search over the built DLL as local acceptance evidence. Superseded by
  revision 4.
- Contract `.lcw/task-runs/m1-panel-swf-menu/implementation-contract-v4.md`: the current revision —
  identical to revision 3 except for its implementation model; the scope, requirements, acceptance
  criteria and constraints are unchanged.
