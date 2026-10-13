//
// Created by AmazingBuff on 2026/10/08.
//

#include "character/final_pose_probe.h"
#include "character/snapshot_transform.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    bool snapshot_placement_preserves_subtree()
    {
        RE::NiTransform captured;
        captured.translate = { 12000.0f, -5000.0f, 800.0f };
        captured.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, 1.5707963f);
        captured.scale = 2.0f;
        const RE::NiPoint3 center = captured * RE::NiPoint3{ 4.0f, 8.0f, 10.0f };
        const RE::NiPoint3 joint = captured * RE::NiPoint3{ 5.0f, 8.0f, 10.0f };
        const RE::NiPoint3 child = captured * RE::NiPoint3{ 7.0f, 8.0f, 10.0f };
        const RE::NiPoint3 anchor{ 0.0f, -485.0f, 0.0f };
        RE::NiTransform placement;
        placement.translate = anchor;
        placement.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, 1.5707963f);
        placement.scale = 0.35f;
        const RE::NiTransform delta = PLUGIN_NAMESPACE::snapshot_delta(captured, placement, center, anchor);
        return (delta * center - anchor).Length() <= 0.002f &&
            std::abs((delta * child - delta * joint).Length() - 0.7f) <= 0.002f;
    }

    bool final_pose_discriminates_index_order()
    {
        std::vector<PLUGIN_NAMESPACE::PoseSample> pose(12);
        std::vector<PLUGIN_NAMESPACE::TransformSample> matching;
        for (std::size_t i = 0; i < pose.size(); ++i)
        {
            const float half_angle = static_cast<float>(i) * 0.0261799f;
            pose[i].position[0] = static_cast<float>(i);
            pose[i].quaternion[2] = std::sin(half_angle);
            pose[i].quaternion[3] = std::cos(half_angle);
            pose[i].scale = 1.0f;
            matching.push_back(PLUGIN_NAMESPACE::pose_local(pose[i]));
        }
        std::vector<PLUGIN_NAMESPACE::TransformSample> shifted = matching;
        std::rotate(shifted.begin(), shifted.begin() + 1, shifted.end());
        const PLUGIN_NAMESPACE::OrderScore exact = PLUGIN_NAMESPACE::score_pose_order(pose, matching, PLUGIN_NAMESPACE::Pose_Match_Degrees);
        const PLUGIN_NAMESPACE::OrderScore wrong = PLUGIN_NAMESPACE::score_pose_order(pose, shifted, PLUGIN_NAMESPACE::Pose_Match_Degrees);
        const std::vector<PLUGIN_NAMESPACE::OrderScore> candidates{ wrong, exact };
        const PLUGIN_NAMESPACE::OrderDecision chosen = PLUGIN_NAMESPACE::decide_pose_order(candidates);
        return exact.compared == 12 && exact.within == 12 && wrong.ratio() < PLUGIN_NAMESPACE::Pose_Match_Min_Ratio &&
            chosen.match && chosen.best == 1;
    }
}

int main()
{
    const bool placement = snapshot_placement_preserves_subtree();
    const bool mapping = final_pose_discriminates_index_order();
    std::printf("snapshot placement: %s\nfinal pose index order: %s\n", placement ? "PASS" : "FAIL", mapping ? "PASS" : "FAIL");
    return placement && mapping ? 0 : 1;
}
