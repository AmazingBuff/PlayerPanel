//
// Created by AmazingBuff on 2026/09/21.
//

#include "panel_camera.h"

#include <algorithm>
#include <cmath>
#include <limits>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // The panel camera has its own forward-Z range: near maps to 0 and far to 1, so the engine's
    // reverse-Z (GREATER) depth state must never be reused and the panel pass pairs this projection
    // with its own LESS depth state.
    constexpr float Radians_Per_Degree = 0.017453292519943295f;

    // Automatic framing: fit the WHOLE body on the vertical axis of the panel's own aspect, head to
    // feet (the BG3-style full-body portrait). The bound of a standing humanoid is loose (measured
    // r=129 for a 128-unit body) and its centre drifts with the stale bound of a culled subtree, so
    // the camera targets the body's true vertical span - the skinned bones' min/max Z - instead of
    // any bound sphere. The fit distance takes the panel's tall aspect into account so the feet stay
    // in frame with a visible margin; CameraDistance still overrides the fit entirely.
    constexpr float Framing_Margin = 1.1f;
    constexpr float Min_Body_Framing_Radius = 48.0f;

    // Fallback framing when the engine's world bound is unusable: a humanoid is roughly this tall
    // (game units), so half of it is the framing radius.
    constexpr float Fallback_Body_Height = 128.0f;
    constexpr float Fallback_Distance_Factor = 3.0f;

    // Bounds on the world bound and on the resulting distance, so a corrupt bound cannot produce a
    // degenerate or absurd camera.
    constexpr float Min_Bound_Radius = 1.0f;
    constexpr float Max_Bound_Radius = 1024.0f;
    constexpr float Min_Camera_Distance = 8.0f;
    constexpr float Max_Camera_Distance = 4000.0f;

    // Near/far of the private range: the near plane stays just off the hands and the far plane keeps
    // the whole body inside the forward-Z range.
    constexpr float Min_Near_Plane = 1.0f;
    constexpr float Far_Radius_Factor = 4.0f;
    constexpr float Far_Slack = 64.0f;

    struct Framing
    {
        DirectX::XMFLOAT3 target;
        float radius;
    };
}

bool PanelCamera::resolve_body_aim(std::span<PanelDraw const> draws, PanelAim& out)
{
    // The first skinned draw's bound bones span the body: their world translates give the true
    // vertical span and the true position. The reference's cached position drifts from its 3D
    // (measured: 262 units after placement), so the camera aims at the geometry, never at the
    // reference. The FULL span, uncapped, is what puts the feet in frame.
    for (PanelDraw const& draw : draws)
    {
        if (!draw.skin)
            continue;

        RE::NiSkinInstance& skin = *draw.skin;
        if (!skin.bones || !skin.boneWorldTransforms || !skin.skinData || skin.skinData->GetBoneCount() == 0)
            continue;

        std::uint32_t const count = (std::min)(skin.skinData->GetBoneCount(), 64u);
        float min_z = std::numeric_limits<float>::max();
        float max_z = std::numeric_limits<float>::lowest();
        double sum_x = 0.0;
        double sum_y = 0.0;
        std::uint32_t counted = 0;
        for (std::uint32_t index = 0; index < count; ++index)
        {
            if (!skin.bones[index])
                continue;
            RE::NiPoint3 const& translate = skin.bones[index]->world.translate;
            min_z = (std::min)(min_z, translate.z);
            max_z = (std::max)(max_z, translate.z);
            sum_x += translate.x;
            sum_y += translate.y;
            ++counted;
        }
        if (counted == 0)
            continue;

        // The framing centre sits slightly below the skeleton's vertical middle: a centre-height
        // target reads bottom-heavy in a portrait window (the feet crop, the head floats with dead
        // space above). Biasing the target down pushes the figure up inside the window and evens
        // the margins around it.
        out.target = DirectX::XMFLOAT3{ static_cast<float>(sum_x / counted),
            static_cast<float>(sum_y / counted), min_z + (max_z - min_z) * 0.55f };
        // The bones' Z span covers the skeleton but not the crown of the head or the soles; a
        // humanoid's full height reads about a quarter higher than the bone span, so the framing
        // radius is the half-height with that headroom built in.
        out.radius = (std::max)((max_z - min_z) * 0.5f * 1.25f, Min_Body_Framing_Radius);
        return true;
    }

    // No skinned geometry (before the palette latch): aim at the first static draw's transform, a
    // body-height above its hip-level origin.
    for (PanelDraw const& draw : draws)
    {
        if (!draw.node)
            continue;
        RE::NiPoint3 const& translate = draw.node->world.translate;
        out.target = DirectX::XMFLOAT3{ translate.x, translate.y, translate.z + Fallback_Body_Height * 0.5f };
        out.radius = Fallback_Body_Height * 0.5f;
        return true;
    }
    return false;
}

