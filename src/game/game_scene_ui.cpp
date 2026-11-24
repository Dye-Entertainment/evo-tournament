#include "game/game_scene.h"
#include "platform_bridge.h"

#include "os/browser.h"
#include "os/clipboard.h"

#include "compiled/warning_off.h"
#include <imgui.h>
#include <cstdio>
#include "compiled/warning_on.h"

namespace evo
{
namespace
{
const ImU32 WHITE = IM_COL32(255, 255, 255, 255);
const ImU32 SHADOW = IM_COL32(0, 0, 0, 170);
const ImU32 PANEL = IM_COL32(8, 8, 14, 150);
const ImU32 ACCENT = IM_COL32(255, 140, 40, 255);
const ImU32 DIM = IM_COL32(170, 170, 185, 255);

ImU32 Color(const FVec3 &color, const Float alpha = 1.0f)
{
    return IM_COL32(
        static_cast<int>(math::Clamp(color.x, 0, 1) * 255),
        static_cast<int>(math::Clamp(color.y, 0, 1) * 255),
        static_cast<int>(math::Clamp(color.z, 0, 1) * 255),
        static_cast<int>(math::Clamp(alpha, 0, 1) * 255)
    );
}

ImU32 WithAlpha(const ImU32 color, const Float alpha)
{
    const auto a = static_cast<ImU32>(
        math::Clamp(alpha, 0.0f, 1.0f) * static_cast<Float>((color >> IM_COL32_A_SHIFT) & 0xFF)
    );
    return (color & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

Float TextWidth(ImFont *font, const Float size, const std::string &text)
{
    if (font == nullptr)
    {
        font = ImGui::GetFont();
    }
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str()).x;
}

void Text(ImDrawList *list, ImFont *font, const Float size, const ImVec2 &position, const ImU32 color,
          const std::string &text, const Float align = 0.0f)
{
    const Float width = TextWidth(font, size, text);
    const ImVec2 at(position.x - width * align, position.y);
    const auto shadow_alpha = static_cast<Float>((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
    list->AddText(font, size, ImVec2(at.x + 2.0f, at.y + 2.0f), WithAlpha(SHADOW, shadow_alpha), text.c_str());
    list->AddText(font, size, at, color, text.c_str());
}

const Char *ShortWeaponName(const WeaponId weapon)
{
    switch (weapon)
    {
    case WeaponId::NOVA:
        return "NOVA";
    case WeaponId::RAIL:
        return "RAIL";
    default:
        return "PULSE";
    }
}

std::string Ordinal(const int rank)
{
    const int mod100 = rank % 100;
    const Char *suffix = "th";
    if (mod100 < 11 || mod100 > 13)
    {
        switch (rank % 10)
        {
        case 1:
            suffix = "st";
            break;
        case 2:
            suffix = "nd";
            break;
        case 3:
            suffix = "rd";
            break;
        default:
            break;
        }
    }
    return std::to_string(rank) + suffix;
}

void CenterNextWindow(const Float width)
{
    const ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
}

constexpr ImGuiWindowFlags PANEL_FLAGS = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;

void CenteredText(ImFont *font, const Float size, const ImU32 color, const Char *text)
{
    ImGui::PushFont(font, size);
    const Float width = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - width) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

const Char *SkillName(const Float skill)
{
    if (skill < 0.2f)
    {
        return "Novice";
    }
    if (skill < 0.4f)
    {
        return "Average";
    }
    if (skill < 0.6f)
    {
        return "Skilled";
    }
    if (skill < 0.8f)
    {
        return "Masterful";
    }
    return "Godlike";
}
} // namespace

void GameScene::DrawUi()
{
    ImGui::PushFont(font, 17.0f);
    if (screen == Screen::MAIN_MENU)
    {
        DrawMainMenu();
    }
    else if (screen == Screen::CONNECTING)
    {
        DrawConnecting();
    }
    else
    {
        DrawHud();
    }
    DrawErrorPopup();
    ImGui::PopFont();
}

void GameScene::DrawOptionsControls()
{
    Bool changed = false;
    changed |= ImGui::SliderFloat("Mouse sensitivity", &options.sensitivity, 0.1f, 5.0f, "%.2f");
    changed |= ImGui::SliderFloat("Field of view", &options.fieldOfView, 80.0f, 120.0f, "%.0f");
    if (ImGui::SliderFloat("Volume", &options.volume, 0.0f, 1.0f, "%.2f"))
    {
        sfx.SetMasterVolume(options.volume);
        changed = true;
    }
    changed |= ImGui::Checkbox("Invert mouse", &options.invertY);
    if (changed)
    {
        SaveOptions();
    }
}

void GameScene::DrawBotControls(const Bool live)
{
    Bool changed = ImGui::SliderInt("Bots", &options.match.botCount, 0, MAX_PLAYERS - 1);
    changed |= ImGui::SliderFloat(
        "Bot skill", &options.match.botSkill, 0.0f, 1.0f, SkillName(options.match.botSkill)
    );
    if (!changed)
    {
        return;
    }
    SaveOptions();
    if (live)
    {
        session.SetBots(world, options.match.botCount, options.match.botSkill);
    }
}

void GameScene::DrawMatchSettingsControls()
{
    int frag_limit = options.match.fragLimit;
    if (ImGui::SliderInt("Frag limit", &frag_limit, 5, 50))
    {
        options.match.fragLimit = static_cast<Int16>(frag_limit);
    }
    int minutes = static_cast<int>(options.match.timeLimitSeconds / 60.0f);
    if (ImGui::SliderInt("Time limit (min)", &minutes, 2, 30))
    {
        options.match.timeLimitSeconds = static_cast<Float>(minutes) * 60.0f;
    }
}

void GameScene::DrawMainMenu()
{
    CenterNextWindow(560.0f);
    ImGui::Begin("##main_menu", nullptr, PANEL_FLAGS);
    CenteredText(font, 46.0f, ACCENT, "EVO TOURNAMENT");
    CenteredText(font, 15.0f, DIM, "Arena deathmatch  -  built on the evogine engine");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextUnformatted("Your name");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##name", nameBuffer, MAX_NAME_LENGTH + 1))
    {
        options.name = nameBuffer;
    }
    if (options.name.empty())
    {
        options.name = "Player";
    }
    ImGui::Spacing();

    const Float full = ImGui::GetContentRegionAvail().x;
    ImGui::PushFont(font, 21.0f);
    if (ImGui::Button("INSTANT ACTION", ImVec2(full, 48.0f)))
    {
        StartOffline();
    }
    ImGui::PopFont();
    DrawBotControls(false);
    ImGui::Spacing();
    ImGui::SeparatorText("Online");

    if (!net::UsesRoomCodes())
    {
        ImGui::TextUnformatted("UDP port");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputText("##host", hostBuffer, sizeof(hostBuffer), ImGuiInputTextFlags_CharsDecimal))
        {
            hostAddress = hostBuffer;
        }
        ImGui::SameLine();
    }
    ImGui::PushFont(font, 19.0f);
    if (ImGui::Button("HOST ONLINE MATCH", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f)))
    {
        StartHost();
    }
    ImGui::PopFont();

    ImGui::SetNextItemWidth(full - 130.0f);
    const ImGuiInputTextFlags join_flags = net::UsesRoomCodes() ? ImGuiInputTextFlags_CharsUppercase : 0;
    if (ImGui::InputTextWithHint("##join", net::UsesRoomCodes() ? "Room code" : "host:port", joinBuffer,
                                 sizeof(joinBuffer), join_flags))
    {
        joinAddress = joinBuffer;
    }
    ImGui::SameLine();
    ImGui::PushFont(font, 19.0f);
    if (ImGui::Button("JOIN", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
    {
        joinAddress = joinBuffer;
        StartJoin();
    }
    ImGui::PopFont();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.68f, 1.0f), "%s",
                       net::UsesRoomCodes()
                           ? "Hosting opens a room with a short code. Friends join with the code or your invite "
                             "link; the host's browser runs the match and its bots (peer-to-peer WebRTC)."
                           : "Hosting listens on the UDP port above. Friends join with your-ip:port.");
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Match settings"))
    {
        DrawMatchSettingsControls();
    }
    if (ImGui::CollapsingHeader("Options"))
    {
        DrawOptionsControls();
    }
    if (ImGui::CollapsingHeader("Controls"))
    {
        ImGui::BulletText("WASD / arrows - move,  mouse - aim,  left click - fire");
        ImGui::BulletText("Space - jump, press again in the air to double jump");
        ImGui::BulletText("Double-tap a direction or press Shift - dodge");
        ImGui::BulletText("Jump along a wall while holding W - wall run, Space to kick off");
        ImGui::BulletText("1 2 3 / mouse wheel / Q - switch weapons");
        ImGui::BulletText("Tab - scoreboard,  Esc - menu");
    }
    ImGui::End();
}

void GameScene::DrawConnecting()
{
    CenterNextWindow(460.0f);
    ImGui::Begin("##connecting", nullptr, PANEL_FLAGS);
    CenteredText(font, 28.0f, ACCENT, "CONNECTING");
    ImGui::Spacing();
    const int dots = static_cast<int>(realTime * 3.0f) % 4;
    std::string status = session.TransportStatus();
    if (status.empty())
    {
        status = "Contacting host";
    }
    status += std::string(static_cast<size_t>(dots), '.');
    CenteredText(font, 16.0f, WHITE, status.c_str());
    ImGui::Spacing();
    if (ImGui::Button("Cancel", ImVec2(ImGui::GetContentRegionAvail().x, 36.0f)))
    {
        StartDemo();
    }
    ImGui::End();
}

void GameScene::DrawErrorPopup()
{
    if (errorMessage.empty())
    {
        return;
    }
    const ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.82f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Always);
    ImGui::Begin("##error", nullptr, PANEL_FLAGS);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", errorMessage.c_str());
    ImGui::PopTextWrapPos();
    if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
    {
        errorMessage.clear();
    }
    ImGui::End();
}

void GameScene::DrawCrosshair(const Float width, const Float height)
{
    ImDrawList *list = ImGui::GetBackgroundDrawList();
    const ImVec2 c(width * 0.5f, height * 0.5f);
    const Player *player = LocalPlayer();
    const WeaponId weapon = player != nullptr ? player->weapon : WeaponId::RIFLE;
    const ImU32 color = IM_COL32(255, 255, 255, 230);
    const auto line = [&](const Float x0, const Float y0, const Float x1, const Float y1)
    {
        list->AddLine(ImVec2(c.x + x0, c.y + y0), ImVec2(c.x + x1, c.y + y1), SHADOW, 4.0f);
        list->AddLine(ImVec2(c.x + x0, c.y + y0), ImVec2(c.x + x1, c.y + y1), color, 2.0f);
    };
    if (weapon == WeaponId::RAIL)
    {
        list->AddCircle(c, 9.0f, SHADOW, 24, 4.0f);
        list->AddCircle(c, 9.0f, color, 24, 2.0f);
        list->AddCircleFilled(c, 1.8f, color);
    }
    else if (weapon == WeaponId::NOVA)
    {
        line(-14, 0, -6, 0);
        line(6, 0, 14, 0);
        line(0, 6, 0, 14);
        list->AddCircleFilled(c, 2.0f, color);
    }
    else
    {
        line(-11, 0, -4, 0);
        line(4, 0, 11, 0);
        line(0, -11, 0, -4);
        line(0, 4, 0, 11);
    }
    if (hitMarker > 0.0f)
    {
        const ImU32 hit = WithAlpha(IM_COL32(255, 60, 50, 255), hitMarker / 0.22f);
        for (const Float sx : {-1.0f, 1.0f})
        {
            for (const Float sy : {-1.0f, 1.0f})
            {
                list->AddLine(ImVec2(c.x + sx * 7, c.y + sy * 7), ImVec2(c.x + sx * 15, c.y + sy * 15), hit, 3.0f);
            }
        }
    }
}

void GameScene::DrawKillFeed(const Float width)
{
    ImDrawList *list = ImGui::GetBackgroundDrawList();
    Float y = 118.0f;
    for (const FeedEntry &entry : feed)
    {
        const Float age = realTime - entry.time;
        const Float alpha = math::Clamp((FEED_SECONDS - age) / 0.8f, 0.0f, 1.0f);
        const std::string weapon = std::string("  [") + ShortWeaponName(entry.weapon) + (entry.headshot ? " HS" : "") + "]  ";
        const Bool suicide = entry.killer == entry.victim;
        const Float size = 16.0f;
        const Float victim_width = TextWidth(font, size, entry.victimName);
        const Float weapon_width = TextWidth(font, size, weapon);
        const Float killer_width = suicide ? 0.0f : TextWidth(font, size, entry.killerName);
        Float x = width - 24.0f - victim_width - weapon_width - killer_width;
        const Bool involved = entry.killer == session.localPlayer || entry.victim == session.localPlayer;
        list->AddRectFilled(ImVec2(x - 10.0f, y - 3.0f), ImVec2(width - 14.0f, y + size + 5.0f),
                            involved ? IM_COL32(120, 60, 10, static_cast<int>(150 * alpha)) : WithAlpha(PANEL, alpha), 5.0f);
        if (!suicide)
        {
            Text(list, font, size, ImVec2(x, y), Color(entry.killerColor, alpha), entry.killerName);
            x += killer_width;
        }
        Text(list, font, size, ImVec2(x, y), WithAlpha(DIM, alpha), weapon);
        x += weapon_width;
        Text(list, font, size, ImVec2(x, y), Color(entry.victimColor, alpha), entry.victimName);
        y += size + 11.0f;
    }
}

void GameScene::DrawNameplates(const Float width, const Float height)
{
    ImDrawList *list = ImGui::GetBackgroundDrawList();
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        const Player &player = world.players[i];
        if (!player.active || !player.alive || IsLocal(i))
        {
            continue;
        }
        const FVec3 head = player.renderPosition + FVec3{0.0f, 2.15f, 0.0f};
        const Float distance = math::GetLength(head - cameraEye);
        if (distance > 45.0f || !world.arena.HasLineOfSight(cameraEye, head))
        {
            continue;
        }
        FVec2 screen_position;
        if (!renderer.Project(head, screen_position, width, height))
        {
            continue;
        }
        const Float alpha = math::Clamp((45.0f - distance) / 10.0f, 0.0f, 1.0f);
        const Float size = math::Clamp(20.0f - distance * 0.15f, 13.0f, 20.0f);
        Text(list, font, size, ImVec2(screen_position.x, screen_position.y - size), Color(PlayerColor(player.colorIndex), alpha),
             player.name, 0.5f);
    }
}

