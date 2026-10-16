//
// Created by AmazingBuff on 2026/9/19.
//

#include "renderer.h"
#include "render_hook.h"
#include "shader_manager.h"

#include "pass/composite.h"
#include "studio/light.h"

#include "panel/panel.h"
#include "character/character_manager.h"
#include "character/equipment_mirror.h"
#include "character/final_pose_apply.h"
#include "character/final_pose_probe.h"
#include "character/puppet_draw.h"
#include "character/puppet_driver.h"
#include "character/scene_graph_copy.h"
#include "oar/hooks.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    class PassMerger
    {
    public:
        static PassMerger& instance()
        {
            static PassMerger s_instance;
            return s_instance;
        }

        void on_post_ui_draw()
        {
            RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();
            if (!renderer)
                return;

            const RE::BSGraphics::RendererData& rt = renderer->GetRuntimeData();
            REX::W32::ID3D11Device* device = rt.forwarder;
            REX::W32::ID3D11DeviceContext* context = rt.context;
            if (!device || !context)
                return;

            REX::W32::ID3D11RenderTargetView* const output_target = rt.renderTargets[RE::RENDER_TARGETS::kFRAMEBUFFER].RTV;
            uint32_t width = 0;
            uint32_t height = 0;
            if (!render_target_dimensions(output_target, width, height))
                return;

            RE::BSGraphics::State* bs_state = RE::BSGraphics::State::GetSingleton();
            uint32_t const frame = bs_state ? bs_state->GetFrameCount() : 0;

            if (frame == 0 || frame == m_last_drawn_frame)
                return;

            m_last_drawn_frame = frame;
            draw(device, context, output_target, width, height);
        }

    private:
        void draw(REX::W32::ID3D11Device* device, REX::W32::ID3D11DeviceContext* context,
            REX::W32::ID3D11RenderTargetView* output_target, uint32_t width, uint32_t height)
        {
            if (!init(device, width, height))
                return;

            if (const RE::UI3DSceneManager* ui3d = RE::UI3DSceneManager::GetSingleton())
            {
                RE::UI* ui = RE::UI::GetSingleton();
                const bool paused = !ui || ui->GameIsPaused();
#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
                SceneGraphCopy::instance().process_requests();
#endif

#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
                // AFP1: A0 is taken before anything advances this frame, so the report after the
                // drivers ticked can say whether our own tick is what moved the output (R02).
                FinalPoseProbe::instance().sample_before_tick();
                // R01 (AFP9): the studio placement must be in place BEFORE the puppet's graph update,
                // because the skin state this frame's draw consumes is computed during that update -
                // a transform applied after it reaches rigid attachments only (AFP9e measured the axe
                // following and the skinned body not). end_frame() below restores what it borrowed.
                PuppetDraw::instance().begin_frame();
                // The scheduler's data source: lifecycle pump, and while the panel pauses the
                // world this frame also drives the puppet's graph on the studio clock.
                PuppetDriver::instance().tick(paused);
                // AFP2: the application stage. It verifies the mapping while the world runs (the
                // engine's own applied locals are the reference then) and writes while it is paused.
                FinalPoseApply::instance().tick(paused);
                // AFP1: the mark is consumed after the drivers ticked, so the sample describes the
                // frame the panel is about to draw rather than the one before it.
                FinalPoseProbe::instance().tick(paused);
#endif

                // Host audit: menuObjects[0] is engine-managed and rebuilt
                // across menu/load transitions (runs 100-102). Nothing hangs
                // under it anymore — the clone graph and the light rig are
                // both detached — so this only maps the rebuild triggers.
                static RE::NiNode* s_last_host = nullptr;
                RE::NiNode* const host = ui3d->menuObjects[0].get();
                if (host != s_last_host)
                {
                    logger::info("Studio host changed: {} -> {} (paused={})", static_cast<void*>(s_last_host), static_cast<void*>(host), paused);
                    s_last_host = host;
                }

                const bool lights_registered = StudioLight::instance().init(RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0]);
                static std::uint32_t s_warn_tick = 0;
                if ((!lights_registered || !StudioLight::instance().refresh()) && ++s_warn_tick % 120 == 1)
                    logger::warn("Studio lights not fully served by the ledger yet");

#if !CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
                if (!paused)
                {
                    CharacterManager::instance().create_clones({RE::PlayerCharacter::GetSingleton()});

                    // Advance the assembly gate on every unpaused frame: the
                    // detach runs in this render bracket (serialized with the
                    // world renderer job), so the figure is ready long before
                    // the first panel open.
                    if (const std::shared_ptr<CharacterClone> clone = CharacterManager::instance().get_clone(RE::PlayerCharacter::GetSingleton()))
                        clone->detach_graph();
                }

#endif
                const bool panel_open = PanelMonitor::instance().is_menu_open();
                if (panel_open)
                {
                    bool image_ready = false;
#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
                    // R01 (AFP9): when enabled, the panel draws the puppet's OWN 3D instead of the
                    // detached copy. The puppet path returns false (and says why) whenever it cannot
                    // draw, and the copy below then serves exactly as before.
                    //
                    // The figure draw lives in ITS OWN state-capture scope, which ends before the
                    // composite below: the capture restores the engine's render target at that point,
                    // so the studio texture is no longer bound as an output when the composite binds
                    // it as a sampler source (D3D11 silently unbinds a resource used as both - the
                    // AFP9 black-panel bug).
                    {
                        D3D11StateCapture capture(context);
                        if (!PuppetDraw::instance().draw(*ui3d, *m_common_states, m_render_target))
                        {
                            if (CharacterClone* clone = SceneGraphCopy::instance().drawable())
                            {
                                // The S2 probe stages run inside draw(), between pose() and pass
                                // generation (PRD 0.6 §6.3); calling it here would run before pose()
                                // and be overwritten.
                                image_ready = clone->draw(ui3d, *m_common_states, m_render_target);
                            }
                        }
                        else
                        {
                            image_ready = true;
                        }
                    }
#else
                    // Draw even while the lights are still converging — the
                    // per-pass injection engages only once every slot is
                    // ledger-served (submit_pass checks the slots itself).
                    if (const std::shared_ptr<CharacterClone> clone = CharacterManager::instance().get_clone(RE::PlayerCharacter::GetSingleton()))
                    {
                        D3D11StateCapture capture(context);
                        image_ready = clone->draw(ui3d, *m_common_states, m_render_target);
                    }
#endif

                    if (image_ready)
                    {
                        D3D11StateCapture composite_capture(context);

                        REX::W32::D3D11_VIEWPORT viewport{
                            .topLeftX = 0.0f,
                            .topLeftY = 0.0f,
                            .width = static_cast<float>(width),
                            .height = static_cast<float>(height),
                            .minDepth = 0.0f,
                            .maxDepth = 1.0f
                        };

                        context->RSSetViewports(1, &viewport);
                        m_composite_pass.draw(context, output_target, *m_common_states, m_render_target);
                    }
                }

#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
                // R01 (AFP9): give the puppet's tree back exactly as begin_frame() borrowed it, on
                // every path - drawn, fell back to the copy, or no panel frame at all.
                PuppetDraw::instance().end_frame();
#endif
            }
        }

        bool init(REX::W32::ID3D11Device* device, uint32_t width, uint32_t height)
        {
            if (m_ready)
                return true;

            // All HLSL passes are compiled once, before any overlay picks them up.
            if (!ShaderManager::instance().compile())
                return false;

            // CommonStates is the local REX::W32-typed mirror; it takes the REX device pointer directly.
            m_common_states = std::make_unique<CommonStates>(device);
            if (!m_common_states || !m_common_states->valid() || !m_render_target.init(device, width, height) || !m_composite_pass.init(device))
                return false;

            m_ready = true;
            return m_ready;
        }
    private:
        PassMerger() : m_render_target(REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM, REX::W32::DXGI_FORMAT_D32_FLOAT), m_last_drawn_frame(0), m_ready(false) {}

    private:
        RenderTarget m_render_target;
        std::unique_ptr<CommonStates> m_common_states;
        CompositePass m_composite_pass;

        uint32_t m_last_drawn_frame;
        bool m_ready;
    };

    void post_ui_draw(int64_t)
    {
        PassMerger::instance().on_post_ui_draw();
    }
}

void Renderer::install()
{
    RenderHook::instance().install(post_ui_draw);
}

PLUGIN_NAMESPACE_END
