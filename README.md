# CharacterPanel

C++23 SKSE character preview plugin with Actor-assembled and audited scene-copy
rendering paths. Current source navigation, controls and limits live in
[Code maintenance](docs/code-maintenance.md).

```powershell
cmake --preset Release -DCHARACTER_PANEL_SCENE_COPY_EXPERIMENT=OFF
cmake --build build --config Release --target CharacterPanel
```

Set VCPKG_ROOT and use the existing Windows x64 VS2022 / MSVC 14.44.35207
configuration. CommonLibSSE and manifest dependencies remain required. The DLL
is build/Release/CharacterPanel.dll; metadata is generated in build/src/plugin.h.
First-party source globbing uses CONFIGURE_DEPENDS.

For the scene-copy / AFP configuration, configure build-scopy with
CHARACTER_PANEL_SCENE_COPY_EXPERIMENT=ON. Its current keys are F7 capture,
F8 draw, F2 mark and F4 drive. The CharacterPanelSceneCopyPackage target packages
binaries and their identity manifest without deploying them. Retired M0 and S2
probe targets are no longer built; historical runbooks below describe their
recorded revisions. See the maintenance document for exact build/test commands
and retained diagnostic reasons.

## Documentation

- [Display backend routing](docs/display-backend-routing-2026-10-13.md): PRD 0.11 §9.7, Actor previews for players/followers and OAR merchants, graph snapshots for corpses/containers, two-sided trade views, corpse equipment refresh and D01–D08 future checks. Not implemented yet.

- [AFP next validation and paused gaps](docs/afp-next-validation-and-paused-gaps-2026-10-13.md): PRD 0.10 review, 98→116 coverage, direct drawing and dual-output transitions; separate R08 physics, F00–F04 expressions and BM00–BM03 source body-deformation synchronization. New controls require a diagnostic build.

- [AFP1 final-output test sheet](docs/scene-copy-afp1-final-output-test-2026-10-13.md): historical AFP1 output/placement evidence. Its original controls describe that revision; current controls are F2 mark and F4 drive, as listed in [Code maintenance](docs/code-maintenance.md).

- [AFP1 verdict](docs/afp1-verdict-2026-10-13.md): the first in-game result — the pose track is readable and animation-skeleton-ordered (105/109 bones within one degree), and the parked puppet's graph generates an output with a live but empty pose track, which re-reads HKX42's node freeze as "there was no output to propagate".

- [AFP2 draw structural input](docs/afp2-draw-structural-input-2026-10-13.md): the read-only pass over the draw path that the same-Actor route needs before it writes anything — per-step sequence, the now-shared pass batch, the per-frame root translation it may not do, the camera entry points, and nine line-referenced risks.

- [Actor final-pose validation](docs/actor-final-pose-validation-2026-10-12.md): PRD 0.9 §6.6, proposed single-Actor rendering with manual application of final graph output; R00–R09 procedures, engine-reference comparison, blending, physics and lifecycle gates. Not implemented by the HKX44 package.

- [Independent physics validation](docs/s3-independent-physics-validation-2026-10-10.md): PRD 0.8 §6.5, proposed separate SMP/CBPC instances on a copied graph, V00–V10 game procedures, A/B evidence and teardown gates. Requires a new diagnostic build; the current DLL does not implement these interfaces.

- [Next HKX work package](docs/s2-hkx-next-work-package-2026-10-10.md): PRD 0.7, one real active clip, resource ownership, deterministic sampling, independent clock and H01–H05 result form; recorded-pose replay remains a limited fallback.

- [S2 animation handoff](docs/s2-animation-handoff.md): **start here for the next session on animation (FR-03)** — why the procedural idle cannot be verified, the engine-side Havok objects we can drive, the three data routes, the HKX1 work package and the numeric A/B protocol.

- [Next scene-copy test](docs/scene-graph-copy-next-test-2026-10-09.md): PRD 0.6 review, corrected probe order prerequisites, T0–T3 measurements and result form. Static display is preliminary; safe teardown is blocked and animation/physics remain unverified.

- [Scene-graph copy validation](docs/scene-graph-copy-validation.md): opt-in S0 experiment, F7/F8/F3/F4 controls, remote game runbook and twelve evidence questions; [result template](docs/scene-graph-copy-results-template.md). Independent animation/CBPC/FSMP drivers are later gates.

- [M0 handoff](docs/m0-handoff.md): historical state (M0 gate passed), verified engine facts, the next work package, and the test runbook.
- [PlayerPanel PRD](docs/player-panel-prd.md): product requirements for an independent character studio rendered alongside the game world, including composition order and acceptance criteria.
- [Community implementation references](docs/community-reference-supplement.md): evidence and limits from Dragon's Eye Minimap, Outfit Preview Selector, Apparel Preview, and SosGui, mapped to PlayerPanel milestones.
- [Stage-0 spike](docs/stage0-spike.md): historical rendering experiment notes; not validation of the current PRD.
- [M0 diagnostics](docs/m0-diagnostics.md): archived engine-evidence probe; source/tool references refer to Git history.
- [M0 capture report (2026-09-28)](docs/m0-capture-report-2026-09-28.md): analysis of the first user-run probe session (AE 1.6.1170 + Community Shaders); verifies routine offsets and the accumulator swap path.
- [M0 prototype](docs/m0-proto.md): historical rendering experiment; the accumulator-swap route (runs 1–8) was falsified, and F6 now arms one menu frame of pass redirection (v2).
