//
// SceneGraph preview commands.
//

#include "input/input.h"

#include "character/character_manager.h"
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
                if (!graph)
                {
                    // G3-A: with no panel open, F7 opens the bounded timeline window instead of
                    // recapturing. Inside the panel F7 keeps its recapture meaning below.
                    if (scan == MapVirtualKeyA(VK_F7, MAPVK_VK_TO_VSC))
                        animation::behavior_graph::arm_source_timeline();
                    continue;
                }
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
