//
// Experimental scene-copy commands.
//

#include "input/input.h"

#include "character/scene_graph_copy.h"

#include <Windows.h>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // Rotate and release are quiet commands, so a press that never reaches them is
    // otherwise indistinguishable from a dropped key; log the first few presses only.
    uint32_t Rotate_Key_Log_Remaining = 5;

    class InputHandler final : public RE::BSTEventSink<RE::InputEvent*>
    {
    public:
        static InputHandler& instance()
        {
            static InputHandler s_instance;
            return s_instance;
        }

        RE::BSEventNotifyControl ProcessEvent(
            RE::InputEvent* const* events,
            RE::BSTEventSource<RE::InputEvent*>*) noexcept override
        {
            if (!events)
                return RE::BSEventNotifyControl::kContinue;

#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
            for (RE::InputEvent* event = *events; event; event = event->next)
            {
                RE::ButtonEvent* button = event->AsButtonEvent();
                if (!button || button->device.get() != RE::INPUT_DEVICE::kKeyboard || !button->IsDown())
                    continue;
                using Command = SceneGraphCopy::Command;
                const uint32_t scan = button->GetIDCode();
                if (scan == MapVirtualKeyA(VK_F7, MAPVK_VK_TO_VSC))
                    SceneGraphCopy::instance().request(Command::e_capture);
                else if (scan == MapVirtualKeyA(VK_F8, MAPVK_VK_TO_VSC))
                    SceneGraphCopy::instance().request(Command::e_toggle_draw);
                else if (scan == MapVirtualKeyA(VK_F3, MAPVK_VK_TO_VSC))
                {
                    if (Rotate_Key_Log_Remaining != 0)
                    {
                        logger::info("SCOPY KEY rotate pressed (VK_F3 -> scan {})", scan);
                        --Rotate_Key_Log_Remaining;
                    }
                    SceneGraphCopy::instance().request(Command::e_rotate);
                }
                else if (scan == MapVirtualKeyA(VK_F4, MAPVK_VK_TO_VSC))
                    SceneGraphCopy::instance().request(Command::e_release);
                else if (scan == MapVirtualKeyA(VK_F2, MAPVK_VK_TO_VSC))
                    SceneGraphCopy::instance().request(Command::e_idle_toggle);
#if CHARACTER_PANEL_S2_PROBE
                // Diagnostic build only: F6 is taken by another plugin in the usual setup, so the
                // retired probe lives on an opt-in build rather than on the product's own key.
                else if (scan == MapVirtualKeyA(VK_F6, MAPVK_VK_TO_VSC))
                    SceneGraphCopy::instance().request(Command::e_anim_probe);
#endif
            }
#endif

            return RE::BSEventNotifyControl::kContinue;
        }

    private:
        InputHandler() = default;
    };
}

bool InputManager::install()
{
    RE::BSInputDeviceManager* source = RE::BSInputDeviceManager::GetSingleton();
    if (!source)
    {
        logger::warn("Input device manager unavailable; panel hotkeys are disabled");
        return false;
    }

    source->AddEventSink(&InputHandler::instance());
    return true;
}

PLUGIN_NAMESPACE_END
