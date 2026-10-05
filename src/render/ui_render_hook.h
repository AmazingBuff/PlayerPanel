#pragma once

PLUGIN_NAMESPACE_BEGIN

// Installs the three RenderPassImmediately call-site hooks and the
// DrawInterfaceStart detour, in that order (the pass hooks first — the
// E8 rel32 pre-patch targets must be parsed before anything else touches
// the call sites; the CS interposer detection logs there). False = an
// install failed; the specifics are in the log and the panel stays off.
bool install_ui_render_hooks();

PLUGIN_NAMESPACE_END
