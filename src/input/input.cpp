//
// Experimental scene-copy commands.
//

#include "input/input.h"

#include "character/final_pose_apply.h"
#include "character/final_pose_probe.h"
#include "character/puppet_draw.h"
#include "character/puppet_driver.h"
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

                // F3 conflicts with another mod; AFP mark and drive use F2 and F4.
                else if (scan == MapVirtualKeyA(VK_F2, MAPVK_VK_TO_VSC))
                    FinalPoseProbe::instance().request_mark();
                else if (scan == MapVirtualKeyA(VK_F4, MAPVK_VK_TO_VSC))
                    FinalPoseApply::instance().cycle();

                // AFP9: R01 direct draw of the puppet's own 3D (F6 is unbound elsewhere).
                else if (scan == MapVirtualKeyA(VK_F6, MAPVK_VK_TO_VSC))
                    PuppetDraw::instance().toggle();

                // F11: the OAR projection's on/off arm (F1/F5/F9/F10/F12 belong to other mods).
                else if (scan == MapVirtualKeyA(VK_F11, MAPVK_VK_TO_VSC))
                {
                    PuppetDriver& driver = PuppetDriver::instance();
                    driver.set_projection(!driver.projection());
                }
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
