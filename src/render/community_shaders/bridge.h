#pragma once

#include "render/community_shaders/binary_profile.h"

PLUGIN_NAMESPACE_BEGIN
namespace community_shaders
{
    struct Bridge
    {
        std::uintptr_t base{};
        bool ready{};
        void (*draw)(void*){};

        template <class T>
        [[nodiscard]] T function(Function id) const
        {
            return reinterpret_cast<T>(base + Entries[static_cast<std::size_t>(id)].rva);
        }

        [[nodiscard]] void* object(std::uintptr_t rva) const
        {
            return *reinterpret_cast<void**>(base + rva);
        }
    };

    struct DrawOverride
    {
        void* context;
        void (*after_draw)(void*);
    };

    [[nodiscard]] Bridge& bridge();
    extern thread_local DrawOverride* current_view;
}
PLUGIN_NAMESPACE_END
