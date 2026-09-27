#pragma once
// SPIKE CODE - throwaway calibration module for the Stage-0 gate.
//
// Hooks the three RenderPassImmediately call sites (the same ones Community
// Shaders' LightLimitFix hooks). When a pass for one of the clone actor's
// geometries flows through, it:
//   1. verifies the function-identity question (is the call target
//      BSBatchRenderer::SetupAndDrawPass, RELOCATION_ID(100854, 107644)?),
//   2. records the technique / passEnum / light-array shape the engine used,
//   3. once, saves render-target state, binds a private offscreen target,
//      replays the pass through the original function, restores state and
//      dumps the target to a TGA file.
//
// All of this runs on the engine's render thread inside the hooked call.

#include <RE/Skyrim.h>

#include <atomic>
#include <cstdint>

PLUGIN_NAMESPACE_BEGIN

namespace spike
{
    class PassHook
    {
    public:
        static PassHook& instance();

        bool install();

        // Game thread; arms one-shot calibration. The armed flag is cleared
        // from the render thread after the first successful replay+dump.
        void arm_calibration();

        struct CalibrationResult
        {
            std::atomic<bool> identity_verified{ false };
            std::atomic<std::uint32_t> observed_passes{ 0 };
            std::atomic<std::uint32_t> replays_done{ 0 };
            std::atomic<std::uint32_t> replays_failed{ 0 };
        };

        [[nodiscard]] const CalibrationResult& result() const { return m_result; }

        // Addresses of the original functions as returned by the trampoline,
        // filled at install time; index matches the call-site index.
        std::uintptr_t m_originals[3]{ 0, 0, 0 };

    private:
        PassHook() = default;

        static void thunk_rendezvous1(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
            std::uint32_t render_flags);
        static void thunk_rendezvous2(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
            std::uint32_t render_flags);
        static void thunk_rendezvous3(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
            std::uint32_t render_flags);

        void on_pass(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test, std::uint32_t render_flags,
            std::size_t site_index);

        bool do_replay(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test, std::uint32_t render_flags,
            std::size_t site_index);

        std::atomic<bool> m_installed{ false };
        std::atomic<bool> m_armed{ false };
        std::atomic<bool> m_identity_logged{ false };
        CalibrationResult m_result{};
    };
}

PLUGIN_NAMESPACE_END
