//
// Experimental scene-copy commands.
//

#include "input/input.h"

#include "character/final_pose_apply.h"
#include "character/final_pose_probe.h"
#include "character/puppet_driver.h"
#include "character/puppet_probe.h"
#include "character/scene_graph_copy.h"

#include <Windows.h>

PLUGIN_NAMESPACE_BEGIN

namespace
{
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
                // The AFP route's controls, on the three keys this setup can spare: F2/F3/F4. The
                // scene-copy route's rotate, release and idle toggle gave those keys up for them (F7
                // still re-captures, which releases the previous copy), and the earlier choices were
                // wrong for their own reasons - F5 is the game's quicksave, and this sink does not
                // consume input, while F11/F12 and NUMPAD1 were tried and are not wanted either.
                //
                //   F2  mark: one read-only forensic sample (A0/A1, the calibration, the camera and
                //       geometry lines). Changes nothing.
                //   F3  drive: off -> tick -> off. "off" is freeze-all (no graph tick, no pose);
                //       "tick" advances the puppet's graph on the panel clock. When the application
                //       stage lands this becomes off -> tick -> tick+apply, which is R04's A/B pair
                //       and R02's push/freeze control on one key.
                //   F4  placement: parked-deep <-> held-pinned. AFP1 measured the parked puppet's
                //       output pose as empty (track live, zero elements, numAnimBones 0) while the
                //       player's carried 116, so this is the one variable that isolates why: out of
                //       the engine's animation scope versus standing at the player, fade pinned.
                else if (scan == MapVirtualKeyA(VK_F2, MAPVK_VK_TO_VSC))
                    FinalPoseProbe::instance().request_mark();
                else if (scan == MapVirtualKeyA(VK_F3, MAPVK_VK_TO_VSC))
                    FinalPoseApply::instance().cycle();
                else if (scan == MapVirtualKeyA(VK_F4, MAPVK_VK_TO_VSC))
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
