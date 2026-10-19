# Historical reports

This is the complete report index previously kept in README. Records describe their own revisions: old controls, removed source paths, trial outcomes, pending validation and future proposals are historical statements, not current implementation or build instructions. Phrases such as “start here”, “now” or “not implemented yet” in the archived descriptions refer to that stage.

Use [build guidance](build.md), [SceneGraph](scene-graph.md), [animation](animation.md) and [maintenance](code-maintenance.md) for current behavior. Dated reports and requirements remain unchanged.


The following reports describe their recorded revisions. Former source paths,
Actor/puppet controls and validation status are historical; current controls and
code navigation are in the documents above.

- [Historical panel idle selection and fast weapon changes](oar-preview-query-2026-10-16.md): verified OAR 3.1.6/3.2.1 queries, preview Random/variant state, per-layer idle cycles, continuous blending and latest-equipment interruption. The oar-idle2 package requires the scene-copy configuration; game verification is pending.

- [UBE transparency trial](ube-transparency-test-2026-10-15.md): material handling, package identity and local checks. The user confirmed correct rendering on another computer on 2026-10-16.

- [OAR hook history](oar-hooks.md): binary identities and earlier diagnostics; current selection/playback behavior is described in the actor animation document above.


- [AFP10g round](afp10-spawn-shape-dress-test-2026-10-14.md): the record's own kit is cut at the source (the duplicate's container cleared and both outfits nulled at spawn, the copy route's recipe), the prune's world-cadence branch works again (its old gate shared a counter the worn sync reset), the figure's quiver now follows the player's current ammo (`equipment_sync_ammo`), and the body-slot list covers wigs, capes, belts and misc attachments it used to skip.

- [AFP10 test sheet](afp10-spawn-shape-dress-test-2026-10-14.md): the three reports of 2026-10-14 and what each work package changed - nothing perceptible during the assembly window (fade zero, and the window closes as soon as the census reports the shell bound and settled), the player's worn body items mirrored onto the figure, and the player's body shape copied through RaceMenu skee64's BodyMorph interface instead of the random preset OBody NG assigns to every newly placed NPC.

- [AFP9 verdict 9: the character controller](afp9-controller-park-verdict-2026-10-13.md): with the panel stable, the last visible defect was an interactable shell beside the player while the figure was invisible - `SetPosition`'s second parameter (`a_updateCharController`) had been passed false, so the collision and activation target never left, and the 3D kept being derived from the controller's stale position. The park now warps the controller when the reference moves.

- [AFP9 verdict 8: the skin cache gate](afp9-skin-cache-verdict-2026-10-13.md): the first complete panel image (whole figure, centred, holding the weapon) and, in the same session, frames with only the weapon - `NiSkinInstance::frameID` had already been set for that render frame before the studio placement was written, so the engine kept matrices computed from the parked bone worlds and the skinned body vanished while the rigid weapon followed its node. The batch now invalidates that id and lets the engine recompute, with no write to `boneMatrices`.

- [AFP9 verdict 7: the framing measured a stale bound](afp9-stale-bound-verdict-2026-10-13.md): the first studio-target readback showed the figure small, dark and off-centre while `center-d` reported 0 - the placement had been centred against a geometry `worldBound` cache still holding the pre-park position, throwing it ~2820 units deeper. Framing now measures live bone world positions and the park is re-asserted on paused frames too.

- [AFP9 verdict 6: evidence dump restored](afp9-evidence-dump-verdict-2026-10-13.md): six rounds of judging the direct-draw figure from external frame captures was the real cost, so the project's pre-172984a studio-target TGA readback is back (`r01-panel-NNN.tga`, first frame and every 300), with an `R01 census` skin line and a `park-3d ref-now` read-back; the park is re-asserted every unpaused frame because the engine re-derives the transform each frame.

- [AFP9 verdict 5: the transform layer](afp9-transform-layer-verdict-2026-10-13.md): a live actor's subtree is re-derived from its root's `local` every frame (a world-space park write was undone 1.3 s later) and the skin state is computed during the graph update (a placement applied after it moved the rigid axe but not the skinned body). Placement now writes `local` plus the derived worlds, and R01 applies it before the puppet's graph update and restores it after the draw.

- [AFP9 verdict 4: the puppet never left the player](afp9-park-verdict-2026-10-13.md): the park moved only the reference position and this actor's 3D is never re-derived from it, so a fully visible clone stood beside the player and the F6 figure - still at the player's world position - projected arbitrarily. The park now moves the 3D subtree by the same offset (idempotent against repeated parks) and logs the reference/3D reconciliation.

- [AFP9 verdict 3: F6 framing](afp9-f6-framing-verdict-2026-10-13.md): the direct-draw figure was drawn but projected far outside the frustum. The framing wrote the root's `local` and cascaded, and a downward pass never moves the object it is called on - the figure stayed at its park depth. Fixed by a world-space delta applied downward (the copy route's own solve) plus a `center-d` assertion; the round also confirmed the composite fix (`match=1`, scene-graph route renders).

