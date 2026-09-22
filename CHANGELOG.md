# Changelog

## [Unreleased]

### Added

- Add a non-persistent preview character that mirrors the player's supported appearance and worn equipment.
- Add a hotkey panel that renders the preview character and its worn equipment into a window with its own camera and lighting.
- Add mouse dragging and a saved position to the preview panel, and size the panel from the render height so it reads the same on 16:9, 21:9 and 32:9 screens.
- Add SWF-driven panel chrome: the panel now loads a swappable `Interface\PlayerPanel\panel.swf` through a registered menu, so UI reskins can restyle it without touching game logic. A skin supplies the panel's own opaque backdrop and its frame, the character is composited with straight alpha over the chrome the engine has already drawn and inset by the configured `PanelSkinInsetFraction`, and the skin's frame is never overdrawn.
- Add `tools/make_panel_swf.py`, a standard-library generator that produces and re-parses the default panel skin SWF.

### Changed

- Change the hotkey to toggle the preview character instead of the plugin master switch.
- Change the panel sizing and margin configuration keys to fractions; the previous absolute pixel keys are ignored and reported once in the log.
- Change the panel to use the game's own menu input and cursor instead of the plugin-managed cursor and look freeze.
- Change the `PanelCursorSensitivity` key to be ignored and reported once in the log, now that the panel follows the game's own menu cursor.
- Change the built-in dark fill and hairline border to be the panel's fallback chrome, used when no skin movie is available.
- Change the panel's character composite to alpha-blend over a skin's chrome instead of painting an opaque rectangle, so a skin's backdrop and frame stay visible behind and around the character; the skin inset is now its own validated `PanelSkinInsetFraction` key rather than the built-in hairline.

### Fixed

- Fix the panel never drawing: the shader compilation step had no caller, so the panel's passes initialised with no shaders and nothing was rendered.
