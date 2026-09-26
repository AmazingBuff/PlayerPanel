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
    [[maybe_unused]] constexpr std::string_view Interface_Prefix = "Interface";
    [[maybe_unused]] constexpr std::string_view Swf_Suffix = ".swf";
    [[maybe_unused]] constexpr std::string_view Gfx_Suffix = ".gfx";

    [[maybe_unused]] char to_lower(char value)
    {
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
    }

    [[maybe_unused]] bool starts_with_ignore_case(std::string_view text, std::string_view prefix)
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

    [[maybe_unused]] bool ends_with_ignore_case(std::string_view text, std::string_view suffix)
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
    [[maybe_unused]] bool to_movie_name(char const* path, std::string& out)
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

    // THE SKIN MOVIE IS SUSPENDED, so the constructor loads none. The engine's UI pass paints a
    // loaded movie's pixels after the plugin's present-hook composite has run - measured by the
    // content dump, where the composed panel was absent from the presented buffer while the movie's
    // chrome was on screen - so a skin's opaque backdrop covered the character every frame, which is
    // exactly the "empty frame" report. Until the composite can run after the UI pass (a different
    // hook point, or the chrome painted by the composite itself), loading a movie only buys a frame
    // that hides the panel's content, and the built-in chrome applies instead.
    m_movie_loaded = false;
    logger::warn("Panel: the skin movie is suspended ({}); the engine paints UI after the plugin's composite, so a loaded skin would cover the panel content. The built-in chrome applies", Panel_Swf_Path);
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

    // One-shot diagnostic: whether the viewport we set actually sticks is the open question of the
    // skin contract (the engine's own menu render may reset it to the full screen). Read it back once
    // per session so the log answers it, together with the movie's parsed stage rect.
    static bool s_probe_logged = false;
    if (!s_probe_logged)
    {
        s_probe_logged = true;
        RE::GViewport before{};
        menu->uiMovie->GetViewport(&before);
        logger::info("Panel viewport probe: before set - buffer={}x{} rect=({},{}),{}x{}",
            before.bufferWidth, before.bufferHeight, before.left, before.top, before.width, before.height);
    }

    menu->uiMovie->SetViewport(static_cast<int32_t>(buffer_width), static_cast<int32_t>(buffer_height),
        static_cast<int32_t>(left), static_cast<int32_t>(top),
        static_cast<int32_t>(width), static_cast<int32_t>(height));

    if (s_probe_logged)
    {
        static bool s_after_logged = false;
        if (!s_after_logged)
        {
            s_after_logged = true;
            RE::GViewport after{};
            menu->uiMovie->GetViewport(&after);
            RE::GRectF const frame = menu->uiMovie->GetVisibleFrameRect();
            logger::info("Panel viewport probe: after set - buffer={}x{} rect=({},{}),{}x{} (requested {},{}),{}x{}; stage frame rect=({},{}),{}x{}",
                after.bufferWidth, after.bufferHeight, after.left, after.top, after.width, after.height,
                left, top, width, height,
                frame.left, frame.top, frame.right - frame.left, frame.bottom - frame.top);
        }
    }
}

void PanelMenu::log_movie_state()
{
    PanelMenu* const menu = current();
    if (!menu || !menu->uiMovie)
    {
        logger::info("Panel movie state: no menu or movie");
        return;
    }

    RE::GViewport viewport{};
    menu->uiMovie->GetViewport(&viewport);
    RE::GRectF const frame = menu->uiMovie->GetVisibleFrameRect();
    logger::info("Panel movie state: viewport buffer={}x{} rect=({},{}),{}x{}; visible frame rect=({},{}),{}x{}",
        viewport.bufferWidth, viewport.bufferHeight, viewport.left, viewport.top, viewport.width, viewport.height,
        frame.left, frame.top, frame.right - frame.left, frame.bottom - frame.top);
}

bool PanelMenu::read_menu_cursor(uint32_t render_width, uint32_t render_height, float& out_x,
    float& out_y)
{    if (render_width == 0 || render_height == 0)
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
