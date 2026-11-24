#include "game/game_scene.h"
#include "platform_bridge.h"

#include "os/browser.h"
#include "os/clipboard.h"
#include "ui/ui_manager.h"
#include "window/window_manager.h"

#include "compiled/warning_off.h"
#include <cctype>
#include <cstdio>
#include "compiled/warning_on.h"

namespace evo
{
namespace
{
std::string Hex(const FVec3 &color)
{
    char buffer[8];
    std::snprintf(
        buffer, sizeof(buffer), "#%02x%02x%02x", static_cast<int>(math::Clamp(color.x, 0, 1) * 255),
        static_cast<int>(math::Clamp(color.y, 0, 1) * 255), static_cast<int>(math::Clamp(color.z, 0, 1) * 255)
    );
    return buffer;
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

void GameScene::InitializeUi()
{
    uiReady = ui.Initialize(std::string(EVO_RESOURCE_DIR) + "/ui");
    if (!uiReady)
    {
        return;
    }
    ui.OnAction("instant", [this] { StartOffline(); });
    ui.OnAction("host", [this] { StartHost(); });
    ui.OnAction("join", [this] { StartJoin(); });
    ui.OnAction("cancel", [this] { StartDemo(); });
    ui.OnAction("leave", [this] { StartDemo(); });
    ui.OnAction("error-ok", [this] { errorMessage.clear(); });
    ui.OnAction(
        "resume",
        [this]
        {
            menuOpen = false;
            fireArmed = false;
        }
    );
    ui.OnAction(
        "copy",
        [this]
        {
            os::SetClipboardText(bridge::InviteLink(roomCode));
            notices.emplace_back("Invite link copied", realTime);
        }
    );

    UiState &s = ui.State();
    s.name = options.name;
    s.botCount = options.match.botCount;
    s.botSkill = options.match.botSkill;
    s.fragLimit = options.match.fragLimit;
    s.timeLimit = static_cast<Int32>(options.match.timeLimitSeconds / 60.0f);
    s.sensitivity = options.sensitivity;
    s.fov = options.fieldOfView;
    s.volume = options.volume;
    s.invertY = options.invertY;
    s.hostPort = hostAddress;
    s.joinText = joinAddress;
    s.roomCodes = net::UsesRoomCodes();
    s.onlineHelp = s.roomCodes ? "Hosting opens a room with a short code. Friends join with the code or your "
                                 "invite link; the host's browser runs the match and its bots (peer-to-peer WebRTC)."
                               : "Hosting listens on the UDP port above. Friends join with your-ip:port.";
    ui.Commit(true);
}

Bool GameScene::IsTypingInUi() const
{
    return uiReady && ui::Manager::GetInstance()->IsTextInputFocused();
}

Bool GameScene::IsPointerOverUi() const
{
    return uiReady && ui::Manager::GetInstance()->IsPointerOverUi();
}

Bool GameScene::SyncUiForms()
{
    // The document writes the form fields straight into the UI state; copy them into the options.
    UiState &s = ui.State();
    Bool forms_dirty = false;
    Bool changed = false;
    if (s.name != options.name)
    {
        options.name = s.name.empty() ? "Player" : s.name.substr(0, MAX_NAME_LENGTH);
    }
    if (s.roomCodes)
    {
        std::string upper = s.joinText;
        for (char &c : upper)
        {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (upper != s.joinText)
        {
            s.joinText = upper;
            forms_dirty = true;
        }
    }
    joinAddress = s.joinText;
    hostAddress = s.hostPort;

    const Int32 bots = std::clamp(s.botCount, 0, MAX_PLAYERS - 1);
    const Bool bots_changed = bots != options.match.botCount || s.botSkill != options.match.botSkill;
    if (bots_changed)
    {
        options.match.botCount = bots;
        options.match.botSkill = math::Clamp(s.botSkill, 0.0f, 1.0f);
        changed = true;
        if (screen == Screen::IN_MATCH &&
            (session.mode == net::SessionMode::OFFLINE || session.mode == net::SessionMode::HOST))
        {
            session.SetBots(world, options.match.botCount, options.match.botSkill);
        }
    }
    const auto frag_limit = static_cast<Int16>(std::clamp(s.fragLimit, 5, 50));
    const Float time_limit = static_cast<Float>(std::clamp(s.timeLimit, 2, 30)) * 60.0f;
    if (frag_limit != options.match.fragLimit || time_limit != options.match.timeLimitSeconds)
    {
        options.match.fragLimit = frag_limit;
        options.match.timeLimitSeconds = time_limit;
    }
    if (s.sensitivity != options.sensitivity || s.fov != options.fieldOfView || s.invertY != options.invertY)
    {
        options.sensitivity = math::Clamp(s.sensitivity, 0.1f, 5.0f);
        options.fieldOfView = math::Clamp(s.fov, 80.0f, 120.0f);
        options.invertY = s.invertY;
        changed = true;
    }
    if (s.volume != options.volume)
    {
        options.volume = math::Clamp(s.volume, 0.0f, 1.0f);
        sfx.SetMasterVolume(options.volume);
        changed = true;
    }
    if (changed)
    {
        SaveOptions();
    }
    return forms_dirty;
}

void GameScene::UpdateUi()
{
    if (!uiReady)
    {
        return;
    }
    const Bool forms_dirty = SyncUiForms();
    UiState &s = ui.State();
    s.screen = screen == Screen::MAIN_MENU ? "menu" : screen == Screen::CONNECTING ? "connecting" : "match";
    s.skillName = SkillName(options.match.botSkill);
    s.error = errorMessage;
    if (screen == Screen::CONNECTING)
    {
        const int dots = static_cast<int>(realTime * 3.0f) % 4;
        s.connectStatus = session.TransportStatus();
        if (s.connectStatus.empty())
        {
            s.connectStatus = "Contacting host";
        }
        s.connectStatus += std::string(static_cast<size_t>(dots), '.');
    }
    if (screen == Screen::IN_MATCH)
    {
        FillHud();
    }
    else
    {
        s.pause = false;
        s.scoreboard = false;
    }
    ui.Commit(forms_dirty);
}

void GameScene::FillHud()
{
    UiState &s = ui.State();
    const IVec2 size = window::Manager::GetInstance()->GetWindowSize(0);
    const auto width = static_cast<Float>(size.x);
    const auto height = static_cast<Float>(size.y);
    const Float scale = ui.Scale();
    const Player *player = LocalPlayer();

    s.damage = math::Clamp(damageFlash, 0.0f, 1.0f);
    s.alive = player != nullptr && player->alive;
    s.respawn = !s.alive && player != nullptr && world.match.phase == MatchPhase::PLAYING;
    s.respawnAlpha = 0.6f + 0.4f * std::sin(realTime * 4.0f);

    // Names over the other players' heads.
    s.nameplates.clear();
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        const Player &other = world.players[i];
        if (!other.active || !other.alive || IsLocal(i))
        {
            continue;
        }
        const FVec3 head = other.renderPosition + FVec3{0.0f, 2.15f, 0.0f};
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
        UiNameplate plate;
        plate.name = other.name;
        plate.color = Hex(PlayerColor(other.colorIndex));
        plate.alpha = math::Clamp((45.0f - distance) / 10.0f, 0.0f, 1.0f);
        plate.size = math::Clamp(20.0f - distance * 0.15f, 13.0f, 20.0f);
        plate.x = screen_position.x / scale;
        plate.y = screen_position.y / scale;
        s.nameplates.push_back(plate);
    }

    if (s.alive)
    {
        s.crosshair = player->weapon == WeaponId::RAIL ? "rail" : player->weapon == WeaponId::NOVA ? "nova" : "rifle";
        s.hit = math::Clamp(hitMarker / 0.22f, 0.0f, 1.0f);
        s.health = std::max<int>(player->health, 0);
        s.armor = player->armor;
        s.healthLow = player->health <= 30;
        s.healthAlpha = s.healthLow ? 0.6f + 0.4f * std::sin(realTime * 8.0f) : 1.0f;
        const int weapon = static_cast<int>(player->weapon);
        const WeaponDef &def = GetWeaponDef(player->weapon);
        s.weaponName = def.name;
        s.weaponColor = Hex(def.color);
        s.ammo = player->ammo[weapon];
        s.slots.resize(WEAPON_COUNT);
        for (int w = 0; w < WEAPON_COUNT; ++w)
        {
            UiWeaponSlot &slot = s.slots[static_cast<size_t>(w)];
            const auto id = static_cast<WeaponId>(w);
            slot.owned = player->hasWeapon[w];
            slot.selected = w == weapon;
            slot.label = std::to_string(w + 1) + " " + ShortWeaponName(id);
            slot.color = slot.owned ? Hex(GetWeaponDef(id).color) : "#6e6e78";
            slot.ammo = slot.owned ? std::to_string(player->ammo[w]) : "-";
        }
    }

    const int seconds = static_cast<int>(std::ceil(world.match.timeRemaining));
    char timer[16];
    std::snprintf(timer, sizeof(timer), "%d:%02d", seconds / 60, seconds % 60);
    s.timer = timer;
    s.timerLow = seconds < 30;
    s.matchFragLimit = world.settings.fragLimit;

    // Placement and lead over the best other player.
    const std::vector<int> ranked = RankedPlayers();
    s.hasRank = player != nullptr;
    if (player != nullptr)
    {
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
        s.rank = Ordinal(rank);
        s.rankFirst = rank == 1;
        s.rankOf = "of " + std::to_string(ranked.size());
        s.fragsText = std::to_string(player->frags) + " frags";
        s.lead.clear();
        if (best_other > -1000)
        {
            const int lead = player->frags - best_other;
            s.lead = lead >= 0 ? "+" + std::to_string(lead) : std::to_string(lead);
            s.leadGood = lead >= 0;
        }
    }

    s.feed.clear();
    for (const FeedEntry &entry : feed)
    {
        UiFeedEntry item;
        const Float age = realTime - entry.time;
        item.alpha = math::Clamp((FEED_SECONDS - age) / 0.8f, 0.0f, 1.0f);
        item.suicide = entry.killer == entry.victim;
        item.involved = entry.killer == session.localPlayer || entry.victim == session.localPlayer;
        item.killer = entry.killerName;
        item.killerColor = Hex(entry.killerColor);
        item.victim = entry.victimName;
        item.victimColor = Hex(entry.victimColor);
        item.weapon = std::string("  [") + ShortWeaponName(entry.weapon) + (entry.headshot ? " HS" : "") + "]  ";
        s.feed.push_back(item);
    }

    s.status.clear();
    if (session.mode == net::SessionMode::HOST && net::UsesRoomCodes())
    {
        s.status = "ROOM " + roomCode + "  -  " + std::to_string(session.RemotePlayerCount(world)) +
                   " friend(s)  -  Esc for invite link";
    }
    else if (session.mode == net::SessionMode::HOST)
    {
        s.status = "HOSTING ON PORT " + roomCode + "  -  " + std::to_string(session.RemotePlayerCount(world)) +
                   " friend(s)";
    }
    else if (session.mode == net::SessionMode::CLIENT)
    {
        s.status = "ONLINE  -  " + session.address;
    }
    s.notices.clear();
    for (const auto &[message, time] : notices)
    {
        s.notices.push_back({message, math::Clamp((6.0f - (realTime - time)) / 0.8f, 0.0f, 1.0f)});
    }

    s.bannerAlpha = realTime < banner.until ? math::Clamp((banner.until - realTime) / 0.5f, 0.0f, 1.0f) : 0.0f;
    s.banner = banner.text;
    s.bannerSub = banner.subtext;
    s.bannerColor = Hex(banner.color);
    s.pickupAlpha =
        realTime < pickupBanner.until ? math::Clamp((pickupBanner.until - realTime) / 0.5f, 0.0f, 1.0f) : 0.0f;
    s.pickup = pickupBanner.text;
    s.pickupColor = Hex(pickupBanner.color);

    // Scoreboard (Tab, and at the end of the match).
    s.scoreboard = showScoreboard && !menuOpen;
    if (s.scoreboard)
    {
        s.scoreTitle = "SCOREBOARD";
        if (world.match.phase == MatchPhase::ENDED)
        {
            s.scoreTitle = world.match.winner >= 0 ? PlayerName(world.match.winner) + " WINS" : "MATCH OVER";
        }
        s.scores.clear();
        int rank = 1;
        for (const int id : ranked)
        {
            const Player &p = world.players[id];
            UiScoreRow row;
            row.rank = rank++;
            row.name = p.name;
            row.color = Hex(PlayerColor(p.colorIndex));
            row.local = IsLocal(id);
            row.frags = p.frags;
            row.deaths = p.deaths;
            if (p.control == PlayerControl::BOT)
            {
                row.tag = "BOT";
            }
            else if (id == 0 && session.mode != net::SessionMode::OFFLINE)
            {
                row.tag = "HOST";
            }
            s.scores.push_back(row);
        }
    }

    // Pause menu
    s.pause = menuOpen;
    if (menuOpen)
    {
        s.pauseTitle = os::IsBrowser() ? "CLICK THE ARENA TO PLAY" : "PAUSED";
        s.isHost = session.mode == net::SessionMode::HOST;
        s.roomCodeText = net::UsesRoomCodes() ? "Room code: " + roomCode : roomCode;
        s.inviteLink = s.isHost && net::UsesRoomCodes() ? bridge::InviteLink(roomCode) : "";
        s.friends = std::to_string(session.RemotePlayerCount(world)) + " friend(s) connected";
        s.connectedTo = session.mode == net::SessionMode::CLIENT ? "Connected to " + session.address : "";
        s.showBots = session.mode == net::SessionMode::OFFLINE || session.mode == net::SessionMode::HOST;
        s.showResume = !os::IsBrowser();
    }
}
} // namespace evo
