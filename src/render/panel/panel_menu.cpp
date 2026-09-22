//
// Created by AmazingBuff on 2026/09/21.
//

#include "panel_menu.h"

#include "config/config.h"

#include <string>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // The interface folder the documented skin path starts with, and the two extensions the engine's
    // movie loader appends on its own.
    constexpr std::string_view Interface_Prefix = "Interface";
    constexpr std::string_view Swf_Suffix = ".swf";
    constexpr std::string_view Gfx_Suffix = ".gfx";

    char to_lower(char value)
    {
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
    }

    bool starts_with_ignore_case(std::string_view text, std::string_view prefix)
    {
        if (text.size() < prefix.size())
            return false;

        for (size_t index = 0; index < prefix.size(); ++index)
        {
            if (to_lower(text[index]) != to_lower(prefix[index]))
                return false;
        }
        return true;
    }

    bool ends_with_ignore_case(std::string_view text, std::string_view suffix)
    {
        if (text.size() < suffix.size())
            return false;

        return starts_with_ignore_case(text.substr(text.size() - suffix.size()), suffix);
    }

    // Turns the documented skin path into the argument the engine's movie loader expects. That loader
    // resolves its argument against the interface folder and appends the extension itself
    // (BSScaleformManager::LoadMovie -> BuildFilePath), so "Interface\PlayerPanel\panel.swf" reaches it
    // as "PlayerPanel/panel". Returns false for a path that would name no movie at all, so an unusable
    // configuration falls back instead of being loaded as something unintended.
    bool to_movie_name(char const* path, std::string& out)
    {
        if (!path || path[0] == '\0')
            return false;

        std::string_view name{ path };
        if (starts_with_ignore_case(name, Interface_Prefix))
        {
            name.remove_prefix(Interface_Prefix.size());
            if (!name.empty() && (name.front() == '\\' || name.front() == '/'))
                name.remove_prefix(1);
        }

        if (ends_with_ignore_case(name, Swf_Suffix))
            name.remove_suffix(Swf_Suffix.size());
        else if (ends_with_ignore_case(name, Gfx_Suffix))
            name.remove_suffix(Gfx_Suffix.size());

        if (name.empty())
            return false;

        // The loader builds its own path with forward slashes, so the name uses them too and the
        // engine never has to normalize a mixed separator.
        out.assign(name);
        for (char& character : out)
        {
            if (character == '\\')
                character = '/';
        }
        return true;
    }
}

PanelMenu* PanelMenu::current()
{
    RE::UI* const ui = RE::UI::GetSingleton();
    if (!ui)
        return nullptr;

    RE::GPtr<RE::IMenu> const menu = ui->GetMenu(Panel_Menu_Name);
    return static_cast<PanelMenu*>(menu.get());
}

RE::IMenu* PanelMenu::create()
{
    return new PanelMenu();
}

PanelMenu::PanelMenu() : m_movie_loaded(false)
{
    // Declaring the cursor menu is the whole of the plugin's input claim: while the panel's menu is
    // shown, the engine drives its own menu cursor and holds the camera, which is what replaces the
    // plugin's self-integrated cursor and its manual look freeze. The menu deliberately does not pause
    // the game, so kPausesGame is never set, and it is not modal, so gameplay keeps running behind it.
    menuFlags.set(RE::UI_MENU_FLAGS::kUsesCursor);

    RE::BSScaleformManager* const scaleform = RE::BSScaleformManager::GetSingleton();
    if (!scaleform)
    {
        logger::warn("Panel: the Scaleform manager is unavailable; the panel uses its built-in chrome");
        return;
    }

    std::string movie_name;
    if (!to_movie_name(Setting::instance().get_config().panel_swf_path, movie_name))
    {
        logger::warn("Panel: the configured skin path is unusable; the panel uses its built-in chrome");
        return;
    }

    // kExactFit scales the movie's stage into the viewport the plugin sets, so a skin's stage-filling
    // frame covers the whole panel rectangle instead of being letterboxed into it. The background alpha
    // keeps the movie's own declared background opaque, so the window still hides the world behind it.
    m_movie_loaded = scaleform->LoadMovie(this, uiMovie, movie_name.c_str(),
        RE::GFxMovieView::ScaleModeType::kExactFit, 1.0f);

    if (m_movie_loaded)
        logger::info("Panel: skin '{}' loaded; the panel's chrome comes from {}", movie_name, Panel_Swf_Path);
    else
        logger::warn("Panel: no usable skin movie at {}; the panel uses its built-in chrome", Panel_Swf_Path);
}

void PanelMenu::install()
{
    static bool s_installed = false;
    if (s_installed)
        return;

    RE::UI* const ui = RE::UI::GetSingleton();
    if (!ui)
    {
        logger::warn("Panel: the UI singleton is unavailable; the panel menu is not registered");
        return;
    }

    ui->Register(Panel_Menu_Name, &PanelMenu::create);
    s_installed = true;
    logger::info("Panel menu '{}' registered", Panel_Menu_Name);
}

void PanelMenu::set_open(bool open)
{
    RE::UIMessageQueue* const queue = RE::UIMessageQueue::GetSingleton();
    if (!queue)
    {
        logger::warn("Panel: the UI message queue is unavailable, so the panel menu cannot be {}",
            open ? "shown" : "hidden");
        return;
    }

    queue->AddMessage(RE::BSFixedString(Panel_Menu_Name),
        open ? RE::UI_MESSAGE_TYPE::kShow : RE::UI_MESSAGE_TYPE::kHide, nullptr);
}

PanelChrome PanelMenu::chrome_mode()
{
    PanelMenu const* const menu = current();
    if (!menu)
        return PanelChrome::e_pending;

    return menu->m_movie_loaded ? PanelChrome::e_swf : PanelChrome::e_built_in;
}

void PanelMenu::set_viewport(uint32_t buffer_width, uint32_t buffer_height, uint32_t left,
    uint32_t top, uint32_t width, uint32_t height)
{
    PanelMenu* const menu = current();
    if (!menu || !menu->m_movie_loaded || !menu->uiMovie)
        return;

    menu->uiMovie->SetViewport(static_cast<int32_t>(buffer_width), static_cast<int32_t>(buffer_height),
        static_cast<int32_t>(left), static_cast<int32_t>(top),
        static_cast<int32_t>(width), static_cast<int32_t>(height));
}

bool PanelMenu::read_menu_cursor(uint32_t render_width, uint32_t render_height, float& out_x,
    float& out_y)
{
    if (render_width == 0 || render_height == 0)
        return false;

    RE::MenuCursor* const cursor = RE::MenuCursor::GetSingleton();
    if (!cursor)
        return false;

    // The engine's cursor position is in the same unit as the screen extents the engine keeps beside
    // it, so those extents are what turn it into the render pixels the panel rectangle uses. Both are
    // absent until the engine has a cursor-using menu to drive.
    RE::MenuCursor::RUNTIME_DATA const& state = cursor->GetRuntimeData();
    if (state.screenWidthX <= 0.0f || state.screenWidthY <= 0.0f)
        return false;

    out_x = state.cursorPosX * static_cast<float>(render_width) / state.screenWidthX;
    out_y = state.cursorPosY * static_cast<float>(render_height) / state.screenWidthY;
    return true;
}

PLUGIN_NAMESPACE_END
