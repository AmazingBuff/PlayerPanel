#include "input.h"
#include "config/config.h"
#include "preview/preview_actor.h"
#include "render/panel/panel_renderer.h"
#include <Windows.h>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // Left mouse button identifier in the engine's mouse ButtonEvent.
    constexpr uint32_t Mouse_Left_Button_Id = 0;

    // Forwards the one mouse channel the panel drag needs. The cursor position is not derived from a
    // mouse delta any more: the panel reads the cursor the engine drives for its own menu, so only the
    // button edge has to be observed here. Nothing else on the panel consumes mouse input, so the event
    // is observed and never consumed.
    void observe_mouse_event(RE::InputEvent* event)
    {
        RE::ButtonEvent* const button = event->AsButtonEvent();
        if (!button || button->device.get() != RE::INPUT_DEVICE::kMouse || button->GetIDCode() != Mouse_Left_Button_Id)
            return;

        PanelRenderer::on_left_button(button->IsPressed());
    }

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
            try
            {
                if (!events)
                    return RE::BSEventNotifyControl::kContinue;

                // The panel observes the mouse before the gates below: the button state it tracks must
                // stay correct while the game is paused or the master switch is off, because a release
                // that were dropped would leave the panel believing a drag was still running.
                bool hotkey_enabled = false;
                uint32_t scan_code = 0;
                RE::UI* const ui = RE::UI::GetSingleton();
                if (ui && !ui->GameIsPaused())
                {
                    auto const config = Setting::instance().get_config();
                    hotkey_enabled = config.enabled && config.hotkey != 0;
                    scan_code = hotkey_enabled ? MapVirtualKeyA(config.hotkey, MAPVK_VK_TO_VSC) : 0u;
                }

                for (RE::InputEvent* event = *events; event; event = event->next)
                {
                    observe_mouse_event(event);
                    if (!hotkey_enabled || scan_code == 0)
                        continue;

                    auto* button = event->AsButtonEvent();
                    if (button && button->device.get() == RE::INPUT_DEVICE::kKeyboard &&
                        button->IsDown() && button->idCode == scan_code)
                        PreviewActor::instance().request_toggle();
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
