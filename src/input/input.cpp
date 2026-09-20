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

        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events,
            RE::BSTEventSource<RE::InputEvent*>*) noexcept override
        {
            try
            {
                if (!a_events)
                    return RE::BSEventNotifyControl::kContinue;
                auto* ui = RE::UI::GetSingleton();
                if (!ui || ui->GameIsPaused())
                    return RE::BSEventNotifyControl::kContinue;
                auto const config = Setting::instance().get_config();
                if (config.hotkey == 0)
                    return RE::BSEventNotifyControl::kContinue;
                auto const scan_code = MapVirtualKeyA(config.hotkey, MAPVK_VK_TO_VSC);
                for (auto* event = *a_events; event; event = event->next)
                {
                    auto* button = event->AsButtonEvent();
                    if (button && button->device.get() == RE::INPUT_DEVICE::kKeyboard &&
                        button->IsDown() && scan_code != 0 && button->idCode == scan_code)
                        Setting::instance().toggle();
                }
            }
            catch (...)
            {
                try { logger::error("Input event failed"); } catch (...) {}
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
    auto* source = RE::BSInputDeviceManager::GetSingleton();
    if (!source)
        return;
    source->AddEventSink(&InputHandler::instance());
    s_installed = true;
    logger::info("Input handler added");
}

PLUGIN_NAMESPACE_END
