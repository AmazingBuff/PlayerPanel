#include "input.h"

#include "config/config.h"

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

        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,
            RE::BSTEventSource<RE::InputEvent*>*) noexcept override
        {
            if (!events)
                return RE::BSEventNotifyControl::kContinue;
            RE::UI* ui = RE::UI::GetSingleton();
            if (!ui || ui->GameIsPaused())
                return RE::BSEventNotifyControl::kContinue;
            Config const config = Setting::instance().get_config();
            if (config.hotkey == 0)
                return RE::BSEventNotifyControl::kContinue;
            UINT const scan_code = MapVirtualKeyA(config.hotkey, MAPVK_VK_TO_VSC);
            for (RE::InputEvent* event = *events; event; event = event->next)
            {
                RE::ButtonEvent* button = event->AsButtonEvent();
                if (button && button->device.get() == RE::INPUT_DEVICE::kKeyboard &&
                    button->IsDown() && scan_code != 0 && button->idCode == scan_code)
                    Setting::instance().toggle();
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    private:
        InputHandler() = default;
    };
}

void InputManager::install()
{
    static bool s_installed = false;
    if (s_installed)
        return;
    RE::BSInputDeviceManager* source = RE::BSInputDeviceManager::GetSingleton();
    if (!source)
        return;
    source->AddEventSink(&InputHandler::instance());
    s_installed = true;
    logger::info("Input handler added");
}

PLUGIN_NAMESPACE_END
