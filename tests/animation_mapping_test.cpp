//
// Created by AmazingBuff on 2026/10/22.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/mapping_report.h"
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

int main()
{
    accepted_alias_changes_publish_and_identical_loops_coalesce();
    indexed_binding_lookup_reads_only_the_selected_name();
    std::cout << "2 accepted mapping/indexed read cases passed (engine-free boundary)\n";
}
