#include "platform_bridge.h"

#include "os/browser.h"
#include "os/settings.h"

#include "compiled/warning_off.h"
#include <cctype>
#include "compiled/warning_on.h"

namespace evo::bridge
{
namespace
{
constexpr size_t MAX_JOIN_CODE_LENGTH = 8;

const os::SettingsLocation &GetSettingsLocation()
{
    static const os::SettingsLocation location{"", ".evo_tournament.cfg", "evo."};
    return location;
}
} // namespace

std::string JoinCodeFromUrl()
{
    std::string code;
    for (const char character : os::GetUrlQueryParameter("join"))
    {
        if (std::isalnum(static_cast<unsigned char>(character)) &&
            code.size() < MAX_JOIN_CODE_LENGTH)
        {
            code += static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
        }
    }
    return code;
}

std::string InviteLink(const std::string &room)
{
    if (!os::IsBrowser())
    {
        return room;
    }
    return os::GetPageAddress() + "?join=" + room;
}

std::string LoadSetting(const std::string &key, const std::string &fallback)
{
    return os::LoadSetting(GetSettingsLocation(), key, fallback);
}

void SaveSetting(const std::string &key, const std::string &value)
{
    os::SaveSetting(GetSettingsLocation(), key, value);
}
} // namespace evo::bridge
