CharacterPanel scene-graph copy test package (probe revision in build-manifest.json,
experiment "CharacterPanel-scopy")

This build replaces the Actor assembly route with the manual copy route. It does
not create an NPC base or an Actor, and it does not modify the source
character's graph, parent, pose, inventory, or equipment.

Install SKSE/Plugins/CharacterPanel.dll AND its matching .pdb from this package
into a separate MO2 test mod that replaces CharacterPanel. A .pdb from another
build cannot symbolize this DLL's crash logs. Do not load the old
CharacterPanel, CharacterPanelProbe, or CharacterPanelProto at the same time:
the probe's F7/F8 conflict with this experiment. Use a test save and keep the
original DLL for rollback.

Every probe round ships as its own package, named for its revision
(CharacterPanel-scopy-<revision>-<version>.zip), and the first log line names
that build:

  SCOPY BUILD CharacterPanel-scopy-<revision>-<commit>-cl<commonlib>-<stamp> ...

Quote that line when returning a result: it is the only way to tell two probe
rounds apart. build-manifest.json carries the same identity plus the SHA-256 of
both binaries; the package step checks the packaged DLL really contains the
identity.

Target runtime is Skyrim AE 1.6.1170 with SKSE 2.2.6. No HKX, CBPC, or SMP
driver is implemented: a copy whose hair or cloth stays in the captured pose is
expected, not a failure.

Keys (pressed events only; no game input is consumed), with the inventory open
and the game paused:

  F7   release the old copy, then copy and audit the current third-person
       graph. The load and main menus refuse the copy, and so does the world
       with no inventory open.
  F8   start or stop drawing the copy that passed the audit
  F3   rotate the copy 90 degrees
  F4   release the copy
  F2   enable or disable the procedural idle (on by default once the copy is
       drawn)

The procedural idle drives a few of the copy's own joints from our own clock —
hips shifting sideways, tilting and turning, the spine counter-rolling and
breathing, the head looking around on its own slower cycle — so the figure is
alive while the game is paused. It reads the captured pose every frame and
recomputes, so nothing accumulates, and F2 off (or F4) returns the figure to
the captured pose on the next frame. Amplitudes and periods live in
src/character/idle_driver.h.

Two log lines describe what it is actually doing:

  SCOPY IDLE bound N/M channels[; missing: ...]
        which joints this body's skeleton provided
  SCOPY IDLE t=<seconds> 'joint' moved=(dx,dy,dz) rot=<deg> | ...
        every two seconds: how far each driven joint has travelled from the
        captured pose, and the net rotation its channels ask of it. This is the
        line that answers "does the waist move?" with numbers rather than an
        impression — a rotation-only joint stays at moved=(0,0,0) by
        construction, which is why the hips also carry a translation channel.

This skeleton splits the hips and the torso into two branches (the pelvis under
CME LBody, the spine under CME UBody), so the two are driven separately and a
pelvis rotation cannot carry the chest.

The S2 measurement probe that used to sit on F2 answered its questions over four
recorded rounds (docs/s2-anim-probe-evidence-2026-10-09.md) and is no longer
bound in this build: a probe build asks for it with
-DCHARACTER_PANEL_S2_PROBE=ON and then presses F6, which is free only in that
opt-in build.

The package build is otherwise unchanged: F7/F8/F3/F4 plus the idle on F2.

The F2 probe swings one bone of the copy and measures whether that change
reaches the actual draw. It runs inside the draw window in the fixed order the
PRD requires (base placement, probe, pass preparation, draw) and prints four
measurement lines per report, every 8 frames:

  SCOPY T0  base placement done: bone identity, its world matrix, witness
  SCOPY T1  after the swing: orientation change (degrees, from the relative
            rotation), witness displacement, and the skin matrix slot read
            BEFORE the swing
  SCOPY T2  at the pass that submits the target geometry: slot, numMatrices,
            frameID (pass-geometry=not-submitted when that pass never came)
  SCOPY T3  after the draw: whether the engine's own buffer update changed
  SCOPY T1c/T3c  what the sampled slot actually resembles: the nearest and
            second-nearest of ten engine-side candidates (bone world, captured,
            previous frame, each transposed, the bind transform, and the
            world/bind products), with their distances, for BOTH matrix
            buffers the skin keeps
  SCOPY TDUMP/TSLOT  once per F2 arm: every slot of the target skin, printed
            raw next to its bone's name and that bone's own world transform,
            including the second buffer. This is the round that decides the
            buffer's layout and whether slot i is bones[i]

The target line names the joint, its parent chain, the skin it is sampled
through and affected-geoms=<touched>/<skinned>, i.e. how much of the figure the
swing can move at all. The probe writes node transforms only; the skin matrix
buffers are sampled read-only. `*-rel=swung|captured|neither` says which of this
frame's two node orientations the slot's rotation block resembles more — an
observation, not a statement about what the buffer stores. `SCOPY ANIM probe
INVALID` means the witness sits on the swing axis and its displacement cannot
judge anything, so that run is not evidence either way.

Return the complete CharacterPanel.log (the log is overwritten on every launch:
copy it before restarting the game), the screenshots, and this package's
build-manifest.json. If the game crashes, return the CrashLogger log together
with the .pdb from this package.
