//
// Created by AmazingBuff on 2026/09/21.
//

#pragma once

#include "config/config.h"

#include <DirectXMath.h>

PLUGIN_NAMESPACE_BEGIN

// One frame of panel camera state: the row-major view-projection matrix (column-vector convention,
// multiplied on the left of the point) and the world-space eye the pixel shader lights from.
struct PanelCameraFrame
{
    DirectX::XMFLOAT4X4 view_proj;
    DirectX::XMFLOAT3 eye;
};

// The panel camera: a forward-Z perspective camera with its own near/far range that looks at the
// preview from the front and frames the whole body automatically. Stateless: build() recomputes a
// frame from the preview and the configuration on every call.
class PanelCamera
{
public:
    PanelCamera() = delete;
    ~PanelCamera() = delete;

    // Builds the camera from the preview's world bound and heading. Framing is automatic unless
    // CameraDistance is positive: the body sphere is fitted on both axes of the panel's own aspect and
    // the larger of the two distances is used, so the body is never clipped at the sides when
    // PanelAspect is below one. When the world bound is unusable a fixed body-height framing is used
    // instead. Returns false only when no finite camera can be derived.
    [[nodiscard]] static bool build(RE::TESObjectREFR const& preview, Config const& config, PanelCameraFrame& out);
};

PLUGIN_NAMESPACE_END
