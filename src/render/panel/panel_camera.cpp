//
// Created by AmazingBuff on 2026/09/21.
//

#include "panel_camera.h"

#include <algorithm>
#include <cmath>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // The panel camera has its own forward-Z range: near maps to 0 and far to 1, so the engine's
    // reverse-Z (GREATER) depth state must never be reused and the panel pass pairs this projection
    // with its own LESS depth state.
    constexpr float Radians_Per_Degree = 0.017453292519943295f;

    // Automatic framing: fit the body sphere into both axes of the panel's own aspect with a small
    // margin, so a portrait panel does not clip the arms or a weapon at the sides.
    constexpr float Framing_Margin = 1.15f;

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

    // The look-at point and framing radius: the preview's world bound when it is finite and inside
    // the accepted range, otherwise a fixed body-height framing derived from the reference position.
    Framing resolve_framing(RE::TESObjectREFR const& preview)
    {
        if (RE::NiAVObject const* const root = preview.GetCurrent3D())
        {
            RE::NiBound const& bound = root->worldBound;
            if (std::isfinite(bound.center.x) && std::isfinite(bound.center.y) && std::isfinite(bound.center.z) &&
                std::isfinite(bound.radius) && bound.radius >= Min_Bound_Radius && bound.radius <= Max_Bound_Radius)
            {
                return Framing{ DirectX::XMFLOAT3{ bound.center.x, bound.center.y, bound.center.z }, bound.radius };
            }
        }

        RE::NiPoint3 const position = preview.GetPosition();
        float const half_height = Fallback_Body_Height * 0.5f;
        return Framing{ DirectX::XMFLOAT3{ position.x, position.y, position.z + half_height }, half_height };
    }
}

bool PanelCamera::build(RE::TESObjectREFR const& preview, Config const& config, PanelCameraFrame& out)
{
    Framing const framing = resolve_framing(preview);
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
        // Fit the body sphere on the vertical axis and on the horizontal one, and keep the larger
        // distance of the two: whichever axis is tighter decides how far the camera has to stand back.
        float const half_fov_y = fov_y * 0.5f;
        float const half_fov_x = std::atan(aspect * std::tan(half_fov_y));
        float const sin_half_fov_y = std::sin(half_fov_y);
        float const sin_half_fov_x = std::sin(half_fov_x);
        float const distance_y = sin_half_fov_y > 0.0f
            ? framing.radius / sin_half_fov_y
            : framing.radius * Fallback_Distance_Factor;
        float const distance_x = sin_half_fov_x > 0.0f
            ? framing.radius / sin_half_fov_x
            : framing.radius * Fallback_Distance_Factor;
        distance = (std::max)(distance_x, distance_y) * Framing_Margin;
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

    DirectX::XMStoreFloat4x4(&out.view_proj, DirectX::XMMatrixMultiply(view, projection));
    out.eye = eye;
    return true;
}

PLUGIN_NAMESPACE_END
