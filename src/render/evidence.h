//
// Created by AmazingBuff on 2026/10/13.
//

#pragma once

#include "render/dx11/d3d11_util.h"

PLUGIN_NAMESPACE_BEGIN

// Synchronous GPU -> CPU copy of the studio target, written as an 8-bit BGRA TGA under the SKSE log
// directory (`.../SKSE/CharacterPanel/`). Restored from the pre-172984a evidence module, which the
// same project used for exactly this purpose: it answers "what is actually in the studio target"
// without a frame capture, and it is synchronous by design - never on a steady-state path, only on
// an explicit request while the panel is being diagnosed.
void dump_studio_target(RenderTarget const& a_target, char const* a_tag);

PLUGIN_NAMESPACE_END
