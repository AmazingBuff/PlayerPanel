# OAR public ABI headers

The condition headers and `OpenAnimationReplacerAPI-Animations.h` are unmodified copies from
[OpenAnimationReplacer](https://github.com/ersh1/OpenAnimationReplacer), by Ersh
and contributors, commit `f4e7688b065175aff70aa523073857911e15aca3` (3.2.1).
They are covered by the upstream GPL-3.0 license; this repository also uses
GPL-3.0. The original declarations and comments are retained.

The Animations header comes from
[`src/API/OpenAnimationReplacerAPI-Animations.h`](https://github.com/ersh1/OpenAnimationReplacer/blob/f4e7688b065175aff70aa523073857911e15aca3/src/API/OpenAnimationReplacerAPI-Animations.h)
at that same commit. The pinned upstream **LF Git blob** has SHA-256
`bd547063601cdf27ac0e0f014ef3819b54dd50048af048501ae0df9040a69242`.
The local Windows checkout with **60 CRLF line endings** has raw-byte SHA-256
`a6e7cc2a571dea4bc64eeea5cdd40536c6b18806eeeb70d98d7c6da96ffb1c2a`.
These are the same declarations/comments after LF normalization. Git's staged
LF header blob is checked against the pinned upstream blob; a raw checksum
comparison must account for checkout newline conversion. The header itself
has not been edited.
The V1 interface and 80-byte replacement-info layout were checked against the
supported shipped OAR 3.1.6 and 3.2.1 DLL/PDB pairs. This does not admit newer
unverified profiles. Only `GetCurrentReplacementAnimationInfo` is called on
actual world clips through the dynamically requested module-owned interface;
the state-clearing methods and synthetic preview registration are unused.

Only the public condition/component and Animations V1 ABIs are consumed. No upstream implementation or
upstream build system is linked into CharacterPanel. Binary provenance and
the supported preview-evaluation boundary are documented in
[OAR hooks](../../../../docs/oar-hooks.md).
