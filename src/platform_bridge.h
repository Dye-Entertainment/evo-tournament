#pragma once
#include "platform/types.h"

#include "compiled/warning_off.h"
#include <string>
#include "compiled/warning_on.h"

namespace evo::bridge
{
[[nodiscard]]
std::string JoinCodeFromUrl();

[[nodiscard]]
std::string InviteLink(const std::string &room);

[[nodiscard]]
std::string LoadSetting(const std::string &key, const std::string &fallback);

void SaveSetting(const std::string &key, const std::string &value);
} // namespace evo::bridge
