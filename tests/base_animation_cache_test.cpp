//
// Created by AmazingBuff on 2026/10/21.
//

#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include <string_view>
namespace Plugin { inline constexpr std::string_view Plugin_Author = "AmazingBuff"; }
#include "base_animation_cache_cases.h"

#include <cassert>
#include <cstdio>

int main()
{
    assert(test_plugin::tests::fixed_bases_resolve_once_across_menu_sessions());
    assert(test_plugin::tests::project_replacement_and_load_reset_refresh_sparse_indices());
    std::puts("Fixed base animation cache: 2 cases PASS");
}
