#include "input.h"

#include "config/config.h"
#include "spike/clone_actor.h"
#include "spike/pass_hook.h"

#include <Windows.h>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // SPIKE CODE - throwaway hotkeys for the Stage-0 gate:
    //   F7 = toggle the clone actor
    //   F8 = arm the one-shot pass calibration (dump a replay TGA)
    constexpr uint32_t SpikeCloneHotkey = 0x76;   // F7
    constexpr uint32_t SpikeCalibrateHotkey = 0x77;  // F8

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
            for (RE::InputEvent* event = *events; event; event = event->next)
            {
                RE::ButtonEvent* button = event->AsButtonEvent();
                if (!button || button->device.get() != RE::INPUT_DEVICE::kKeyboard || !button->IsDown())
                    continue;
                UINT const scan_code = MapVirtualKeyA(SpikeCloneHotkey, MAPVK_VK_TO_VSC);
                UINT const calibrate_scan = MapVirtualKeyA(SpikeCalibrateHotkey, MAPVK_VK_TO_VSC);
                if (scan_code != 0 && button->idCode == scan_code)
                {
                    if (spike::CloneActor::instance().has_actor())
                        spike::CloneActor::instance().request_despawn();
                    else
                        spike::CloneActor::instance().request_spawn();
                }
                else if (calibrate_scan != 0 && button->idCode == calibrate_scan)
                {
                    spike::PassHook::instance().arm_calibration();
                }
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