void GameScene::DrawScoreboard(const Float width, const Float height)
{
    ImDrawList *list = ImGui::GetBackgroundDrawList();
    const std::vector<int> ranked = RankedPlayers();
    const Float panel_width = 640.0f;
    const Float row = 30.0f;
    const Float panel_height = 120.0f + row * static_cast<Float>(ranked.size());
    const ImVec2 origin((width - panel_width) * 0.5f, std::max(60.0f, (height - panel_height) * 0.42f));
    list->AddRectFilled(origin, ImVec2(origin.x + panel_width, origin.y + panel_height), IM_COL32(6, 6, 12, 215), 12.0f);
    list->AddRect(origin, ImVec2(origin.x + panel_width, origin.y + panel_height), IM_COL32(255, 140, 40, 120), 12.0f, 0, 2.0f);

    std::string title = "SCOREBOARD";
    if (world.match.phase == MatchPhase::ENDED)
    {
        title = world.match.winner >= 0 ? PlayerName(world.match.winner) + " WINS" : "MATCH OVER";
    }
    Text(list, font, 30.0f, ImVec2(origin.x + panel_width * 0.5f, origin.y + 16.0f), ACCENT, title, 0.5f);
    const Float header_y = origin.y + 66.0f;
    Text(list, font, 14.0f, ImVec2(origin.x + 30.0f, header_y), DIM, "#");
    Text(list, font, 14.0f, ImVec2(origin.x + 70.0f, header_y), DIM, "PLAYER");
    Text(list, font, 14.0f, ImVec2(origin.x + 450.0f, header_y), DIM, "FRAGS", 0.5f);
    Text(list, font, 14.0f, ImVec2(origin.x + 550.0f, header_y), DIM, "DEATHS", 0.5f);

    Float y = header_y + 28.0f;
    int rank = 1;
    for (const int id : ranked)
    {
        const Player &player = world.players[id];
        const Bool local = IsLocal(id);
        if (local)
        {
            list->AddRectFilled(ImVec2(origin.x + 14.0f, y - 4.0f), ImVec2(origin.x + panel_width - 14.0f, y + row - 6.0f),
                                IM_COL32(255, 140, 40, 55), 6.0f);
        }
        std::string tag;
        if (player.control == PlayerControl::BOT)
        {
            tag = "BOT";
        }
        else if (id == 0 && session.mode != net::SessionMode::OFFLINE)
        {
            tag = "HOST";
        }
        Text(list, font, 18.0f, ImVec2(origin.x + 30.0f, y), WHITE, std::to_string(rank));
        list->AddRectFilled(ImVec2(origin.x + 56.0f, y + 3.0f), ImVec2(origin.x + 62.0f, y + 18.0f),
                            Color(PlayerColor(player.colorIndex)));
        Text(list, font, 18.0f, ImVec2(origin.x + 70.0f, y), local ? ACCENT : WHITE, player.name);
        if (!tag.empty())
        {
            Text(list, font, 12.0f, ImVec2(origin.x + 80.0f + TextWidth(font, 18.0f, player.name), y + 4.0f), DIM, tag);
        }
        Text(list, font, 18.0f, ImVec2(origin.x + 450.0f, y), WHITE, std::to_string(player.frags), 0.5f);
        Text(list, font, 18.0f, ImVec2(origin.x + 550.0f, y), DIM, std::to_string(player.deaths), 0.5f);
        y += row;
        ++rank;
    }
}

