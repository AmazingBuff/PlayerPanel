CharacterPanelProbe diagnostic package

This package is an opt-in, read-only evidence probe for the blocked experimental
studio renderer. It does not render a panel, patch the renderer, create actors,
attach scene objects, or change gameplay state.

Before testing, disable the old CharacterPanel plugin. Run the original
renderer scenario first, then repeat with Community Shaders 1.9.1 enabled.

Enter a save, open the SkyUI inventory, highlight an item with a visible 3D
model, and press F7 after the model appears. Close the inventory and press F8.
Exit normally. Return the capture-NNN folders, the package build-manifest.txt,
and CharacterPanelProbe.log from the standard SKSE log directory. Captured
executable snippets are private diagnostic evidence and should not be
redistributed.
