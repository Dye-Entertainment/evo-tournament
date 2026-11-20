#include "platform_bridge.h"

#ifdef __EMSCRIPTEN__
#include "compiled/warning_off.h"
#include <emscripten.h>
#include <cstdlib>
#include "compiled/warning_on.h"

EM_JS(void, evo_js_set_capture, (int wanted, int over_ui), {
    if (!Module.evoMouse) {
        return;
    }
    Module.evoMouse.wantLock = wanted !== 0;
    Module.evoMouse.overUi = over_ui !== 0;
    if (!Module.evoMouse.wantLock && document.pointerLockElement) {
        document.exitPointerLock();
    }
});

EM_JS(int, evo_js_is_captured, (), {
    return (Module.canvas && document.pointerLockElement === Module.canvas) ? 1 : 0;
});

EM_JS(void, evo_js_take_delta, (float *dx, float *dy), {
    var mouse = Module.evoMouse || {dx : 0, dy : 0};
    HEAPF32[dx >> 2] = mouse.dx;
    HEAPF32[dy >> 2] = mouse.dy;
    mouse.dx = 0;
    mouse.dy = 0;
});

EM_JS(char *, evo_js_join_code, (), {
    var code = '';
    try {
        code = new URLSearchParams(window.location.search).get('join') || '';
    } catch (e) {
    }
    code = code.replace(/[^A-Za-z0-9]/g, '').toUpperCase().slice(0, 8);
    return stringToNewUTF8(code);
});

EM_JS(char *, evo_js_invite_link, (const char *room), {
    var url = window.location.origin + window.location.pathname + '?join=' + UTF8ToString(room);
    return stringToNewUTF8(url);
});

EM_JS(void, evo_js_copy, (const char *text), {
    var value = UTF8ToString(text);
    try {
        navigator.clipboard.writeText(value);
    } catch (e) {
    }
});

EM_JS(char *, evo_js_load_setting, (const char *key), {
    var value = null;
    try {
        value = window.localStorage.getItem('evo.' + UTF8ToString(key));
    } catch (e) {
    }
    return value === null ? 0 : stringToNewUTF8(value);
});

EM_JS(void, evo_js_save_setting, (const char *key, const char *value), {
    try {
        window.localStorage.setItem('evo.' + UTF8ToString(key), UTF8ToString(value));
    } catch (e) {
    }
});

namespace evo::bridge
{
namespace
{
std::string TakeString(char *text)
{
    if (text == nullptr)
    {
        return {};
    }
    std::string value(text);
    std::free(text);
    return value;
}
} // namespace

Bool IsWeb()
{
    return true;
}

void SetMouseCapture(const Bool wanted, const Bool pointer_over_ui)
{
    evo_js_set_capture(wanted ? 1 : 0, pointer_over_ui ? 1 : 0);
}

Bool IsMouseCaptured()
{
    return evo_js_is_captured() != 0;
}

void TakeMouseDelta(Float &dx, Float &dy)
{
    evo_js_take_delta(&dx, &dy);
}

std::string JoinCodeFromUrl()
{
    return TakeString(evo_js_join_code());
}

std::string InviteLink(const std::string &room)
{
    return TakeString(evo_js_invite_link(room.c_str()));
}

void CopyToClipboard(const std::string &text)
{
    evo_js_copy(text.c_str());
}

std::string LoadSetting(const std::string &key, const std::string &fallback)
{
    char *value = evo_js_load_setting(key.c_str());
    return value == nullptr ? fallback : TakeString(value);
}

void SaveSetting(const std::string &key, const std::string &value)
{
    evo_js_save_setting(key.c_str(), value.c_str());
}
} // namespace evo::bridge

#else

#include "sdl/window/window_backend.h"
#include "window/window_manager.h"

#include "compiled/warning_off.h"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <fstream>
#include <map>
#include "compiled/warning_on.h"

namespace evo::bridge
{
namespace
{
SDL_Window *GetSdlWindow()
{
    auto *manager = window::Manager::GetInstance();
    if (manager == nullptr)
    {
        return nullptr;
    }
    auto *backend = static_cast<::sdl::window::WindowBackend *>(manager->GetWindow(0).backend);
    return backend != nullptr ? backend->sdlWindow : nullptr;
}

Bool captured = false;

std::string SettingsPath()
{
    const Char *home = std::getenv("HOME");
    return std::string(home != nullptr ? home : ".") + "/.evo_tournament.cfg";
}

std::map<std::string, std::string> ReadSettings()
{
    std::map<std::string, std::string> settings;
    std::ifstream file(SettingsPath());
    std::string line;
    while (std::getline(file, line))
    {
        const size_t equals = line.find('=');
        if (equals != std::string::npos)
        {
            settings[line.substr(0, equals)] = line.substr(equals + 1);
        }
    }
    return settings;
}
} // namespace

Bool IsWeb()
{
    return false;
}

void SetMouseCapture(const Bool wanted, const Bool pointer_over_ui)
{
    if (wanted == captured)
    {
        return;
    }
    SDL_Window *window = GetSdlWindow();
    if (window == nullptr)
    {
        return;
    }
    SDL_SetWindowRelativeMouseMode(window, wanted);
    captured = wanted;
    float x = 0.0f;
    float y = 0.0f;
    SDL_GetRelativeMouseState(&x, &y);
}

Bool IsMouseCaptured()
{
    return captured;
}

void TakeMouseDelta(Float &dx, Float &dy)
{
    float x = 0.0f;
    float y = 0.0f;
    SDL_GetRelativeMouseState(&x, &y);
    dx = captured ? x : 0.0f;
    dy = captured ? y : 0.0f;
}

std::string JoinCodeFromUrl()
{
    return {};
}

std::string InviteLink(const std::string &room)
{
    return room;
}

void CopyToClipboard(const std::string &text)
{
    SDL_SetClipboardText(text.c_str());
}

std::string LoadSetting(const std::string &key, const std::string &fallback)
{
    const auto settings = ReadSettings();
    const auto found = settings.find(key);
    return found == settings.end() ? fallback : found->second;
}

void SaveSetting(const std::string &key, const std::string &value)
{
    auto settings = ReadSettings();
    settings[key] = value;
    std::ofstream file(SettingsPath(), std::ios::trunc);
    for (const auto &[name, setting] : settings)
    {
        file << name << '=' << setting << '\n';
    }
}
} // namespace evo::bridge
#endif
