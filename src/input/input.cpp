//
// Experimental scene-copy commands.
//

#include "input/input.h"

#include "character/final_pose_probe.h"
#include "character/puppet_driver.h"
#include "character/puppet_probe.h"
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
                // AFP1 (PRD 0.9 section 6.6) controls, on the keys the earlier rounds left free: F11
                // marks the final-output sample and F12 freezes or resumes the panel's manual graph
                // tick. F5 was the first choice and is wrong - the game's own quicksave owns it, and
                // this sink does not consume input. Both controls are read-only with respect to the
                // Actor, its graph and the source.
                else if (scan == MapVirtualKeyA(VK_F11, MAPVK_VK_TO_VSC))
                    FinalPoseProbe::instance().request_mark();
                else if (scan == MapVirtualKeyA(VK_F12, MAPVK_VK_TO_VSC))
                    PuppetDriver::instance().toggle_tick();
                // Numpad 1: cycle the puppet's placement. AFP1 measured the parked puppet's output
                // pose as empty (track live, zero elements, numAnimBones 0), and this is the one
                // variable that isolates why - parked out of the engine's animation scope versus
                // standing at the player with the fade pinned.
                else if (scan == MapVirtualKeyA(VK_NUMPAD1, MAPVK_VK_TO_VSC))
                    PuppetDriver::instance().cycle_placement();
#if CHARACTER_PANEL_S2_PROBE
                // Diagnostic build only: F6 is taken by another plugin in the usual setup, so the
                // probe lives on an opt-in build rather than on the product's own key. The HKX37
                // round repurposed the key for the puppet stage machine; the retired S2 swing
                // probe keeps its command enum value but has no binding.
                else if (scan == MapVirtualKeyA(VK_F6, MAPVK_VK_TO_VSC))
                    PuppetProbe::instance().request_advance();
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
