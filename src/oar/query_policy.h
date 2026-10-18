//
// Created by AmazingBuff on 2026/10/16.
//

#pragma once

#include "oar/selection_state.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

PLUGIN_NAMESPACE_BEGIN
namespace oar
{
    [[nodiscard]] inline std::string canonical_animation_path(std::string_view path)
    {
        std::string result;
        result.reserve(path.size());
        for (unsigned char character : path)
            result.push_back(character == '/' ? '\\' : static_cast<char>(std::tolower(character)));
        return result;
    }

    [[nodiscard]] inline std::optional<std::uint16_t> find_base_animation(
        std::span<std::string const> names, std::span<std::string_view const> candidates)
    {
        for (std::string_view candidate : candidates)
        {
            const std::string wanted = canonical_animation_path(candidate);
            for (std::size_t i = 0; i < names.size() && i < 0xFFFF; ++i)
            {
                if (canonical_animation_path(names[i]) == wanted)
                    return static_cast<std::uint16_t>(i);
            }
            for (std::size_t i = 0; i < names.size() && i < 0xFFFF; ++i)
            {
                std::string path = canonical_animation_path(names[i]);
                if (path.find("openanimationreplacer") != std::string::npos ||
                    path.find("dynamicanimationreplacer") != std::string::npos)
                    continue;
                for (std::string_view gender : { "animations\\female\\", "animations\\male\\" })
                {
                    const std::size_t at = path.find(gender);
                    if (at != std::string::npos)
                        path.replace(at, gender.size(), "animations\\");
                }
                if (path == wanted || path.ends_with("\\" + wanted))
                    return static_cast<std::uint16_t>(i);
            }
        }
        return std::nullopt;
    }

    // How a world clip decides the stance the player is ENTERING with (the panel-open edge). The
    // engine clears weaponState before the menu event and freezes it for the whole menu session,
    // so the recent clip history is the honest source: a draw move means drawn, a sheathe move
    // means sheathed, an idle decides by its family, and locomotion/attacks are not decisive on
    // their own (round 17: entering during 1HM_Equip answered sheathed and showed MT_Idle, and
    // entering during 1HM_Unequip answered drawn and showed a weapon idle).
    enum class ClipStance : std::uint8_t
    {
        e_undecided,
        e_drawn,
        e_sheathed
    };

    // "unequip" contains "equip": the sheathe move must be tested first.
    [[nodiscard]] inline bool clip_is_unequip(std::string_view clip)
    {
        return canonical_animation_path(clip).find("unequip") != std::string::npos;
    }

    [[nodiscard]] inline bool clip_is_equip(std::string_view clip)
    {
        const std::string path = canonical_animation_path(clip);
        return path.find("unequip") == std::string::npos && path.find("equip") != std::string::npos;
    }

    [[nodiscard]] inline ClipStance clip_stance(std::string_view clip)
    {
        const std::string path = canonical_animation_path(clip);
        if (path.find("unequip") != std::string::npos)
            return ClipStance::e_sheathed;
        if (path.find("equip") != std::string::npos)
            return ClipStance::e_drawn;
        if (path.find("idle") == std::string::npos)
            return ClipStance::e_undecided;
        // The unarmed base idle family answers sheathed; every other idle-named clip (weapon,
        // hand, torch, sneaking ...) answers drawn, matching the previous rounds' name rule.
        for (std::string_view base : { "mt_idle", "sneakmtidle", "mt_sprint", "mt_walk", "mt_run", "mt_jump", "mag_" })
            if (path.find(base) != std::string::npos)
                return ClipStance::e_sheathed;
        return ClipStance::e_drawn;
    }

    // A pack's idle sequence authored as sibling folders ("... idle Start", "... idle Loop").
    // OAR chooses between them with phase conditions (Smooth Moveset gates them on its own magic
    // effects), and those effects are switched by animation events the panel never fires while
    // sampling offline — so the panel advances by NAME, per the user's "the name is the
    // interface" rule (round 17).
    enum class ClipSequenceToken : std::uint8_t
    {
        e_none,
        e_start,
        e_loop
    };

    [[nodiscard]] inline ClipSequenceToken clip_sequence_token(std::string_view clip)
    {
        const std::string path = canonical_animation_path(clip);
        const std::size_t file = path.find_last_of('\\');
        if (file == std::string::npos || file == 0)
            return ClipSequenceToken::e_none;
        const std::size_t parent_begin = path.find_last_of('\\', file - 1);
        if (parent_begin == std::string::npos)
            return ClipSequenceToken::e_none;
        const std::string_view parent(path.data() + parent_begin + 1, file - parent_begin - 1);
        if (parent.find("start") != std::string::npos)
            return ClipSequenceToken::e_start;
        if (parent.find("loop") != std::string::npos)
            return ClipSequenceToken::e_loop;
        return ClipSequenceToken::e_none;
    }

    // What the panel should mirror as its entry action when the inventory opens. The source
    // clip is the world's own decision; replaying it from the source's local time is the only
    // faithful continuation while the paused world cannot be re-evaluated (round 18).
    enum class EntryActionKind : std::uint8_t
    {
        e_none,
        e_equip,
        e_unequip,
        e_sequence_start
    };

