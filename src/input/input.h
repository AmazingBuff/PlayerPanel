#pragma once

PLUGIN_NAMESPACE_BEGIN

// The plugin's single engine input sink: the panel toggle hotkey and the left-button edge the panel
// drag needs. It observes events and never consumes them, and it performs no engine mutation, because
// it runs inside an engine callback; the game-thread frame tick is what acts on what it recorded.
//
// It deliberately owns no cursor behaviour: the engine drives the cursor while the panel's own menu is
// open, and the drag reads that cursor instead of integrating one, so this module contains no user32
// cursor call at all.
class InputManager
{
public:
    InputManager() = delete;
    static void install();
};

PLUGIN_NAMESPACE_END
