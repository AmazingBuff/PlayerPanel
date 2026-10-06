#pragma once

PLUGIN_NAMESPACE_BEGIN

class RenderHook
{
public:
    using Callback = void (*)(int64_t);

    static RenderHook& instance();

    bool install(Callback callback);
private:
    RenderHook();
    ~RenderHook();

    using DrawInterfaceFunc = void (*)(int64_t);

    static void draw_interface_thunk(int64_t argument);

    DrawInterfaceFunc m_ref_original;
    Callback m_callback;
};

PLUGIN_NAMESPACE_END