void GameScene::DrawPauseMenu(const Float width, const Float height)
{
    static_cast<void>(width);
    static_cast<void>(height);
    CenterNextWindow(480.0f);
    ImGui::Begin("##pause", nullptr, PANEL_FLAGS);
    CenteredText(font, 30.0f, ACCENT, os::IsBrowser() ? "CLICK THE ARENA TO PLAY" : "PAUSED");
    ImGui::Spacing();
    if (session.mode == net::SessionMode::HOST)
    {
        ImGui::SeparatorText("Invite friends");
        if (net::UsesRoomCodes())
        {
            const std::string code = "Room code: " + roomCode;
            CenteredText(font, 26.0f, WHITE, code.c_str());
            static char link_buffer[256];
            std::snprintf(link_buffer, sizeof(link_buffer), "%s", bridge::InviteLink(roomCode).c_str());
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90.0f);
            ImGui::InputText("##link", link_buffer, sizeof(link_buffer), ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            if (ImGui::Button("Copy", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
            {
                os::SetClipboardText(link_buffer);
                notices.emplace_back("Invite link copied", realTime);
            }
        }
        else
        {
            ImGui::Text("Friends join with  <your-ip>:%s", roomCode.c_str());
        }
        ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.72f, 1.0f), "%d friend(s) connected", session.RemotePlayerCount(world));
    }
    else if (session.mode == net::SessionMode::CLIENT)
    {
        ImGui::Text("Connected to %s", session.address.c_str());
    }
    if (session.mode == net::SessionMode::OFFLINE || session.mode == net::SessionMode::HOST)
    {
        ImGui::SeparatorText("Bots");
        DrawBotControls(true);
    }
    if (ImGui::CollapsingHeader("Options"))
    {
        DrawOptionsControls();
    }
    ImGui::Spacing();
    const Float full = ImGui::GetContentRegionAvail().x;
    if (!os::IsBrowser() && ImGui::Button("Resume", ImVec2(full, 38.0f)))
    {
        menuOpen = false;
        fireArmed = false;
    }
    if (ImGui::Button("Leave match", ImVec2(full, 38.0f)))
    {
        StartDemo();
    }
    ImGui::End();
}

