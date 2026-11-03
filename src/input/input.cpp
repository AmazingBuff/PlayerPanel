//
// SceneGraph preview commands.
//

#include "input/input.h"

#include "character/character_manager.h"
#include "panel/panel_monitor.h"
#include "animation/behavior_graph/probe_liveness.h"
#include "animation/behavior_graph/source_event_capture.h"
#include "animation/behavior_graph/source_write_capture.h"
#include "animation/behavior_graph/source_timeline.h"

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

            for (RE::InputEvent* event = *events; event; event = event->next)
            {
                RE::ButtonEvent* button = event->AsButtonEvent();
                if (!button || button->device.get() != RE::INPUT_DEVICE::kKeyboard || !button->IsDown())
                    continue;
                const uint32_t scan = button->GetIDCode();
                const auto graph = CharacterManager::instance().get_scene_graph(RE::PlayerCharacter::GetSingleton());
                // A scene graph object exists outside the pause menu too, so the panel-open state
                // is the discriminator, not the graph pointer. F7 recaptures only in the panel;
                // elsewhere it opens the G3-A timeline window.
                if (!PanelMonitor::instance().is_menu_open())
                {
                    if (scan == MapVirtualKeyA(VK_F7, MAPVK_VK_TO_VSC))
                    {
                        // One key, whichever recorder is enabled: the timeline when its key is
                        // on, otherwise the E1 event capture. The timeline wins when both are
                        // on because then the capture hook is never installed.
                        animation::behavior_graph::arm_source_timeline();
                        animation::behavior_graph::arm_source_event_capture();
                        animation::behavior_graph::arm_source_write_capture();
                        animation::behavior_graph::arm_probe_liveness();
                    }
                    continue;
                }
                if (!graph) continue;
                using Command = SceneGraph::Command;
                if (scan == MapVirtualKeyA(VK_F7, MAPVK_VK_TO_VSC))
                    graph->request(Command::e_capture);
                else if (scan == MapVirtualKeyA(VK_F8, MAPVK_VK_TO_VSC))
                    graph->request(Command::e_toggle_draw);
            }

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
