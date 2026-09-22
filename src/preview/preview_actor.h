//
// Created by AmazingBuff on 2026/09/21.
//

#pragma once

#include <cstdint>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN

// One non-persistent preview actor that mirrors the player's supported appearance and worn
// equipment. Every member function runs on the game thread: request_toggle() is called from the
// input sink and the engine work is serviced by on_frame() on the frame hook's game-thread task.
// No engine pointer survives a frame: the placed reference is held as an ObjectRefHandle and is
// re-resolved before use, while m_ref_base is the duplicated base record the preview borrows.
class PreviewActor
{
public:
    enum class State : std::uint8_t
    {
        kIdle = 0,
        kReady,
        kUnavailable
    };

    static PreviewActor& instance();
    PreviewActor(PreviewActor const&) = delete;
    PreviewActor& operator=(PreviewActor const&) = delete;

    // Records a show/hide request; the master switch is applied by the caller before this call.
    void request_toggle();
    // Frame hook listener; static so its address is a plain function pointer on the game thread.
    static void on_frame();
    // Disables and deletes the preview reference and clears every retained field.
    void destroy();
    [[nodiscard]] State state() const;
    // The live preview reference, or nullptr when no preview is ready. Borrowed for immediate use;
    // the caller must not retain it across frames.
    [[nodiscard]] RE::TESObjectREFR* current_reference() const;

private:
    PreviewActor();
    ~PreviewActor() = default;

    void service_request();
    bool create();
    void set_state(State state, std::string_view reason);

private:
    RE::ObjectRefHandle m_handle;
    RE::TESNPC* m_ref_base;
    State m_state;
    bool m_toggle_requested;
};

PLUGIN_NAMESPACE_END
