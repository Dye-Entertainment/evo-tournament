#pragma once
#include "platform/types.h"

#include "compiled/warning_off.h"
#include <string>
#include "compiled/warning_on.h"

namespace evo::bridge
{
[[nodiscard]]
Bool IsWeb();

void SetMouseCapture(Bool wanted, Bool pointer_over_ui);

[[nodiscard]]
Bool IsMouseCaptured();

void TakeMouseDelta(Float &dx, Float &dy);

[[nodiscard]]
std::string JoinCodeFromUrl();

[[nodiscard]]
std::string InviteLink(const std::string &room);

void CopyToClipboard(const std::string &text);

[[nodiscard]]
std::string LoadSetting(const std::string &key, const std::string &fallback);

void SaveSetting(const std::string &key, const std::string &value);
} // namespace evo::bridge
