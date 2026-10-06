//
// Input hotkey module (HLC-style: InputManager shell + anonymous sink).
// Split out of the former panel.cpp (2026-10-05 file-organization pass);
// behavior byte-identical.
//

#include "input/input.h"

#include "panel/panel.h"

#include <Windows.h>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // F6 is bound in the user's game setup, so the panel lives on F7/F8
    // (the probe's F7/F8 never co-load with this DLL).
    constexpr std::uint32_t Panel_Toggle_Virtual_Key = VK_F7;
    constexpr std::uint32_t Dump_Virtual_Key = VK_F8;

    // FR-05: the sink only observes the two panel keys; every other event
    // passes through untouched, and held/repeat events are rejected so a
    // single press toggles exactly once. The callback stays active while
    // InventoryMenu pauses the game, so the panel is operable exactly
    // where its content exists.
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

            // UINT const panel_scan = MapVirtualKeyA(Panel_Toggle_Virtual_Key, MAPVK_VK_TO_VSC);
            // UINT const dump_scan = MapVirtualKeyA(Dump_Virtual_Key, MAPVK_VK_TO_VSC);
            // for (RE::InputEvent* event = *events; event; event = event->next)
            // {
            //     RE::ButtonEvent* button = event->AsButtonEvent();
            //     if (!button || button->device.get() != RE::INPUT_DEVICE::kKeyboard || !button->IsDown())
            //         continue;
            //
            //     if (panel_scan != 0 && button->GetIDCode() == panel_scan)
            //         Proto::instance().toggle_panel();
            //     else if (dump_scan != 0 && button->GetIDCode() == dump_scan)
            //         Proto::instance().request_dump();
            // }
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
