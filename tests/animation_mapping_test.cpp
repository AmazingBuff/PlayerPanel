//
// Created by AmazingBuff on 2026/10/22.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/mapping_report.h"
#include "animation/track_mapping.h"
#include "animation/oar/base_animation_cache.h"

#include <cassert>
#include <iostream>
#include <vector>

void accepted_alias_changes_publish_and_identical_loops_coalesce()
{
    test_plugin::animation::MappingPublication publication;
    test_plugin::animation::AcceptedMapping accepted{
        "Animations\\TOR_Idle.hkx", "pack-a\\idle.hkx", "deduplicated\\idle.hkx", 10, 40000, 40000, 0, 1, true
    };
    assert(publication.accept(1, accepted));
    assert(!publication.accept(1, accepted));
    accepted.logical = "pack-b\\idle.hkx";
    accepted.variant_position = 1;
    assert(publication.accept(1, accepted));
    assert(!publication.accept(1, accepted));
    assert(publication.accept(2, accepted));
}

void indexed_binding_lookup_reads_only_the_selected_name()
{
    std::vector<std::string> names(50000);
    names[40000] = "OpenAnimationReplacer\\torch\\idle.hkx";
    std::size_t reads = 0;
    const std::string bound = test_plugin::oar::BaseAnimationCache::binding_name(names.size(), 40000, [&](std::size_t index)
    {
        ++reads;
        assert(index == 40000);
        return std::string_view(names[index]);
    });
    assert(bound == names[40000] && reads == 1);
}

void oversized_tracks_require_an_explicit_mapping()
{
    using test_plugin::resolve_animation_tracks;
    const auto compact = resolve_animation_tracks({}, 99, 116);
    assert(compact.size() == 99 && compact.back().track == 98 && compact.back().bone == 98);
    assert(resolve_animation_tracks({}, 116, 116).size() == 116);
    assert(resolve_animation_tracks({}, 242, 116).empty());
    assert(resolve_animation_tracks({}, 156, 116).empty());

    std::vector<std::int16_t> table(242, -1);
    table[1] = 2;
    table[241] = 38;
    const auto indexed = resolve_animation_tracks(table, 242, 116);
    assert(indexed.size() == 2 && indexed[0].track == 1 && indexed[0].bone == 2 &&
        indexed[1].track == 241 && indexed[1].bone == 38);
}

int main()
{
    accepted_alias_changes_publish_and_identical_loops_coalesce();
    indexed_binding_lookup_reads_only_the_selected_name();
    oversized_tracks_require_an_explicit_mapping();
    std::cout << "3 animation mapping cases passed (engine-free boundary)\n";
}
