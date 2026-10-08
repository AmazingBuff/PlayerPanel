#pragma once

PLUGIN_NAMESPACE_BEGIN

// Experimental copy commands (F7/F8/F3/F4) are queued for the render bracket.
class InputManager
{
public:
    InputManager() = delete;
    ~InputManager() = delete;
    InputManager(InputManager const&) = delete;
    InputManager(InputManager const&&) = delete;
    InputManager operator=(InputManager&) = delete;
    InputManager operator=(InputManager&&) = delete;

    // Returns false when the input device manager is unavailable.
    static bool install();
};

PLUGIN_NAMESPACE_END