void GameScene::DrawHud()
{
    const ImGuiIO &io = ImGui::GetIO();
    const Float width = io.DisplaySize.x;
    const Float height = io.DisplaySize.y;
    ImDrawList *list = ImGui::GetBackgroundDrawList();
    const Player *player = LocalPlayer();

    if (damageFlash > 0.0f)
    {
        const ImU32 red =
            IM_COL32(200, 0, 0, static_cast<int>(math::Clamp(damageFlash, 0.0f, 1.0f) * 110));
        const ImU32 clear = IM_COL32(200, 0, 0, 0);
        const Float edge = 160.0f;
        list->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(edge, height), red, clear, clear, red);
        list->AddRectFilledMultiColor(ImVec2(width - edge, 0), ImVec2(width, height), clear, red, red, clear);
        list->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(width, edge), red, red, clear, clear);
        list->AddRectFilledMultiColor(ImVec2(0, height - edge), ImVec2(width, height), clear, clear, red, red);
    }

    DrawNameplates(width, height);

    if (player != nullptr && player->alive)
    {
        DrawCrosshair(width, height);

        const ImVec2 panel(24.0f, height - 96.0f);
        list->AddRectFilled(panel, ImVec2(panel.x + 330.0f, panel.y + 72.0f), PANEL, 10.0f);
        const Bool low = player->health <= 30;
        const Float pulse = low ? 0.6f + 0.4f * std::sin(realTime * 8.0f) : 1.0f;
        const ImU32 health_color = low ? WithAlpha(IM_COL32(255, 70, 60, 255), pulse) : WHITE;
        list->AddRectFilled(ImVec2(panel.x + 20, panel.y + 30), ImVec2(panel.x + 44, panel.y + 40), IM_COL32(255, 70, 60, 255));
        list->AddRectFilled(ImVec2(panel.x + 27, panel.y + 23), ImVec2(panel.x + 37, panel.y + 47), IM_COL32(255, 70, 60, 255));
        Text(list, font, 46.0f, ImVec2(panel.x + 54.0f, panel.y + 10.0f), health_color, std::to_string(std::max<int>(player->health, 0)));
        list->AddRectFilled(ImVec2(panel.x + 180, panel.y + 22), ImVec2(panel.x + 204, panel.y + 40), IM_COL32(255, 205, 60, 255), 3.0f);
        list->AddTriangleFilled(ImVec2(panel.x + 180, panel.y + 40), ImVec2(panel.x + 204, panel.y + 40),
                                ImVec2(panel.x + 192, panel.y + 50), IM_COL32(255, 205, 60, 255));
        Text(list, font, 46.0f, ImVec2(panel.x + 214.0f, panel.y + 10.0f),
             player->armor > 0 ? WHITE : WithAlpha(DIM, 0.6f), std::to_string(player->armor));

        const int weapon = static_cast<int>(player->weapon);
        const WeaponDef &def = GetWeaponDef(player->weapon);
        const ImVec2 ammo_panel(width - 254.0f, height - 96.0f);
        list->AddRectFilled(ammo_panel, ImVec2(width - 24.0f, ammo_panel.y + 72.0f), PANEL, 10.0f);
        Text(list, font, 15.0f, ImVec2(ammo_panel.x + 18.0f, ammo_panel.y + 10.0f), Color(def.color), def.name);
        Text(list, font, 46.0f, ImVec2(width - 42.0f, ammo_panel.y + 10.0f),
             player->ammo[weapon] > 0 ? WHITE : IM_COL32(255, 70, 60, 255), std::to_string(player->ammo[weapon]), 1.0f);

        for (int w = 0; w < WEAPON_COUNT; ++w)
        {
            const ImVec2 slot(width - 254.0f + static_cast<Float>(w) * 78.0f, height - 140.0f);
            const Bool owned = player->hasWeapon[w];
            const Bool selected = w == weapon;
            const auto id = static_cast<WeaponId>(w);
            list->AddRectFilled(slot, ImVec2(slot.x + 72.0f, slot.y + 36.0f),
                                selected ? IM_COL32(255, 140, 40, 120) : PANEL, 6.0f);
            const ImU32 text_color = owned ? Color(GetWeaponDef(id).color) : IM_COL32(110, 110, 120, 160);
            Text(list, font, 12.0f, ImVec2(slot.x + 6.0f, slot.y + 3.0f), text_color,
                 std::to_string(w + 1) + " " + ShortWeaponName(id));
            Text(list, font, 14.0f, ImVec2(slot.x + 66.0f, slot.y + 18.0f), owned ? WHITE : IM_COL32(110, 110, 120, 160),
                 owned ? std::to_string(player->ammo[w]) : "-", 1.0f);
        }
    }
    else if (player != nullptr && world.match.phase == MatchPhase::PLAYING)
    {
        const Float alpha = 0.6f + 0.4f * std::sin(realTime * 4.0f);
        Text(list, font, 24.0f, ImVec2(width * 0.5f, height * 0.62f), WithAlpha(WHITE, alpha), "Click to respawn", 0.5f);
    }

    const int seconds = static_cast<int>(std::ceil(world.match.timeRemaining));
    char timer[16];
    std::snprintf(timer, sizeof(timer), "%d:%02d", seconds / 60, seconds % 60);
    list->AddRectFilled(ImVec2(width * 0.5f - 70.0f, 12.0f), ImVec2(width * 0.5f + 70.0f, 72.0f), PANEL, 10.0f);
    Text(list, font, 30.0f, ImVec2(width * 0.5f, 16.0f), seconds < 30 ? IM_COL32(255, 90, 60, 255) : WHITE, timer, 0.5f);
    Text(list, font, 12.0f, ImVec2(width * 0.5f, 52.0f), DIM, "FRAG LIMIT " + std::to_string(world.settings.fragLimit), 0.5f);

    if (player != nullptr)
    {
        const std::vector<int> ranked = RankedPlayers();
        int rank = 1;
        int best_other = -1000;
        for (const int id : ranked)
        {
            if (id == session.localPlayer)
            {
                break;
            }
            ++rank;
        }
        for (const int id : ranked)
        {
            if (id != session.localPlayer)
            {
                best_other = std::max<int>(best_other, world.players[id].frags);
            }
        }
        list->AddRectFilled(ImVec2(width - 200.0f, 12.0f), ImVec2(width - 14.0f, 98.0f), PANEL, 10.0f);
        Text(list, font, 36.0f, ImVec2(width - 186.0f, 16.0f), rank == 1 ? ACCENT : WHITE, Ordinal(rank));
        Text(list, font, 14.0f, ImVec2(width - 28.0f, 22.0f), DIM, "of " + std::to_string(ranked.size()), 1.0f);
        Text(list, font, 18.0f, ImVec2(width - 186.0f, 62.0f), WHITE, std::to_string(player->frags) + " frags");
        if (best_other > -1000)
        {
            const int lead = player->frags - best_other;
            const std::string lead_text = lead >= 0 ? "+" + std::to_string(lead) : std::to_string(lead);
            Text(list, font, 16.0f, ImVec2(width - 28.0f, 64.0f), lead >= 0 ? IM_COL32(120, 255, 140, 255) : IM_COL32(255, 120, 100, 255),
                 lead_text, 1.0f);
        }
    }

    DrawKillFeed(width);

    Float notice_y = 18.0f;
    if (session.mode == net::SessionMode::HOST && net::UsesRoomCodes())
    {
        Text(list, font, 16.0f, ImVec2(20.0f, notice_y), IM_COL32(120, 220, 255, 255),
             "ROOM " + roomCode + "  -  " + std::to_string(session.RemotePlayerCount(world)) + " friend(s)  -  Esc for invite link");
        notice_y += 26.0f;
    }
    else if (session.mode == net::SessionMode::HOST)
    {
        Text(list, font, 16.0f, ImVec2(20.0f, notice_y), IM_COL32(120, 220, 255, 255),
             "HOSTING ON PORT " + roomCode + "  -  " + std::to_string(session.RemotePlayerCount(world)) + " friend(s)");
        notice_y += 26.0f;
    }
    else if (session.mode == net::SessionMode::CLIENT)
    {
        Text(list, font, 16.0f, ImVec2(20.0f, notice_y), IM_COL32(120, 220, 255, 255), "ONLINE  -  " + session.address);
        notice_y += 26.0f;
    }
    for (const auto &[message, time] : notices)
    {
        const Float alpha = math::Clamp((6.0f - (realTime - time)) / 0.8f, 0.0f, 1.0f);
        Text(list, font, 15.0f, ImVec2(20.0f, notice_y), WithAlpha(WHITE, alpha), message);
        notice_y += 22.0f;
    }

    if (realTime < banner.until)
    {
        const Float alpha = math::Clamp((banner.until - realTime) / 0.5f, 0.0f, 1.0f);
        Text(list, font, 34.0f, ImVec2(width * 0.5f, height * 0.26f), Color(banner.color, alpha), banner.text, 0.5f);
        if (!banner.subtext.empty())
        {
            Text(list, font, 18.0f, ImVec2(width * 0.5f, height * 0.26f + 44.0f), WithAlpha(WHITE, alpha), banner.subtext, 0.5f);
        }
    }
    if (realTime < pickupBanner.until)
    {
        const Float alpha = math::Clamp((pickupBanner.until - realTime) / 0.5f, 0.0f, 1.0f);
        Text(list, font, 20.0f, ImVec2(width * 0.5f, height - 150.0f), Color(pickupBanner.color, alpha), pickupBanner.text, 0.5f);
    }

    if (showScoreboard && !menuOpen)
    {
        DrawScoreboard(width, height);
    }
    if (menuOpen)
    {
        list->AddRectFilled(ImVec2(0, 0), ImVec2(width, height), IM_COL32(0, 0, 0, 90));
        DrawPauseMenu(width, height);
    }
}
} // namespace evo
