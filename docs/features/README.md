# Feature documentation

Developer-facing documents for the PlayerPanel feature modules. `README.md` at the project root
stays the short entry point; each feature gets one canonical document here that records its durable
intent, its code map, its invariants, its failure modes and how it is verified.

## Index

| Document | Module | Summary |
| --- | --- | --- |
| [Preview actor](preview-actor.md) | `src/preview`, `src/render/frame_hook.*` | M1: creates, dresses and destroys one non-persistent preview actor that mirrors the player's supported appearance and worn equipment, driven by the hotkey and a game-thread frame tick |
| [Preview panel](preview-panel.md) | `src/render/panel`, `src/render/dx11`, `src/render/shaders`, `src/input`, `tools/make_panel_swf.py` | M1: renders the preview character, its face and its worn equipment into a private offscreen target with its own camera and fixed lighting, composites it alpha-blended and inset into a resolution-relative window whose backdrop and frame both come from a swappable registered-menu SWF, drags it with the game's own menu cursor, keeps the in-world copy hidden, and falls back to the built-in opaque fill and hairline when no movie is available |

## Adding a feature document

- One document per feature module, named after the module (`preview-actor.md`).
- Start from the section list used by `preview-actor.md`: Purpose; Scope and non-goals;
  Architecture; Code map; Interfaces; Invariants; Failure modes; Dependencies; Tests and
  verification; Safe modification guidance; Synchronized files; Related history.
- Name only paths and symbols that exist, and link them relatively from this directory.
- Record unsupported behaviour explicitly instead of implying it, and list the verification that
  was actually executed, separately from the checks that are still manual.
- Add the new document to the table above in the same change as the code it describes.

## Related documentation

- [`README.md`](../../README.md) — project entry point, build environment and the selected feature
  list.
- [Workspace design-document index](../../../docs/README.md) — the product design documents that
  sit above this repository (PRD, rendering and CommonLibSSE-NG references), one directory up from
  the repository root.