bool PanelCamera::build(RE::TESObjectREFR const& preview, PanelAim const& aim, Config const& config, PanelCameraFrame& out)
{
    Framing const framing{ aim.target, aim.radius };
    if (!std::isfinite(framing.radius) || framing.radius <= 0.0f)
        return false;

    // Orbit direction: stand in front of the character along its own heading, so the panel shows the
    // face the preview was turned to show.
    float const heading = preview.GetAngleZ();
    if (!std::isfinite(heading))
        return false;
    DirectX::XMFLOAT3 const orbit{ std::sin(heading), std::cos(heading), 0.0f };

    float const fov_degrees = static_cast<float>(config.camera_fov);
    float const fov_y = fov_degrees * Radians_Per_Degree;
    if (!std::isfinite(fov_y) || fov_y <= 0.0f || fov_y >= 3.14159265358979323846f)
        return false;

    // The panel's own aspect, not the screen's: the camera frames the character the same way at every
    // monitor shape.
    float const aspect = static_cast<float>(config.panel_aspect);
    if (!std::isfinite(aspect) || aspect <= 0.0f)
        return false;

    float distance = static_cast<float>(config.camera_distance);
    if (!std::isfinite(distance) || distance <= 0.0f)
    {
        // The vertical fit against the panel's own tall aspect: the horizontal half-FOV is wider on
        // a portrait window, so fitting the radius on the vertical axis alone keeps the whole body
        // (head to feet) in frame instead of pushing the camera back for the empty side margins.
        float const horizontal_fit = framing.radius / std::tan(fov_y * 0.5f) / std::max(aspect, 0.01f);
        float const vertical_fit = framing.radius / std::sin(fov_y * 0.5f);
        distance = std::max(horizontal_fit, vertical_fit);
        distance *= Framing_Margin;
    }
    distance = std::clamp(distance, Min_Camera_Distance, Max_Camera_Distance);

    DirectX::XMFLOAT3 const eye{
        framing.target.x + orbit.x * distance,
        framing.target.y + orbit.y * distance,
        framing.target.z + orbit.z * distance
    };

    float const near_plane = std::max(distance - framing.radius * 2.0f, Min_Near_Plane);
    float const far_plane = distance + framing.radius * Far_Radius_Factor + Far_Slack;
    if (!(far_plane > near_plane))
        return false;

    // Skyrim's world is left-handed with +Z up, so a left-handed view matrix matches the engine's
    // own world-to-view orientation and the image is not mirrored.
    DirectX::XMMATRIX const view = DirectX::XMMatrixLookAtLH(
        DirectX::XMLoadFloat3(&eye),
        DirectX::XMLoadFloat3(&framing.target),
        DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
    DirectX::XMMATRIX const projection = DirectX::XMMatrixPerspectiveFovLH(fov_y, aspect, near_plane, far_plane);

    // DirectXMath composes and stores row-vector matrices - the translation of XMMatrixLookAtLH ends
    // up in the fourth row - while the panel shaders read row_major memory as mul(matrix, column
    // vector), which needs the translation in the fourth column. The transpose is the one conversion
    // between the two conventions, matching the column-vector layout the static and palette paths
    // already upload (see the shader's matrix-convention comment).
    DirectX::XMStoreFloat4x4(&out.view_proj,
        DirectX::XMMatrixTranspose(DirectX::XMMatrixMultiply(view, projection)));
    out.eye = eye;
    return true;
}

PLUGIN_NAMESPACE_END
