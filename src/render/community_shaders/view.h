#pragma once

#include <memory>

PLUGIN_NAMESPACE_BEGIN
struct FigurePassRequest;

namespace community_shaders
{
    // Does nothing outside a verified CS build. Called after all SKSE plugins have loaded.
    [[nodiscard]] bool install();

    // One isolated view. Native CS material hooks stay active; only this render thread's draw
    // window redirects their resources. The caller supplies the outer D3D11StateCapture.
    class View
    {
    public:
        explicit View(FigurePassRequest const& request);
        ~View();
        View(View const&) = delete;
        View& operator=(View const&) = delete;

        [[nodiscard]] bool active() const;
        void begin_deferred();
        // False means a native deferred permutation is still compiling. Redraw forward this frame.
        [[nodiscard]] bool resolve();
        void begin_forward(bool alpha_blend);
        void before_pass();
        void after_pass();

    private:
        struct State;
        std::unique_ptr<State> m_state;
    };
}
PLUGIN_NAMESPACE_END
