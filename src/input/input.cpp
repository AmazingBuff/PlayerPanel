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
                // The AFP route's controls. F3 is bound by another mod in this load order, and
                // pressing it pauses the world - exactly the state the drive's verification needs
                // NOT to be in (AFP7's session waited for a verify that could never run), so the
                // key is left to that mod and the drive lives on F4. The placement cycle that used
                // to own F4 is unbound for now: the park was measured not to matter (AFP3b -
                // parked-deep still generated 98 elements with the world running), so the cycle had
                // no remaining question to answer.
                //
                //   F2  mark: one read-only forensic sample (A0/A1, the calibration, the camera and
                //       geometry lines). Changes nothing.
                //   F4  drive: off -> tick -> tick+apply -> tick+apply+force -> step -> off. "off"
                //       is freeze-all (no graph tick, no pose). "tick" advances the puppet's graph
                //       on the panel clock. "tick+apply" also writes the graph's final output pose
                //       into the puppet's skeleton. "tick+apply+force" additionally asks for
                //       hkbGenerator::generate() on every frame, which is what makes the paused
                //       graph produce a pose at all. "step" is a one-shot declared dt.
                else if (scan == MapVirtualKeyA(VK_F2, MAPVK_VK_TO_VSC))
                    FinalPoseProbe::instance().request_mark();
                else if (scan == MapVirtualKeyA(VK_F4, MAPVK_VK_TO_VSC))
                    FinalPoseApply::instance().cycle();
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