    [[nodiscard]] inline EntryActionKind entry_action_kind(std::string_view clip)
    {
        const std::string path = canonical_animation_path(clip);
        if (path.find("unequip") != std::string::npos)
            return EntryActionKind::e_unequip;
        if (path.find("equip") != std::string::npos)
            return EntryActionKind::e_equip;
        if (clip_sequence_token(path) == ClipSequenceToken::e_start)
            return EntryActionKind::e_sequence_start;
        return EntryActionKind::e_none;
    }

    // Case-insensitive path helpers. The sibling scan walks the whole animation-name catalogue
    // (~15k entries), so it must not allocate: the earlier canonical_animation_path-per-name
    // version built 15k strings on the draw thread at every transition (round 17).
    [[nodiscard]] inline bool path_equals_icase(std::string_view lhs, std::string_view rhs)
    {
        if (lhs.size() != rhs.size())
            return false;
        for (std::size_t i = 0; i < lhs.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(lhs[i])) != std::tolower(static_cast<unsigned char>(rhs[i])))
                return false;
        return true;
    }

    [[nodiscard]] inline std::size_t path_rfind_icase(std::string_view haystack, std::string_view needle)
    {
        if (needle.empty() || haystack.size() < needle.size())
            return std::string_view::npos;
        for (std::size_t i = haystack.size() - needle.size() + 1; i-- > 0;)
            if (path_equals_icase(haystack.substr(i, needle.size()), needle))
                return i;
        return std::string_view::npos;
    }

    [[nodiscard]] inline std::string_view path_tail(std::string_view path)
    {
        const std::size_t at = path.find_last_of("\\/");
        return at == std::string_view::npos ? path : path.substr(at + 1);
    }

    [[nodiscard]] inline std::string_view path_parent(std::string_view path)
    {
        const std::size_t file = path.find_last_of("\\/");
        if (file == std::string_view::npos || file == 0)
            return {};
        const std::size_t parent = path.find_last_of("\\/", file - 1);
        return parent == std::string_view::npos ? std::string_view{} : path.substr(parent + 1, file - parent - 1);
    }

    [[nodiscard]] inline std::string_view path_root(std::string_view path)
    {
        const std::size_t file = path.find_last_of("\\/");
        if (file == std::string_view::npos || file == 0)
            return {};
        const std::size_t parent = path.find_last_of("\\/", file - 1);
        return parent == std::string_view::npos ? std::string_view{} : path.substr(0, parent);
    }

    // The sequence family of a member folder: the folder name with its trailing start/loop marker
    // and episode digits removed ("1hm Non-Combat idle Loop2" → "1hm non-combat idle"). Siblings
    // must share it: Smooth Moveset also authors "1hm Combat idle Loop" under the SAME project
    // folder, and a root+filename match alone crossed into the combat family (round 18 verdict).
    [[nodiscard]] inline std::string path_sequence_stem(std::string_view parent)
    {
        std::string_view p = parent;
        bool marker_stripped = false;
        bool stripped = true;
        while (stripped)
        {
            stripped = false;
            while (!p.empty() && (std::isdigit(static_cast<unsigned char>(p.back())) || p.back() == ' '))
            {
                p.remove_suffix(1);
                stripped = true;
            }
            for (std::string_view marker : { "start", "loop" })
            {
                if (p.size() >= marker.size() &&
                    path_equals_icase(p.substr(p.size() - marker.size()), marker))
                {
                    p.remove_suffix(marker.size());
                    marker_stripped = true;
                    stripped = true;
                    break;
                }
            }
        }
        while (!p.empty() && (p.back() == ' ' || p.back() == '_' || p.back() == '-'))
            p.remove_suffix(1);
        // A folder without a sequence marker belongs to no family.
        return marker_stripped ? std::string(p) : std::string();
    }

    // One member of the sequence family a clip belongs to.
    struct SequenceMember
    {
        std::uint16_t index = 0xFFFF;
        std::string name;
        ClipSequenceToken token = ClipSequenceToken::e_none;
        int episode = 0;  // digits after the token: Loop → 0, Loop2 → 2
    };

    // Every member of the sequence family `current` belongs to: same project folder, same file
    // name, same folder stem (parent minus its start/loop marker and episode digits), ordered
    // Start → Loop → Loop2. The pack's own folder convention is the only interface — OAR's phase
    // conditions are frozen while the panel pauses the world, so the sequence is planned once
    // from names instead of being re-decided at every boundary (round 18, user model).
    [[nodiscard]] inline std::vector<SequenceMember> collect_sequence_family(
        std::span<std::string const> names, std::string_view current)
    {
        const std::string_view wanted_file = path_tail(current);
        const std::string_view wanted_root = path_root(current);
        const std::string wanted_stem = path_sequence_stem(path_parent(current));
        std::vector<SequenceMember> members;
        if (wanted_file.empty() || wanted_root.empty() || wanted_stem.empty())
            return members;
        for (std::size_t i = 0; i < names.size() && i < 0xFFFF; ++i)
        {
            const std::string_view path = names[i];
            if (!path_equals_icase(path_tail(path), wanted_file) ||
                !path_equals_icase(path_root(path), wanted_root))
                continue;
            const std::string_view parent = path_parent(path);
            const std::string stem = path_sequence_stem(parent);
            if (!path_equals_icase(stem, wanted_stem))
                continue;
            // The member's own token: the folder name ends (after episode digits) with the
            // marker. clip_sequence_token() cannot be used here — it expects a full path and
            // this is a bare folder name (round 18: the Loop/Loop2 members came back e_none).
            SequenceMember member;
            member.index = static_cast<std::uint16_t>(i);
            member.name = std::string(path);
            for (std::string_view marker : { std::string_view("loop"), std::string_view("start") })
            {
                const std::size_t at = path_rfind_icase(parent, marker);
                if (at == std::string_view::npos)
                    continue;
                const std::size_t end = at + marker.size();
                std::size_t after = end;
                while (after < parent.size() &&
                       (std::isdigit(static_cast<unsigned char>(parent[after])) || parent[after] == ' '))
                    ++after;
                if (after != parent.size())
                    continue;
                member.token = marker == "loop" ? ClipSequenceToken::e_loop : ClipSequenceToken::e_start;
                for (std::size_t d = end; d < parent.size() && std::isdigit(static_cast<unsigned char>(parent[d])); ++d)
                    member.episode = member.episode * 10 + (parent[d] - '0');
                break;
            }
            if (member.token == ClipSequenceToken::e_none)
                continue;
            members.push_back(std::move(member));
        }
        std::sort(members.begin(), members.end(), [](SequenceMember const& lhs, SequenceMember const& rhs)
        {
            const int lhs_rank = lhs.token == ClipSequenceToken::e_start ? 0 : 1;
            const int rhs_rank = rhs.token == ClipSequenceToken::e_start ? 0 : 1;
            if (lhs_rank != rhs_rank)
                return lhs_rank < rhs_rank;
            if (lhs.episode != rhs.episode)
                return lhs.episode < rhs.episode;
            return lhs.index < rhs.index;
        });
        return members;
    }

    class QuerySession
    {
    public:
        explicit QuerySession(std::uint32_t seed = 0x50414E4C) : selection(seed) {}
        SelectionState selection;
        void reset()
        {
            selection.reset();
            m_owner = nullptr;
            m_names.clear();
        }

        template <class F>
        [[nodiscard]] std::span<std::string const> names(void const* owner, void const* data, std::size_t count, F&& read)
        {
            if (m_owner != owner || m_name_data != data || m_names.size() != count)
            {
                m_owner = owner;
                m_name_data = data;
                // The catalogue is pure derived data of the source project's name table and
                // outlives panel sessions: rebuilding it on the inventory-open frame walked
                // ~15k entries on the main thread and stalled the panel for seconds
                // (oar-idle2 report, problem 5). The session-local previous behaviour remains
                // as the miss path; the process cache only removes the rebuild.
                m_names.clear();
                if (Catalogue const* const cached = project_catalogue(owner, data, count))
                {
                    m_names = cached->names;
                }
                else
                {
                    m_names.reserve(count);
                    for (std::size_t i = 0; i < count; ++i)
                        m_names.push_back(read(i));
                    remember_catalogue(owner, data, count, m_names);
                }
            }
            return m_names;
        }

    private:
        struct Catalogue
        {
            void const* owner = nullptr;
            void const* data = nullptr;
            std::size_t count = 0;
            std::vector<std::string> names;
        };

        // The table is engine-owned and replaced at a load boundary. A replacement usually gets a
        // NEW owner/data pointer pair, so the old entry cannot be matched and updated; keep the
        // cache bounded instead of appending one full ~15k-name table per load (round 16).
        static constexpr std::size_t Max_Catalogues = 4;

        [[nodiscard]] static Catalogue const* project_catalogue(void const* owner, void const* data, std::size_t count)
        {
            for (Catalogue& entry : catalogues())
            {
                if (entry.owner == owner && entry.data == data && entry.count == count)
                    return &entry;
            }
            return nullptr;
        }

        static void remember_catalogue(void const* owner, void const* data, std::size_t count,
            std::vector<std::string> const& names)
        {
            for (Catalogue& entry : catalogues())
            {
                if (entry.owner == owner || entry.data == data)
                {
                    entry.owner = owner;
                    entry.data = data;
                    entry.count = count;
                    entry.names = names;
                    return;
                }
            }
            if (catalogues().size() >= Max_Catalogues)
                catalogues().erase(catalogues().begin());
            catalogues().push_back({ owner, data, count, names });
        }

        static std::vector<Catalogue>& catalogues()
        {
            static std::vector<Catalogue> s_catalogues;
            return s_catalogues;
        }

        void const* m_owner = nullptr;
        void const* m_name_data = nullptr;
        std::vector<std::string> m_names;
    };
}
PLUGIN_NAMESPACE_END