- [AFP9 verdict 2: black panel](afp9-black-panel-verdict-2026-10-13.md): both routes drew the figure into the studio target yet the panel stayed empty. The composite bound that texture as a sampler source while it was still bound as the output target, and D3D11 silently unbinds a resource used as both. Fixed by ordering (release the target, then bind the source), separate capture scopes, and a `SCOPY COMPOSITE src/bound/match` readback line; the round's real coverage-audit data is recorded too.

- [AFP9 verdict 1](afp9-verdict-2026-10-13.md): the first AFP9 game session - world-running verification held (0.96, match=1) and the F4 chain reached the paused forced frame, which then crashed inside the new coverage audit. Root cause: `NiSkinInstance::bones` entries were dereferenced to read names; they may not be readable nodes. Fixed by pointer-only palette walks plus tree-resolved classification; field logs archived.

- [AFP9 test sheet](afp9-coverage-and-direct-draw-test-2026-10-13.md): coverage ledger audit (98→116 slot reconciliation, named unreachable/unprocessed bones classified against the skin palette; no blind extension) and the R01 same-Actor direct-draw experiment (F6 toggle, A4/A5 sampling, full framing restoration). Implemented; game validation pending.

- [Display backend routing](display-backend-routing-2026-10-13.md): PRD 0.11 §9.7, Actor previews for players/followers and OAR merchants, graph snapshots for corpses/containers, two-sided trade views, corpse equipment refresh and D01–D08 future checks. Not implemented yet.

- [AFP next validation and paused gaps](afp-next-validation-and-paused-gaps-2026-10-13.md): PRD 0.10 review, 98→116 coverage, direct drawing and dual-output transitions; separate R08 physics, F00–F04 expressions and BM00–BM03 source body-deformation synchronization. New controls require a diagnostic build.

- [AFP1 final-output test sheet](scene-copy-afp1-final-output-test-2026-10-13.md): historical AFP1 output/placement evidence. Its original controls describe that revision; current controls are in [SceneGraph previews](scene-graph.md).

- [AFP1 verdict](afp1-verdict-2026-10-13.md): the first in-game result — the pose track is readable and animation-skeleton-ordered (105/109 bones within one degree), and the parked puppet's graph generates an output with a live but empty pose track, which re-reads HKX42's node freeze as "there was no output to propagate".

- [AFP2 draw structural input](afp2-draw-structural-input-2026-10-13.md): the read-only pass over the draw path that the same-Actor route needs before it writes anything — per-step sequence, the now-shared pass batch, the per-frame root translation it may not do, the camera entry points, and nine line-referenced risks.

- [Actor final-pose validation](actor-final-pose-validation-2026-10-12.md): PRD 0.9 §6.6, proposed single-Actor rendering with manual application of final graph output; R00–R09 procedures, engine-reference comparison, blending, physics and lifecycle gates. Not implemented by the HKX44 package.

- [Independent physics validation](s3-independent-physics-validation-2026-10-10.md): PRD 0.8 §6.5, proposed separate SMP/CBPC instances on a copied graph, V00–V10 game procedures, A/B evidence and teardown gates. Requires a new diagnostic build; the current DLL does not implement these interfaces.

- [Next HKX work package](s2-hkx-next-work-package-2026-10-10.md): PRD 0.7, one real active clip, resource ownership, deterministic sampling, independent clock and H01–H05 result form; recorded-pose replay remains a limited fallback.

- [S2 animation handoff](s2-animation-handoff.md): **start here for the next session on animation (FR-03)** — why the procedural idle cannot be verified, the engine-side Havok objects we can drive, the three data routes, the HKX1 work package and the numeric A/B protocol.

- [Next scene-copy test](scene-graph-copy-next-test-2026-10-09.md): PRD 0.6 review, corrected probe order prerequisites, T0–T3 measurements and result form. Static display is preliminary; safe teardown is blocked and animation/physics remain unverified.

- [Scene-graph copy validation](scene-graph-copy-validation.md): opt-in S0 experiment, F7/F8/F3/F4 controls, remote game runbook and twelve evidence questions; [result template](scene-graph-copy-results-template.md). Independent animation/CBPC/FSMP drivers are later gates.

- [M0 handoff](m0-handoff.md): historical state (M0 gate passed), verified engine facts, the next work package, and the test runbook.
- [PlayerPanel PRD](player-panel-prd.md): product requirements for an independent character studio rendered alongside the game world, including composition order and acceptance criteria.
- [Community implementation references](community-reference-supplement.md): evidence and limits from Dragon's Eye Minimap, Outfit Preview Selector, Apparel Preview, and SosGui, mapped to PlayerPanel milestones.
- [Stage-0 spike](stage0-spike.md): historical rendering experiment notes; not validation of the current PRD.
- [M0 diagnostics](m0-diagnostics.md): archived engine-evidence probe; source/tool references refer to Git history.
- [M0 capture report (2026-09-28)](m0-capture-report-2026-09-28.md): analysis of the first user-run probe session (AE 1.6.1170 + Community Shaders); verifies routine offsets and the accumulator swap path.
- [M0 prototype](m0-proto.md): historical rendering experiment; the accumulator-swap route (runs 1–8) was falsified, and F6 now arms one menu frame of pass redirection (v2).
