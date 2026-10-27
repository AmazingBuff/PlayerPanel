//
// Created by AmazingBuff on 2026/10/27.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/generation_entry_policy.h"

#include <cassert>
#include <iostream>
#include <vector>

#ifdef NDEBUG
#error This test requires assertions.
#endif

void native_entry_requires_private_active_storage()
{
    using test_plugin::animation::behavior_graph::GenerationEntryStatus;
    using test_plugin::animation::behavior_graph::GenerationEntryView;
    using test_plugin::animation::behavior_graph::generation_entry_status;
    const GenerationEntryView source{ true, 97, 652, 116, 1, 1, 2, 3, 4, 5, 6, { 7, 8 }, { 9, 10 } };
    const GenerationEntryView independent{ true, 97, 652, 116, 11, 11, 12, 13, 14, 15, 16, { 17, 18 }, { 19, 20 } };
    struct Row
    {
        GenerationEntryView view;
        GenerationEntryStatus expected;
    };
    std::vector<Row> rows{ { independent, GenerationEntryStatus::e_ready } };
    GenerationEntryView altered = independent;
    altered.outer_behavior = 0;
    rows.push_back({ altered, GenerationEntryStatus::e_behavior_missing });
    altered = independent; altered.character_behavior = 21;
    rows.push_back({ altered, GenerationEntryStatus::e_behavior_mismatch });
    altered = independent; altered.outer_behavior = 1; altered.character_behavior = 1;
    rows.push_back({ altered, GenerationEntryStatus::e_behavior_alias });
    altered = independent; altered.active = false;
    rows.push_back({ altered, GenerationEntryStatus::e_inactive });
    for (std::uintptr_t GenerationEntryView::* field : {
        &GenerationEntryView::active_nodes, &GenerationEntryView::active_data,
        &GenerationEntryView::variable_values, &GenerationEntryView::variable_data, &GenerationEntryView::pose })
    {
        altered = independent; altered.*field = 0;
        rows.push_back({ altered, GenerationEntryStatus::e_runtime_storage_missing });
        altered = independent; altered.*field = source.*field;
        rows.push_back({ altered, GenerationEntryStatus::e_runtime_storage_alias });
    }
    altered = independent; altered.pose_count = 0;
    rows.push_back({ altered, GenerationEntryStatus::e_runtime_storage_missing });
    altered = independent; altered.outputs[0] = 0;
    rows.push_back({ altered, GenerationEntryStatus::e_output_missing });
    altered = independent; altered.elements[1] = 0;
    rows.push_back({ altered, GenerationEntryStatus::e_output_missing });
    altered = independent; altered.outputs[0] = source.outputs[1];
    rows.push_back({ altered, GenerationEntryStatus::e_output_alias });
    altered = independent; altered.elements[1] = source.elements[0];
    rows.push_back({ altered, GenerationEntryStatus::e_output_alias });
    altered = independent; altered.outputs[0] = altered.outputs[1];
    rows.push_back({ altered, GenerationEntryStatus::e_output_alias });
    altered = independent; altered.elements[0] = altered.elements[1];
    rows.push_back({ altered, GenerationEntryStatus::e_output_alias });
    for (Row const& row : rows) assert(generation_entry_status(row.view, source) == row.expected);
    std::cout << "1 parameterized generation-entry ownership case passed (" << rows.size() << " value rows, engine-free)\n";
}

int main()
{
    native_entry_requires_private_active_storage();
}
