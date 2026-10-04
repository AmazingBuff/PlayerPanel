//
// Hook installation TU: the DrawInterfaceStart Detours detour (the frame
// bracket driver) and the Proto::install_hook / install_pass_hooks entry
// points. (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)
//

#include "panel.h"
#include "pinstance/pinstance.h"
#include "render/pass_redirector.h"
#include "render/render_internal.h"

#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <Windows.h>
#include <detours/detours.h>
#include <fmt/format.h>

using namespace std::literals;
namespace logger = SKSE::log;

namespace CharacterPanelProto
{
    namespace
    {
        // Set by install_hook() before any menu frame can run the thunk;
        // Detours relocates the overwritten prologue into its own trampoline,
        // so calling this runs the true original function.
        DrawInterfaceStart_t s_original_draw_interface_start = nullptr;

        // Render-thread detour. DrawInterfaceStart runs once per rendered
        // frame (the HUD is a menu too); the panel state decides whether
        // this frame is bracketed for studio redirection.
        void draw_interface_start_thunk(std::int64_t a1)
        {
            DrawInterfaceStart_t const original = s_original_draw_interface_start;
            if (!original)
            {
                // Detour not fully installed; bail out without recursing.
                return;
            }

            // The P build state machine is paced by REAL rendered frames:
            // each DrawInterfaceStart queues at most one game-thread step
            // (run-28 finding — a self-rescheduling task drains within one
            // game frame and never lets the engine load anything).
            PInstance::instance().pump();

            if (!Proto::instance().panel_frame_active())
            {
                // Panel closed: run FR-06 cleanup here on the render thread
                // — a user close writes one evidence TGA first, a
                // force-close releases silently — then draw untouched. The
                // retired P graphs (stage 2) also drain here: this frame
                // provably runs no pass hooks, so no in-flight pass can
                // still read the detached scene graph.
                if (Proto::instance().take_release_pending())
                {
                    if (Proto::instance().take_dump_on_close())
                        PassRedirector::instance().dump_and_release("panel closed");
                    else
                        PassRedirector::instance().release_target("panel force-closed");
                }
                PInstance::instance().drain_retired();
                original(a1);
                return;
            }

            // Panel open: bracket the menu draw. The pass hooks replay menu
            // lighting passes into the persistent studio target while it is
            // open; the panel composite runs at end_frame into THIS frame's
            // composite instance (v4.2); the menu frame itself renders
            // normally.
            PassRedirector::instance().begin_frame();
            original(a1);
            PassRedirector::instance().end_frame();
        }
    }

    bool Proto::install_hook()
    {
        // Detours-based entry detour (same mechanism as Community Shaders'
        // stl::detour_thunk): the overwritten prologue bytes are relocated to
        // a trampoline, so the thunk can call the original function body. A
        // raw write_call<5> on the entry is NOT viable here — the entry is a
        // 5-byte jmp whose bytes would be lost and the thunk would recurse
        // into itself (the main-menu stack overflow seen in the first run).
        static DrawInterfaceStart_t original =
            reinterpret_cast<DrawInterfaceStart_t>(REL::RelocationID(79947, 82084).address());

        DetourRestoreAfterWith();
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        if (DetourAttach(reinterpret_cast<PVOID*>(&original),
                reinterpret_cast<PVOID>(draw_interface_start_thunk)) != NO_ERROR ||
            DetourTransactionCommit() != NO_ERROR)
        {
            DetourTransactionAbort();
            logger::warn("Proto DetourAttach failed; the panel has no effect");
            return false;
        }
        s_original_draw_interface_start = original;
        logger::info("Proto DrawInterfaceStart detour installed at 0x{:X}",
            REL::RelocationID(79947, 82084).address());
        return true;
    }

    bool Proto::install_pass_hooks()
    {
        return PassRedirector::instance().install();
    }
}
