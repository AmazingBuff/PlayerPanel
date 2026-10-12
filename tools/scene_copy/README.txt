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
and the game paused. Three keys, and the AFP route owns all of them: the
scene-copy route keeps F7 (re-capture, which also releases the previous copy) and
F8 (draw it), while its old F2 idle / F3 rotate / F4 release bindings are gone.

  F2   mark: one read-only sample of the same-Actor forensics (see "Same-Actor
       final output (AFP1)" below). Changes nothing anywhere.
  F3   drive: off -> tick -> tick+apply -> step -> off. "off" is freeze-all - no
       graph tick on the panel's clock and no pose written. "tick" advances the
       puppet's own behaviour graph on the panel's clock. "tick+apply" also
       writes the graph's final output pose into the puppet's skeleton.
       "step" is a one-shot: exactly one declared dt (1/60 s) and one
       application, then the state falls back to off. One key carries R02's
       push/freeze control, R04's A/B pair and single-step, because this setup
       has three keys, not nine.
  F4   placement: parked-deep <-> held-pinned. The park is what hides the shell
       from the world, and AFP1 measured the parked puppet's pose track as live
       but EMPTY (zero elements, numAnimBones 0) while the player's carried 116 -
       so this is the one variable that isolates whether the emptiness is the
       park or the graph. The mark line prints which placement was measured.

The application stage (F3's tick+apply)

The engine applies the behaviour graph's pose to the skeleton every frame while
the world runs; while the panel pauses the world it does not (that is HKX42's
finding, and AFP1 re-read it as "the parked puppet has no output to propagate").
This stage does that half, strictly:

  - it writes only a mapping it verified against the engine's OWN applied node
    locals through the same scoring and decision the forensics print, and blocks
    with the reason when that fails. Nothing unverified drives a skeleton;
  - verification therefore happens while the world is RUNNING - the engine is
    applying the pose then, so its node locals are the reference - and the write
    happens while the panel pauses the world. Cycle F3 to tick+apply in the
    running world first, then open the inventory. If you arm it while already
    paused it reports `blocked reason=unverified-world-first` instead of
    guessing, and it never re-scores after it has started writing (the locals
    would then be its own);
  - every node is resolved from the engine's live bone table on the frame it
    writes; no node pointer is kept across frames, which is what crashed HKX43;
  - it writes local transforms and recomputes the subtree's worlds once - the
    only write path this codebase has measured reaching a draw.

  SCOPY AFP2 drive=tick+apply apply=on tick=off (...)      one line per F3 press
  SCOPY AFP2 verify generation=N samples=N bone-nodes within-1deg=A/B ratio=X
        | skeleton within-1deg=C/D ratio=Y | match=0|1 reason=matched|...
  SCOPY AFP2 apply blocked reason=... mapping=... verified=0|1 generation=N
        (at most once per ~120 frames while it stays blocked)
  SCOPY FP A2 drive=... apply=... mapping=skeleton|bone-nodes|none verified=0|1
        generation=N written=N unreachable=N frames=N verifications=N
        max-rot=Xdeg max-pos=X blocked=...

`verify` is the R03 comparison: the same output pose scored against the two
index orders on the engine's own applied locals. `written` counts the bones the
last frame actually posed and `unreachable` the pose indices the chosen mapping
could not resolve to a bone table entry - a name the engine's table does not
carry is counted, never silently dropped. `blocked=matched|verified` means the
stage is allowed to write; any other value names the condition that stopped it.

Same-Actor final output (AFP1)

This round opens the route PRD 0.9 section 6.6 describes; it does not implement it yet. That route
draws the puppet Actor's OWN third-person 3D and, while the world is paused, applies the behaviour
graph's FINAL output pose to that Actor's own skeleton: no detached copy, no single clip resampled
onto a second skeleton, no empirical track solver, no procedural fallback.

Two things have to be measured before any build may write a pose into a skeleton (test sheet R00),
and both are measured here, read-only:

  1. whether the graph's own output buffers (BShkbAnimationGraph::generatorOutputs[0] and [1]) hold a
     readable final pose, and which of the two buffers is the current one;
  2. which index order that pose uses - the engine's boneNodes table, or the animation skeleton -
     because the engine applied it during normal play and the manual route has to reproduce exactly
     that.

Press F2 for one mark. A mark samples both the puppet and, as the reference, the player (whose
graph the engine drives by itself), and writes nothing anywhere:

  SCOPY FP mark=N paused=0|1 tick=on|off manual-ticks=N placement=parked-deep|held-pinned
  SCOPY FP A0 subject=... output0=0x... pose0=N reason0=... output1=0x... pose1=N reason1=...
        bones=N/N
  SCOPY FP A0A1 subject=... pose0=N/N pose1=N/N pose-max-rot=Xdeg nodes=N/N nodes-moved=N
        nodes-max-rot=Xdeg nodes-max-pos=X nodes-rebound=N
  SCOPY FP A3 physics=skipped reason=first-round-physics-off
  SCOPY FP camera ui3d=0x... ui3d-camera=0x... accumulator=0x... accumulator-camera=0x...
        world-camera=0x... accumulator-is-ui3d=0|1 accumulator-is-world=0|1 ui3d-is-world=0|1
  SCOPY FP camera ui3d-camera world-t=(x,y,z) view=(x,y,z) w2c-t=(x,y,z) near=X far=X
        l=X r=X t=X b=X ortho=0|1 cached-pos=(x,y,z) cached-rot-col2=(x,y,z)
        ui3d-frustum l=X r=X t=X b=X near=X far=X
  SCOPY FP figure subject=... root=0x... world-t=(x,y,z) view-depth=X lateral=X
        eye-distance=X fade=X bound-center=(x,y,z) bound-radius=X
  SCOPY FP geoms subject=... root=0x... geometries=N skinned=N property=N none=N effect=N
        renderer-data=N skinned-with-rd=N
  SCOPY FP census subject=puppet form=... gen=N graph=0x... root=0x... bone-nodes=N bound=N
        pose-local=N num-anim-bones=N tracks-in-lod=N lod=N skeleton='...' skeleton-bones=N
  SCOPY FP slot=0|1 subject=... out=0x... tracks=N on-tracks=[i,j,...] exists=0|1 valid=0|1
        type=N flags=N num-data=N cap=N elem=N off=N on=X data=0x... readable=0|1 reason=...
  SCOPY FP delta slot=0|1 subject=... prev=N now=N compared=N changed=N
        max-rot=Xdeg max-pos=X vs-mark=N
  SCOPY FP nodes subject=... bones=N compared=N rebound=N gone=N missing=N changed=N
        max-rot=Xdeg max-pos=X vs-mark=N
  SCOPY FP order=bone-nodes slot=0|1 subject=... sampled=N bones=N compared=N
        within-1deg=N beyond=N ratio=X max-rot=Xdeg max-pos=X max-scale=X outliers='...','...'
  SCOPY FP order=skeleton slot=0|1 subject=... sampled=N bones=N named=N compared=N
        within-1deg=N beyond=N ratio=X max-rot=Xdeg max-pos=X max-scale=X outliers='...','...'
  SCOPY FP world order=skeleton slot=0|1 subject=... compared=N within-1deg=N beyond=N
        ratio=X max-rot=Xdeg max-pos=X max-scale=X outliers='...'
  SCOPY FP necessary subject=... outliers=N necessary=N helper=N palettes=N/N entries=N
        verdict=classified|unbound-palette
  SCOPY FP verdict subject=... match=0|1 reason=... best=<order>/slot<N> within-1deg=N/N
        ratio=X max-rot=Xdeg max-pos=X max-scale=X world-within-1deg=N/N named=N nodes-moved=N

How to read it:

  A0 / A0A1            the mark is now taken twice in one frame: A0 before anything advanced it, A0A1
                       after the drivers ticked. That pair is what answers "is OUR tick what moved
                       the output", which no cross-mark delta can separate from the engine having run
                       in between - and in a paused world only one of the two can move anything.
                       pose0/pose1 are the element counts of the two buffers at A0 and after the
                       tick; nodes-moved / nodes-rebound are the node locals this frame changed, with
                       a rebuilt tree reported as rebound rather than as a phantom move.
  A3                   physics=skipped until the physics round: written out so the six observation
                       points are complete and an absent stage cannot read as a passing one.
  reason=ok            the whole chain validated: output object, tracks blob, track header, element
                       size (a full 0x30 QsTransform) and the pose data span. Any other reason names
                       the step that failed, so a buffer this build does not understand reports
                       which assumption broke instead of printing zeros that look like data.
                       reason=pose-empty is its own answer: the track is part of the output
                       (onFraction=1) but carries zero elements - the graph generated no pose, which
                       is what the parked puppet does and the player never did.
  WHICH BUFFER         do not read the index: AFP1 saw the live buffer at slot 0 in every mark while
                       the two buffer objects themselves moved, so the live one is the one with
                       valid=1 / num-data>0. Both must be read and compared, never assumed.
  delta slot=...       what the pose in that buffer did since the previous mark. This is the line
                       that answers "does our manual tick (F3 on) move the final output, and does
                       freezing it stop the output": without it, an idle that holds still and an
                       output that is never written look the same.
  nodes subject=...    what the engine-applied node locals did since the previous mark. changed=0
                       while delta changed>0 is exactly the HKX42 finding (the graph advances, the
                       nodes do not follow); changed>0 means this Actor's nodes are being driven.
  order=... / verdict  the calibration. Each candidate (index order x output slot) is scored against
                       the node locals the engine itself wrote; ratio is the fraction of compared
                       bones that the candidate reproduces within one degree, `beyond` counts the
                       ones that missed and `outliers` names up to eight of them - R03 asks which
                       bones, because a helper branch can be skipped while a target mesh's own bone
                       cannot. `max-scale` is the third space, measured and never gating: a pose
                       carries three scales where a NIF node carries one. The verdict names the best
                       candidate and the decision `decide_pose_order` reaches on those numbers - the
                       same call the application stage makes: match=1 reason=matched, or match=0
                       with reason=no-candidate / insufficient-compared / below-ratio.
  world order=...      the same pose composed along the animation skeleton's own parent table and
                       compared against the nodes' worlds. R03 asks for local and world separately
                       because they disagree exactly where the two hierarchies do: a helper branch
                       hangs a bone where the skeleton does not. This line appears only for the
                       skeleton order, the one that has a real parent table to compose with.
  necessary ...        of the bones nothing reproduced, how many a skinned mesh actually reads - the
                       palette `NiSkinInstance::bones` points at. A bone a target mesh reads cannot be
                       skipped; a helper branch or a cloth chain the panel does not draw can. `palettes=
                       bound/skinned` is what makes the answer honest: an unbound palette (a figure the
                       engine has never rendered) means the classification is unavailable, not that
                       nothing is necessary.
  geoms ...            the Actor's own geometry inventory, read-only: how many geometries its 3D root
                       carries, how many are skinned, how many have a shader property (and how many of
                       those are EffectShader by class), and - the field runs 49-52 died on - how many
                       already have device-side `rendererData`. A figure the paused world never
                       rendered has none, and every pass generation dies on it.
  camera ...           the camera chain the studio draw renders through, read-only. The pass
                       submission takes no camera and writes no projection, so the view the figure is
                       projected with is engine state left in place - and the route that must not
                       translate the Actor per frame has to adapt that camera instead.
                       accumulator-camera is the decisive identity: it is the camera the accumulator
                       generating these passes was started with, so accumulator-is-ui3d=1 means the
                       UI3D camera (the one whose view axis the studio anchor is built from) is the
                       object to move. `view` is row 2 of worldToCam, `w2c-t` its translation.
  figure ...           where the subject's own 3D root sits: its world translation, how far along the
                       camera's view axis that is (view-depth), how far off that axis (lateral), and
                       the fade. A camera-adapting framing has to drive lateral to zero and choose a
                       view-depth; a fade of 0 makes the engine skip the node entirely, which is why
                       it is printed next to the position rather than left to be discovered.
  named=N              the skeleton order only: how many bones of the animation skeleton resolved to
                       a node of this Actor by exact name (the HKX1/HKX40 name bridge reach).
  gen=N                how many times this subject's 3D root or its graph changed identity since the
                       first mark. That is exactly the event that invalidates a mapping or a pose held
                       across frames (R02/R06 ask for a generation next to the graph and the bone
                       table), so a rebuilt tree is numbered here instead of being inferred from
                       rebound counts.
  on-tracks=[...]      which standard tracks that buffer currently carries. The pose (2) is what this
                       round needs; world-from-model (0) and extracted motion (1) are what the
                       display side will need later for root motion, and they come free with the
                       header read.
  nodes-moved=N        carried on the verdict line, because a calibration against a frozen tree
                       measures nothing and that has to be readable next to the number it qualifies.

A reading that means something, in four presses:

  1. In the running world (not paused) press F2. nodes-moved should be non-zero and the verdict
     should name one order/slot with a high ratio: that is the engine's own answer on this machine
     and skeleton, and it is the reference every later step is judged against.
  2. Open the paused inventory. Press F2 (baseline), wait a few seconds with F3 on, press F2
     again: delta slot=... changed>0 says the manual tick really does move the final output.
  3. Press F3 (tick off), press F2, wait, press F2. Both the pose delta and the node delta should
     be zero - a frozen graph, not a hidden one.
  4. Press F4 (placement=held-pinned), wait for the assembly window, press F2 twice. This is the
     measurement AFP1's empty puppet pose asks for: if the pose track's num-data turns 116 and
     num-anim-bones turns 116 here, the emptiness was the park (the engine does not animate what it
     has culled); if it stays 0, the puppet's graph never generates an output at all and the route
     needs that answer instead. Press F4 again to go back to parked-deep - held means the shell
     stands next to the player (fade 0.05), which is diagnostic only.
  5. Return the whole block, plus whether the puppet was born at all (SCOPY PUPPET born ...).

The camera and figure lines come with every mark, so the same four presses also answer the display
question: whether the accumulator that generates the studio's passes was started with the UI3D
camera (accumulator-is-ui3d), and where the puppet's own root actually sits relative to that camera
(view-depth, lateral, eye-distance, fade). Both are read-only; nothing is moved to find out.

If the puppet's slots report reason=output-unreadable or reason=no-pose-track while the player's do
not, that is a finding about the parked Actor and not a crash: return both blocks.

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

This build also runs the first step of the S2 animation work (HKX1): right after
F7 captures and audits a copy, it reads the source character's animation graphs
and reports how the engine's animation skeleton maps onto the copy's nodes. It
is read-only — nothing in the source character, the copy, or the engine's graphs
is written — and it prints:

  SCOPY ANIM source form=<id> graphs=N copy-nodes=N
  SCOPY ANIM graph[i] project='...' holder=... root=... bone-nodes=N
        anim-bones=N behavior-graph=... root-generator='...' binding-set=...
        bindings=N pose-local=N
  SCOPY ANIM skeleton graph=i name='...' bones=N bone-nodes=N
        bone-node-names-agree=K/N matched=N ambiguous=N duplicate-nodes=N
        parent-ancestors=N missing='...' wrong-parent='...'
  SCOPY ANIM gate verdict=PASS|PASS-AMBIGUOUS|INCOMPLETE|UNAVAILABLE graph=i
        matched=N/N ambiguous=N

`matched` counts the bones of the engine's animation skeleton that name a node
of the copy exactly; `missing` samples the difference (up to eight names — the
counts beside it say how many there are). PASS means every bone resolved,
PASS-AMBIGUOUS means it did but at least one bone name is shared by several
nodes (modded hair and cloth chains repeat skeleton names), INCOMPLETE means
some bone has no node, and UNAVAILABLE names the pointer that was missing
instead of reporting numbers. `parent-ancestors`/`wrong-parent` compare the
skeleton's own parent relation against the node hierarchy: this rig's spine
hangs under CME UBody while its pelvis hangs under CME LBody, so a disagreement
there is expected and does not fail the gate. `holder`/`root` say which of the
character's graphs is the one that was captured (the third-person graph);
`bindings=N` is how many animations that character has loaded.

Return the whole block with a result: the next round picks how to sample an
animation from it.

This build also replays the source character's own current pose into the copy and
measures how far the copy lands from it (HKX2). The engine holds that pose in
hkbCharacter::poseLocal, so the write path the animation sampling will use is
verified without any clip: bone to node by exact name, Havok's quaternion to
NiMatrix3, the local write, one downward world recompute. It poses the copy four
ways and prints the worst bone-to-source distance for each, next to a control
(pose the copy half a radian off the source first) that says whether the
measurement is sensitive at all:

  SCOPY ANIM map graph=i bones=N matched=N helper-unresolved=N other-unresolved=N
  SCOPY ANIM replay pose entries=N scale-off-from-one=N
  SCOPY ANIM replay control max-pos-delta=X max-rot-delta=Ydeg bones=N
  SCOPY ANIM replay order=skeleton|bone-nodes quat=direct|transposed
        written=N max-pos-delta=X max-rot-delta=Ydeg bones=N   (four lines)
  SCOPY ANIM replay restored max-pos-delta=X max-rot-delta=Ydeg bones=N
  SCOPY ANIM replay verdict match=<order>/<quat> control=Xu best=Yu

`control` must be clearly large; the candidate the verdict names is the engine's
own indexing and quaternion convention, and the other three should be far off.
`match=none` means no candidate reproduced the source, which is a real answer about
the conversion rather than a silent failure. `restored` proves the figure was put
back. Nothing is written to the source character or the engine's graphs, and the
copy is left exactly as captured.

This build also reports the animation list and the shape of the binding set's
elements, as the evidence that closed that route off. The names are typed, so an
index can be named; the elements turned out to be 0x30-byte stubs with no
animation binding inside them, which is why the animation itself is reached
through a clip generator instead (below):

  SCOPY ANIM catalogue character='...' names=N bindings=N
  SCOPY ANIM catalogue idle-names=K/N first='...'
  SCOPY ANIM catalogue idle-plain first='...'
  SCOPY ANIM catalogue idle-base first='...'
  SCOPY ANIM catalogue elements=K object-like=K/N scan=0x80
  SCOPY ANIM catalogue candidate offset=0xNN as=value|pointer valid=K/N reasons='...'
  SCOPY ANIM catalogue raw index=K qwords='...'     (two elements, first 0x80 bytes)
  SCOPY ANIM catalogue probe index=K name='...' type=spline|interleaved|... duration=...s
        frames=K tracks=K animation-tracks=K skeleton-name='...' bones='...' copy-resolved=K/N

The two raw lines are the ones that settled the layout question, and every read
goes through a committed-page check; nothing here calls a virtual function on a
candidate or samples anything.

This build plays one of the source character's own animations on the copy (HKX6).
The binding set's elements turned out to be 0x30-byte stubs with no animation
binding in them, so this route uses the fully typed hkbClipGenerator instead: a
bounded walk of the behaviour graph follows a state machine's typed states and,
for everything else, only pointers whose first word looks like a vtable, and
believes a node only when its animation name looks like an animation file, its
binding passes the structural checks, and its playback mode and speed are ones
the engine defines. Sampling goes through SampleIndividualTransformTracks, which
takes no chunk cache, so spline-compressed animations need no extra structure.

  SCOPY ANIM play search objects=N clips=K first='...'
  SCOPY ANIM play clip='Animations\female\mt_idle.hkx' type=spline duration=3.40s
        tracks=116 copy-resolved=109/116 truth-bones=116
  SCOPY ANIM play ground-truth best-t=...s max-pos-delta=... max-rot-delta=...deg
        worst-pos-delta=... bones=109
  SCOPY ANIM play ground-truth verdict=match|none best=... worst=...
  SCOPY IDLE enabled (F2); driver=Animations\female\mt_idle.hkx

The ground-truth pair is the one to read: the clip is sampled at every phase and
compared against the pose the engine itself holds, so a small best next to a much
larger worst says the sampling chain reproduces the engine. copy-resolved is how
many of the clip's tracks land on nodes of the copy - the rest are bones this
body does not carry.

F2 now plays that clip from the studio's own clock, because the game is paused
and nothing else advances a pose; when no clip was found it falls back to the
procedural idle, and the line above says which one it chose. F7 prints the
catalogue and the discovery, F8 draws, F3 rotates, F4 releases.

The package build is otherwise unchanged: F7/F8/F3/F4 plus the animation on F2.

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
