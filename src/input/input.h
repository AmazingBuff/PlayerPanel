#pragma once

PLUGIN_NAMESPACE_BEGIN

// F7/F8 hotkey observation (FR-05: observe-only; held/repeat events are
// rejected so a single press toggles exactly once). The panel's primary
// lifecycle is the inventory menu (panel/panel.cpp MenuSink); F7 remains
// the manual fallback.
class InputManager
{
public:
    InputManager() = delete;
    ~InputManager() = delete;
    InputManager(InputManager const&) = delete;
    InputManager(InputManager const&&) = delete;
    InputManager operator=(InputManager&) = delete;
    InputManager operator=(InputManager&&) = delete;

    // Registers the input sink on the game's input device manager. False =
    // the manager is unavailable and the panel hotkeys are disabled
    // (logged); the caller aborts the install, exactly as before the
    // module split.
    static bool install();
};

PLUGIN_NAMESPACE_END
