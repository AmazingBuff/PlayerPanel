//
// Created by AmazingBuff on 2026/09/21.
//

#pragma once

#include "config/config.h"
#include "render/panel/panel_geometry.h"

#include <DirectXMath.h>

PLUGIN_NAMESPACE_BEGIN

// One frame of panel camera state: the row-major view-projection matrix (column-vector convention,
// multiplied on the left of the point) and the world-space eye the pixel shader lights from.
struct PanelCameraFrame
{
    DirectX::XMFLOAT4X4 view_proj;
    DirectX::XMFLOAT3 eye;
};

// Where the camera aims and how wide it frames, derived from the collected geometry: the reference's
// cached position drifts from its actual 3D (measured: 262 units after placement), so the camera aims
// at the body's bound bones, not at the reference.
struct PanelAim
{
    DirectX::XMFLOAT3 target;
    float radius;
};

// The panel camera: a forward-Z perspective camera with its own near/far range that looks at the
// preview from the front and frames the whole body automatically. Stateless: build() recomputes a
// frame from the collected geometry and the configuration on every call.
class PanelCamera
{
public:
    PanelCamera() = delete;
    ~PanelCamera() = delete;

    // Derives the aim from the collected draws: the first skinned draw's bound bones span the body
    // (their world translates give the true vertical span and position), falling back to the first
    // static draw's transform. Returns false when the collection offers no geometry to aim at.
    [[nodiscard]] static bool resolve_body_aim(std::span<PanelDraw const> draws, PanelAim& out);

    // Builds the camera from the aim and heading. Framing is automatic unless CameraDistance is
    // positive: the framing radius is fitted on the vertical axis of the panel's own aspect, which is
    // what makes the character fill the portrait window. Returns false only when no finite camera can
    // be derived.
    [[nodiscard]] static bool build(RE::TESObjectREFR const& preview, PanelAim const& aim, Config const& config, PanelCameraFrame& out);
};

PLUGIN_NAMESPACE_END
