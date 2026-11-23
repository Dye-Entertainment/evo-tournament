#include "game/game_scene.h"
#include "game/bot.h"
#include "platform_bridge.h"

#include "input/keyboard.h"
#include "input/manager.h"
#include "input/mouse.h"
#include "input/mouse_capture.h"
#include "os/browser.h"
#include "window/window_manager.h"

#include "compiled/warning_off.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "compiled/warning_on.h"

namespace evo
{
namespace
{
constexpr Float MAX_DELTA = 0.05f;
constexpr Float LOOK_SCALE = 0.0022f;
constexpr Float DOUBLE_TAP_WINDOW = 0.25f;
constexpr Float MAX_PITCH = 1.5f;
constexpr int DEMO_BOTS = 6;

enum KeySlot
{
    KEY_W,
    KEY_A,
    KEY_S,
    KEY_D,
    KEY_SPACE,
    KEY_SHIFT,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_Q,
    KEY_TAB,
    KEY_ESCAPE,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_COUNT
};

const input::KeyboardKey KEY_MAP[KEY_COUNT] = {
    input::KeyboardKey::W,     input::KeyboardKey::A,      input::KeyboardKey::S,
    input::KeyboardKey::D,     input::KeyboardKey::Space,  input::KeyboardKey::LShift,
    input::KeyboardKey::Num1,  input::KeyboardKey::Num2,   input::KeyboardKey::Num3,
    input::KeyboardKey::Q,     input::KeyboardKey::Tab,    input::KeyboardKey::Escape,
    input::KeyboardKey::Up,    input::KeyboardKey::Down,   input::KeyboardKey::Left,
    input::KeyboardKey::Right,
};

Float *globalWheel = nullptr;

Float HorizontalToVerticalFov(const Float horizontal_degrees, const Float aspect)
{
    const Float horizontal = horizontal_degrees * math::PI / 180.0f;
    return 2.0f * std::atan(std::tan(horizontal * 0.5f) / aspect) * 180.0f / math::PI;
}

Sound PickupSound(const PickupKind kind)
{
    switch (kind)
    {
    case PickupKind::HEALTH:
    case PickupKind::MEGA_HEALTH:
        return Sound::PICKUP_HEALTH;
    case PickupKind::ARMOR:
        return Sound::PICKUP_ARMOR;
    case PickupKind::WEAPON_NOVA:
    case PickupKind::WEAPON_RAIL:
        return Sound::PICKUP_WEAPON;
    default:
        return Sound::PICKUP_AMMO;
    }
}

} // namespace

FVec3 PickupColor(const PickupKind kind)
{
    switch (kind)
    {
    case PickupKind::HEALTH:
        return {1.0f, 0.25f, 0.25f};
    case PickupKind::MEGA_HEALTH:
        return {0.3f, 0.6f, 1.0f};
    case PickupKind::ARMOR:
        return {1.0f, 0.8f, 0.2f};
    case PickupKind::WEAPON_NOVA:
    case PickupKind::AMMO_NOVA:
        return GetWeaponDef(WeaponId::NOVA).color;
    case PickupKind::WEAPON_RAIL:
    case PickupKind::AMMO_RAIL:
        return GetWeaponDef(WeaponId::RAIL).color;
    default:
        return GetWeaponDef(WeaponId::RIFLE).color;
    }
}

GameScene::GameScene()
{
    LoadOptions();
    world.Initialize(options.match);
    rendererReady = renderer.Initialize(world.arena);
    sfx.Initialize();
    sfx.SetMasterVolume(options.volume);
    joinAddress = bridge::JoinCodeFromUrl();
    if (joinAddress.empty() && !net::UsesRoomCodes())
    {
        joinAddress = "127.0.0.1:" + net::DefaultHostAddress();
    }
    hostAddress = net::DefaultHostAddress();
    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", options.name.c_str());
    std::snprintf(joinBuffer, sizeof(joinBuffer), "%s", joinAddress.c_str());
    std::snprintf(hostBuffer, sizeof(hostBuffer), "%s", hostAddress.c_str());
    StartDemo();
}

GameScene::~GameScene()
{
    session.Shutdown(world);
    globalWheel = nullptr;
}

void GameScene::LoadOptions()
{
    char default_name[32];
    std::snprintf(
        default_name, sizeof(default_name), "Player%03u",
        math::GetHash(static_cast<UInt32>(SDL_GetTicks())) % 1000
    );
    options.name = bridge::LoadSetting("name", default_name);
    options.sensitivity = std::strtof(bridge::LoadSetting("sensitivity", "1.0").c_str(), nullptr);
    options.fieldOfView = std::strtof(bridge::LoadSetting("fov", "100").c_str(), nullptr);
    options.volume = std::strtof(bridge::LoadSetting("volume", "0.8").c_str(), nullptr);
    options.invertY = bridge::LoadSetting("invertY", "0") == "1";
    options.match.botCount = std::atoi(bridge::LoadSetting("bots", "4").c_str());
    options.match.botSkill = std::strtof(bridge::LoadSetting("botSkill", "0.3").c_str(), nullptr);
    options.match.fragLimit = static_cast<Int16>(std::atoi(bridge::LoadSetting("fragLimit", "15").c_str()));
    options.match.timeLimitSeconds = std::strtof(bridge::LoadSetting("timeLimit", "480").c_str(), nullptr);

    options.sensitivity = math::Clamp(options.sensitivity, 0.1f, 5.0f);
    options.fieldOfView = math::Clamp(options.fieldOfView, 80.0f, 120.0f);
    options.volume = math::Clamp(options.volume, 0.0f, 1.0f);
    options.match.botCount = std::clamp(options.match.botCount, 0, MAX_PLAYERS - 1);
    options.match.botSkill = math::Clamp(options.match.botSkill, 0.0f, 1.0f);
    options.match.fragLimit = std::clamp<Int16>(options.match.fragLimit, 5, 100);
    options.match.timeLimitSeconds = math::Clamp(options.match.timeLimitSeconds, 120.0f, 1800.0f);
}

void GameScene::SaveOptions() const
{
    bridge::SaveSetting("name", options.name);
    bridge::SaveSetting("sensitivity", std::to_string(options.sensitivity));
    bridge::SaveSetting("fov", std::to_string(options.fieldOfView));
    bridge::SaveSetting("volume", std::to_string(options.volume));
    bridge::SaveSetting("invertY", options.invertY ? "1" : "0");
    bridge::SaveSetting("bots", std::to_string(options.match.botCount));
    bridge::SaveSetting("botSkill", std::to_string(options.match.botSkill));
    bridge::SaveSetting("fragLimit", std::to_string(options.match.fragLimit));
    bridge::SaveSetting("timeLimit", std::to_string(options.match.timeLimitSeconds));
}

void GameScene::EnsureImGui()
{
    if (imguiReady || ImGui::GetCurrentContext() == nullptr)
    {
        return;
    }
    imguiReady = true;
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    const std::string font_path = std::string(EVO_RESOURCE_DIR) + "/fonts/DejaVuSans-Bold.ttf";
    font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 17.0f);
    if (font != nullptr)
    {
        io.FontDefault = font;
    }

    ImGuiStyle &style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);
    style.WindowRounding = 10.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.WindowPadding = ImVec2(22.0f, 18.0f);
    style.FramePadding = ImVec2(10.0f, 7.0f);
    style.ItemSpacing = ImVec2(10.0f, 9.0f);
    style.WindowBorderSize = 1.0f;
    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.09f, 0.92f);
    colors[ImGuiCol_Border] = ImVec4(1.0f, 0.55f, 0.15f, 0.35f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.19f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.20f, 0.26f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.24f, 0.30f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.85f, 0.42f, 0.10f, 0.85f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.55f, 0.18f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.75f, 0.35f, 0.05f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.55f, 0.15f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.7f, 0.3f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.30f, 0.20f, 0.14f, 0.8f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.45f, 0.28f, 0.16f, 0.9f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.55f, 0.32f, 0.16f, 1.0f);
    colors[ImGuiCol_Separator] = ImVec4(1.0f, 0.55f, 0.15f, 0.3f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.10f, 0.14f, 1.0f);

    globalWheel = &wheel;
    window::Manager::GetInstance()->AddWindowMessageListener(
        [](const window::WindowMessage message)
        {
            if (ImGui::GetCurrentContext() == nullptr)
            {
                return;
            }
            const auto *event = static_cast<const SDL_Event *>(message.backendData);
            ImGui_ImplSDL3_ProcessEvent(event);
            if (event->type == SDL_EVENT_MOUSE_WHEEL && globalWheel != nullptr)
            {
                *globalWheel += event->wheel.y;
            }
        }
    );
}

InputFrame GameScene::ReadInput(const Float delta_seconds)
{
    InputFrame frame;
    std::array<Bool, KEY_COUNT> keys{};
    if (auto keyboard = input::Manager::GetInstance()->GetKeyboard(); keyboard.has_value())
    {
        for (int i = 0; i < KEY_COUNT; ++i)
        {
            keys[i] = input::IsKeyDown(*keyboard.value(), KEY_MAP[i]);
        }
    }
    const Bool typing = imguiReady && ImGui::GetIO().WantTextInput;
    if (typing)
    {
        keys = {};
    }
    const auto pressed = [&](const int slot) { return keys[slot] && !previousKeys[slot]; };

    Bool fire = false;
    if (auto mouse = input::Manager::GetInstance()->GetMouse(0); mouse.has_value())
    {
        fire = input::isButtonDown(*mouse.value(), input::MouseButton::Left);
    }

    Float dx = 0.0f;
    Float dy = 0.0f;
    input::TakeMouseDelta(dx, dy);
    frame.lookX = dx;
    frame.lookY = dy;

    frame.forward = (keys[KEY_W] || keys[KEY_UP] ? 1.0f : 0.0f) - (keys[KEY_S] || keys[KEY_DOWN] ? 1.0f : 0.0f);
    frame.right = (keys[KEY_D] || keys[KEY_RIGHT] ? 1.0f : 0.0f) - (keys[KEY_A] || keys[KEY_LEFT] ? 1.0f : 0.0f);
    frame.jump = pressed(KEY_SPACE);
    frame.fireHeld = fire;
    frame.firePressed = fire && !previousFire;
    frame.scoreboard = keys[KEY_TAB];
    frame.escape = pressed(KEY_ESCAPE);
    frame.lastWeapon = pressed(KEY_Q);
    if (pressed(KEY_1))
    {
        frame.selectWeapon = 0;
    }
    else if (pressed(KEY_2))
    {
        frame.selectWeapon = 1;
    }
    else if (pressed(KEY_3))
    {
        frame.selectWeapon = 2;
    }
    if (wheel >= 1.0f || wheel <= -1.0f)
    {
        frame.cycleWeapon = wheel > 0.0f ? -1 : 1;
        wheel = 0.0f;
    }

    const int tap_keys[4] = {KEY_W, KEY_S, KEY_A, KEY_D};
    const DodgeDirection tap_directions[4] = {DodgeDirection::FORWARD, DodgeDirection::BACK, DodgeDirection::LEFT,
                                              DodgeDirection::RIGHT};
    for (int i = 0; i < 4; ++i)
    {
        if (!pressed(tap_keys[i]))
        {
            continue;
        }
        if (realTime - lastTapTime[i] < DOUBLE_TAP_WINDOW)
        {
            frame.dodge = tap_directions[i];
            lastTapTime[i] = -10.0f;
        }
        else
        {
            lastTapTime[i] = realTime;
        }
    }
    if (pressed(KEY_SHIFT) && frame.dodge == DodgeDirection::NONE)
    {
        if (std::fabs(frame.right) > std::fabs(frame.forward))
        {
            frame.dodge = frame.right > 0.0f ? DodgeDirection::RIGHT : DodgeDirection::LEFT;
        }
        else
        {
            frame.dodge = frame.forward < 0.0f ? DodgeDirection::BACK : DodgeDirection::FORWARD;
        }
    }

    for (int i = 0; i < KEY_COUNT; ++i)
    {
        previousKeys[i] = keys[i];
    }
    previousFire = fire;
    static_cast<void>(delta_seconds);
    return frame;
}

void GameScene::StartDemo()
{
    session.Shutdown(world);
    MatchSettings demo;
    demo.botCount = DEMO_BOTS;
    demo.botSkill = 0.55f;
    demo.fragLimit = 1000;
    demo.timeLimitSeconds = 100000.0f;
    world.Initialize(demo);
    for (int i = 0; i < DEMO_BOTS; ++i)
    {
        world.AddPlayer(BotName(i), PlayerControl::BOT, -1);
    }
    world.TakeNewEvents();
    effects.Clear();
    screen = Screen::MAIN_MENU;
    menuOpen = false;
}

void GameScene::EnterMatch()
{
    world.TakeNewEvents();
    effects.Clear();
    feed.clear();
    notices.clear();
    banner = Banner{};
    pickupBanner = Banner{};
    hitMarker = 0.0f;
    damageFlash = 0.0f;
    fireArmed = false;
    menuOpen = false;
    wasCaptured = false;
    screen = Screen::IN_MATCH;
    SaveOptions();
    if (const Player *player = LocalPlayer())
    {
        cameraYaw = player->yaw;
    }
}

void GameScene::StartOffline()
{
    session.StartOffline(world, options.match, options.name);
    EnterMatch();
    banner = Banner{"INSTANT ACTION", "First to " + std::to_string(options.match.fragLimit) + " frags wins",
                    {1.0f, 0.6f, 0.2f}, realTime + 3.0f};
}

void GameScene::StartHost()
{
    roomCode = net::UsesRoomCodes() ? net::DefaultHostAddress() : hostAddress;
    if (!session.StartHost(world, options.match, options.name, roomCode))
    {
        errorMessage = session.failureReason;
        StartDemo();
        return;
    }
    EnterMatch();
    banner = Banner{"MATCH HOSTED", net::UsesRoomCodes() ? "Room code " + roomCode : "UDP port " + roomCode,
                    {0.4f, 0.9f, 1.0f}, realTime + 4.0f};
}

void GameScene::StartJoin()
{
    std::string address = joinAddress;
    address.erase(std::remove(address.begin(), address.end(), ' '), address.end());
    if (address.empty())
    {
        errorMessage = net::UsesRoomCodes() ? "Enter a room code first" : "Enter a host address first";
        return;
    }
    if (net::UsesRoomCodes())
    {
        std::transform(address.begin(), address.end(), address.begin(), ::toupper);
    }
    SaveOptions();
    if (!session.StartClient(world, options.name, address))
    {
        errorMessage = session.failureReason;
        StartDemo();
        return;
    }
    effects.Clear();
    screen = Screen::CONNECTING;
}

void GameScene::LeaveMatch(const std::string &reason)
{
    errorMessage = reason;
    StartDemo();
}

Bool GameScene::IsLocal(const int player_id) const
{
    return screen == Screen::IN_MATCH && player_id >= 0 && player_id == session.localPlayer;
}

const Player *GameScene::LocalPlayer() const
{
    if (screen != Screen::IN_MATCH || session.localPlayer < 0)
    {
        return nullptr;
    }
    const Player &player = world.players[session.localPlayer];
    return player.active ? &player : nullptr;
}

std::string GameScene::PlayerName(const int player_id) const
{
    if (player_id < 0 || player_id >= MAX_PLAYERS)
    {
        return "World";
    }
    const std::string &name = world.players[player_id].name;
    return name.empty() ? "Player" : name;
}

std::vector<int> GameScene::RankedPlayers() const
{
    std::vector<int> ranked;
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (world.players[i].active)
        {
            ranked.push_back(i);
        }
    }
    std::stable_sort(ranked.begin(), ranked.end(),
                     [this](const int a, const int b)
                     {
                         const Player &pa = world.players[a];
                         const Player &pb = world.players[b];
                         if (pa.frags != pb.frags)
                         {
                             return pa.frags > pb.frags;
                         }
                         return pa.deaths < pb.deaths;
                     });
    return ranked;
}

void GameScene::Update(const double elapsed_seconds)
{
    const Float delta_seconds =
        math::Clamp(static_cast<Float>(elapsed_seconds), 0.0005f, MAX_DELTA);
    realTime += delta_seconds;
    EnsureImGui();
    const InputFrame input = ReadInput(delta_seconds);

    if (screen == Screen::MAIN_MENU && !autoStartDone && realTime > 1.0f)
    {
        autoStartDone = true;
        if (const Char *mode = std::getenv("EVO_AUTOSTART"); mode != nullptr)
        {
            if (std::string(mode) == "offline")
            {
                StartOffline();
            }
            else if (std::string(mode) == "host")
            {
                StartHost();
            }
            else if (std::string(mode).rfind("join:", 0) == 0)
            {
                joinAddress = std::string(mode).substr(5);
                StartJoin();
            }
        }
    }
    if (screen == Screen::MAIN_MENU)
    {
        world.HostTick(delta_seconds, MoveInput{}, FireRequest{});
    }
    else if (screen == Screen::CONNECTING)
    {
        session.Update(world, delta_seconds);
        if (session.state == net::SessionState::ACTIVE)
        {
            EnterMatch();
            banner = Banner{"CONNECTED", "Joined room " + joinAddress, {0.4f, 0.9f, 1.0f}, realTime + 3.0f};
        }
        else if (session.state == net::SessionState::FAILED)
        {
            LeaveMatch(session.failureReason);
        }
    }
    else
    {
        UpdateMatch(delta_seconds, input);
    }

    ProcessEvents();
    effects.Update(delta_seconds, world.arena);
    UpdateRenderState(delta_seconds);
    UpdateCamera(delta_seconds);
    sfx.SetListener(cameraEye, cameraYaw);
    UpdateCapture(input);

    hitMarker = std::max(0.0f, hitMarker - delta_seconds);
    damageFlash = std::max(0.0f, damageFlash - delta_seconds * 1.5f);
    shake = std::max(0.0f, shake - delta_seconds * 2.5f);
    recoil = std::max(0.0f, recoil - delta_seconds * 6.0f);
    landDip = std::max(0.0f, landDip - delta_seconds * 1.2f);
    while (!feed.empty() && realTime - feed.front().time > FEED_SECONDS)
    {
        feed.pop_front();
    }
    while (!notices.empty() && realTime - notices.front().second > 6.0f)
    {
        notices.pop_front();
    }
}

void GameScene::UpdateMatch(const Float delta_seconds, const InputFrame &input)
{
    showScoreboard = input.scoreboard || world.match.phase == MatchPhase::ENDED;
    for (const std::string &message : session.TakeMessages())
    {
        notices.emplace_back(message, realTime);
    }

    if (session.mode == net::SessionMode::CLIENT)
    {
        session.Update(world, delta_seconds);
        if (session.state == net::SessionState::FAILED)
        {
            LeaveMatch(session.failureReason);
            return;
        }
        UpdateLocalPlayerClient(delta_seconds, input);
    }
    else
    {
        UpdateLocalPlayerHost(delta_seconds, input);
        session.Update(world, delta_seconds);
    }
    PlayLocalMoveSounds();
}

void GameScene::ApplyLook(Player &player, const InputFrame &input) const
{
    if (menuOpen)
    {
        return;
    }
    const Float scale = LOOK_SCALE * options.sensitivity;
    player.yaw = math::GetWrappedAngle(player.yaw + input.lookX * scale);
    player.pitch = math::Clamp(
        player.pitch - input.lookY * scale * (options.invertY ? -1.0f : 1.0f), -MAX_PITCH, MAX_PITCH
    );
}

void GameScene::SelectWeapon(Player &player, const WeaponId weapon)
{
    const int index = static_cast<int>(weapon);
    if (!player.hasWeapon[index] || player.weapon == weapon)
    {
        return;
    }
    previousWeapon = player.weapon;
    player.weapon = weapon;
    player.fireCooldown = std::max(player.fireCooldown, 0.15f);
    sfx.Play(Sound::SWITCH, 0.6f);
}

Bool GameScene::AutoSwitchIfEmpty(Player &player)
{
    if (player.ammo[static_cast<int>(player.weapon)] > 0)
    {
        return false;
    }
    const WeaponId order[] = {WeaponId::RAIL, WeaponId::NOVA, WeaponId::RIFLE};
    for (const WeaponId weapon : order)
    {
        const int index = static_cast<int>(weapon);
        if (player.hasWeapon[index] && player.ammo[index] > 0)
        {
            SelectWeapon(player, weapon);
            return true;
        }
    }
    return false;
}

void GameScene::HandleWeaponInput(Player &player, const InputFrame &input)
{
    if (input.selectWeapon >= 0)
    {
        SelectWeapon(player, static_cast<WeaponId>(input.selectWeapon));
    }
    if (input.lastWeapon)
    {
        SelectWeapon(player, previousWeapon);
    }
    if (input.cycleWeapon != 0)
    {
        int index = static_cast<int>(player.weapon);
        for (int step = 0; step < WEAPON_COUNT; ++step)
        {
            index = (index + input.cycleWeapon + WEAPON_COUNT) % WEAPON_COUNT;
            if (player.hasWeapon[index])
            {
                SelectWeapon(player, static_cast<WeaponId>(index));
                break;
            }
        }
    }
    if (input.fireHeld && fireArmed && player.alive &&
        player.ammo[static_cast<int>(player.weapon)] <= 0 && player.fireCooldown <= 0.0f)
    {
        if (!AutoSwitchIfEmpty(player))
        {
            sfx.Play(Sound::EMPTY, 0.6f);
            player.fireCooldown = 0.4f;
        }
    }
}

void GameScene::UpdateLocalPlayerHost(const Float delta_seconds, const InputFrame &input)
{
    MoveInput move;
    FireRequest fire;
    if (session.localPlayer >= 0)
    {
        Player &player = world.players[session.localPlayer];
        ApplyLook(player, input);
        if (!menuOpen)
        {
            HandleWeaponInput(player, input);
        }
        move.yaw = player.yaw;
        if (!menuOpen && player.alive)
        {
            move.forward = input.forward;
            move.right = input.right;
            move.jump = input.jump;
            move.dodge = input.dodge;
            if (input.fireHeld && fireArmed)
            {
                fire.fire = true;
                fire.origin = EyePosition(player.move);
                fire.direction = world.ApplySpread(
                    player, math::GetDirectionFromYawPitch(player.yaw, player.pitch),
                    world.random.Next()
                );
            }
        }
        if (!player.alive && input.firePressed && fireArmed && !menuOpen)
        {
            player.wantsRespawn = true;
        }
    }
    world.localMoveEvents = MoveEvents{};
    world.HostTick(delta_seconds, move, fire);
}

void GameScene::UpdateLocalPlayerClient(const Float delta_seconds, const InputFrame &input)
{
    world.localMoveEvents = MoveEvents{};
    for (Projectile &projectile : world.projectiles)
    {
        projectile.position = projectile.position + projectile.velocity * delta_seconds;
    }
    if (session.localPlayer < 0)
    {
        return;
    }
    Player &player = world.players[session.localPlayer];
    player.fireCooldown = std::max(0.0f, player.fireCooldown - delta_seconds);
    ApplyLook(player, input);
    respawnRequestTimer = std::max(0.0f, respawnRequestTimer - delta_seconds);
    if (!player.alive)
    {
        if (input.firePressed && fireArmed && !menuOpen && respawnRequestTimer <= 0.0f)
        {
            session.SendRespawnRequest();
            respawnRequestTimer = 0.3f;
        }
        return;
    }
    if (!menuOpen)
    {
        HandleWeaponInput(player, input);
    }
    MoveInput move;
    move.yaw = player.yaw;
    if (!menuOpen)
    {
        move.forward = input.forward;
        move.right = input.right;
        move.jump = input.jump;
        move.dodge = input.dodge;
    }
    MoveEvents events;
    StepMovement(player.move, move, delta_seconds, world.arena, events);
    world.localMoveEvents = events;

    if (!menuOpen && input.fireHeld && fireArmed && world.CanFire(player))
    {
        world.ConsumeShot(player);
        const FVec3 origin = EyePosition(player.move);
        const FVec3 direction = world.ApplySpread(
            player, math::GetDirectionFromYawPitch(player.yaw, player.pitch), world.random.Next()
        );
        session.SendFire(player.weapon, origin, direction);
        const WeaponDef &def = GetWeaponDef(player.weapon);
        if (def.hitscan)
        {
            const World::TraceResult trace = world.Trace(origin, direction, 250.0f, session.localPlayer);
            HandleShot(session.localPlayer, player.weapon, origin, trace.end);
        }
        else
        {
            HandleShot(session.localPlayer, player.weapon, origin, origin + direction);
        }
    }
}

void GameScene::PlayLocalMoveSounds()
{
    const MoveEvents &events = world.localMoveEvents;
    if (events.jumped)
    {
        sfx.Play(Sound::JUMP, 0.5f);
    }
    if (events.doubleJumped)
    {
        sfx.Play(Sound::JUMP, 0.6f);
    }
    if (events.dodged)
    {
        sfx.Play(Sound::DODGE, 0.7f);
    }
    if (events.launched)
    {
        sfx.Play(Sound::PAD, 0.7f);
    }
    if (events.landingSpeed > 6.0f)
    {
        sfx.Play(Sound::LAND, math::Clamp(events.landingSpeed / 18.0f, 0.2f, 0.9f));
        landDip = math::Clamp(events.landingSpeed / 60.0f, 0.0f, 0.25f);
    }
}

void GameScene::ProcessEvents()
{
    for (const GameEvent &event : world.TakeNewEvents())
    {
        HandleEvent(event);
    }
}

void GameScene::HandleShot(const int shooter, const WeaponId weapon, const FVec3 &origin, const FVec3 &end)
{
    const Bool local = IsLocal(shooter);
    const Bool demo = screen != Screen::IN_MATCH;
    FVec3 muzzle = origin;
    if (local)
    {
        muzzle = ViewmodelMuzzle();
        recoil = weapon == WeaponId::RIFLE ? 0.25f : 1.0f;
    }
    else if (shooter >= 0 && shooter < MAX_PLAYERS)
    {
        Player &player = world.players[shooter];
        player.fireFlash = 0.08f;
        const FVec3 forward = math::GetDirectionFromYawPitch(player.renderYaw, player.renderPitch);
        muzzle = player.renderPosition + FVec3{0.0f, 1.3f, 0.0f} +
                 (RightXZ(player.renderYaw) * 0.28f + forward * 0.7f);
    }

    const WeaponDef &def = GetWeaponDef(weapon);
    Sound sound = Sound::RIFLE;
    if (weapon == WeaponId::RIFLE)
    {
        effects.AddBeam(muzzle, end, def.color, 0.035f, 0.07f);
        effects.AddSparks(end, math::GetNormalized(origin - end), def.color, 3);
    }
    else if (weapon == WeaponId::RAIL)
    {
        effects.AddBeam(muzzle, end, def.color, 0.16f, 0.8f);
        effects.AddBeam(muzzle, end, {1.0f, 0.9f, 1.0f}, 0.05f, 0.35f);
        effects.AddSparks(end, math::GetNormalized(origin - end), def.color, 12);
        sound = Sound::RAIL;
    }
    else
    {
        sound = Sound::NOVA_FIRE;
    }
    effects.AddMuzzleFlash(muzzle, def.color);
    if (demo)
    {
        return;
    }
    if (local)
    {
        sfx.Play(sound, 0.7f);
    }
    else
    {
        sfx.PlayAt(sound, origin, 0.9f);
    }
}

void GameScene::HandleEvent(const GameEvent &event)
{
    const Bool demo = screen != Screen::IN_MATCH;
    const int local = session.localPlayer;
    switch (event.type)
    {
    case EventType::SHOT:
        HandleShot(event.a, static_cast<WeaponId>(event.c % WEAPON_COUNT), event.p0, event.p1);
        break;
    case EventType::EXPLOSION:
    {
        effects.AddExplosion(event.p0);
        if (!demo)
        {
            sfx.PlayAt(Sound::EXPLOSION, event.p0, 1.0f);
            const Float distance = math::GetLength(event.p0 - cameraEye);
            shake = std::max(shake, math::Clamp(1.0f - distance / 12.0f, 0.0f, 1.0f) * 0.6f);
        }
        break;
    }
    case EventType::KILL:
    {
        const int killer = event.a == 255 ? -1 : event.a;
        const int victim = event.b;
        const auto weapon = static_cast<WeaponId>((event.c & 0x7F) % WEAPON_COUNT);
        const Bool headshot = (event.c & 0x80) != 0;
        const FVec3 color = victim < MAX_PLAYERS ? PlayerColor(world.players[victim].colorIndex) : FVec3{1, 1, 1};
        effects.AddGibs(event.p0, event.p1, color);
        if (victim < MAX_PLAYERS)
        {
            world.players[victim].alive = false;
        }
        if (demo)
        {
            break;
        }
        sfx.PlayAt(Sound::DEATH, event.p0, 0.9f);
        FeedEntry entry;
        entry.killer = killer;
        entry.victim = victim;
        entry.killerName = PlayerName(killer);
        entry.victimName = PlayerName(victim);
        entry.killerColor = killer >= 0 ? PlayerColor(world.players[killer].colorIndex) : FVec3{1, 1, 1};
        entry.victimColor = color;
        entry.weapon = weapon;
        entry.headshot = headshot;
        entry.time = realTime;
        feed.push_back(entry);
        while (feed.size() > 6)
        {
            feed.pop_front();
        }
        if (victim == local && local >= 0)
        {
            deathPosition = event.p0;
            deathYaw = cameraYaw;
            deathStartTime = realTime;
            const std::string by = killer == victim ? "You fragged yourself" : "Fragged by " + PlayerName(killer);
            banner = Banner{by, "", {1.0f, 0.3f, 0.25f}, realTime + 3.0f};
        }
        else if (killer == local && local >= 0)
        {
            sfx.Play(Sound::FRAG, 0.7f);
            banner = Banner{"You fragged " + PlayerName(victim), headshot ? "HEADSHOT" : "",
                            {1.0f, 0.85f, 0.3f}, realTime + 2.0f};
        }
        break;
    }
    case EventType::HIT:
    {
        const int attacker = event.a == 255 ? -1 : event.a;
        if (!(event.b == local && local >= 0 && !demo))
        {
            effects.AddSparks(event.p0, {0.0f, 1.0f, 0.0f}, {0.8f, 0.05f, 0.05f}, 5);
        }
        if (demo)
        {
            break;
        }
        if (attacker == local && event.b != local && local >= 0)
        {
            hitMarker = 0.22f;
            sfx.Play(Sound::HIT, 0.5f);
        }
        if (event.b == local && local >= 0)
        {
            damageFlash = std::min(1.0f, damageFlash + static_cast<Float>(event.c) / 60.0f);
            sfx.Play(Sound::HURT, 0.7f);
        }
        break;
    }
    case EventType::PICKUP:
    {
        const auto kind = static_cast<PickupKind>(event.b);
        effects.AddPickupBurst(event.p0, PickupColor(kind));
        if (demo)
        {
            break;
        }
        if (event.a == local && local >= 0)
        {
            sfx.Play(PickupSound(kind), 0.75f);
            pickupBanner = Banner{std::string("+ ") + PickupName(kind), "", PickupColor(kind), realTime + 2.0f};
        }
        else
        {
            sfx.PlayAt(PickupSound(kind), event.p0, 0.4f);
        }
        break;
    }
    case EventType::SPAWN:
    {
        const FVec3 color = event.a < MAX_PLAYERS ? PlayerColor(world.players[event.a].colorIndex) : FVec3{1, 1, 1};
        effects.AddSpawnColumn(event.p0, color);
        if (event.a < MAX_PLAYERS)
        {
            world.players[event.a].renderPosition = event.p0;
        }
        if (!demo)
        {
            sfx.PlayAt(Sound::SPAWN, event.p0, event.a == local ? 0.8f : 0.5f);
        }
        break;
    }
    case EventType::MATCH_END:
        if (!demo)
        {
            sfx.Play(Sound::MATCH_END, 0.8f);
            const int winner = event.a == 255 ? -1 : event.a;
            banner = Banner{winner == local ? "YOU WIN THE MATCH" : PlayerName(winner) + " wins the match",
                            "Next match starts shortly", {1.0f, 0.8f, 0.2f}, realTime + 8.0f};
        }
        break;
    case EventType::MATCH_START:
        if (!demo)
        {
            feed.clear();
            banner = Banner{"FIGHT", "First to " + std::to_string(world.settings.fragLimit) + " frags",
                            {1.0f, 0.6f, 0.2f}, realTime + 2.5f};
        }
        break;
    default:
        break;
    }
}

void GameScene::UpdateRenderState(const Float delta_seconds)
{
    const Bool client = session.mode == net::SessionMode::CLIENT;
    const Float blend = std::min(1.0f, delta_seconds * 16.0f);
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        Player &player = world.players[i];
        if (!player.active)
        {
            continue;
        }
        if (client)
        {
            player.fireFlash = std::max(0.0f, player.fireFlash - delta_seconds);
        }
        if (IsLocal(i))
        {
            player.renderPosition = player.move.position;
            player.renderYaw = player.yaw;
            player.renderPitch = player.pitch;
        }
        else if (client || player.control == PlayerControl::REMOTE)
        {
            const Float age =
                client ? math::Clamp(world.time - player.lastHeard, 0.0f, 0.1f) : 0.0f;
            const FVec3 target = player.move.position + player.move.velocity * age;
            if (math::GetLength(target - player.renderPosition) > 4.0f)
            {
                player.renderPosition = target;
            }
            else
            {
                player.renderPosition = math::GetLerp(player.renderPosition, target, blend);
            }
            player.renderYaw = math::GetWrappedAngle(
                player.renderYaw + math::GetWrappedAngle(player.yaw - player.renderYaw) * blend
            );
            player.renderPitch += (player.pitch - player.renderPitch) * blend;
        }
        else
        {
            player.renderPosition = player.move.position;
            player.renderYaw = player.yaw;
            player.renderPitch = player.pitch;
        }
        if (player.move.onGround)
        {
            player.walkPhase += LengthXZ(player.move.velocity) * delta_seconds * 1.7f;
        }
    }
    for (const Projectile &projectile : world.projectiles)
    {
        effects.AddSparks(
            projectile.position, math::GetNormalized(projectile.velocity) * -1.0f,
            GetWeaponDef(WeaponId::NOVA).color, 1
        );
    }
}

void GameScene::UpdateCamera(const Float delta_seconds)
{
    const Player *player = LocalPlayer();
    if (player == nullptr)
    {
        const Float angle = realTime * 0.08f;
        cameraEye = FVec3{std::sin(angle) * 38.0f, 17.0f, std::cos(angle) * 38.0f};
        math::GetYawPitchFromDirection(FVec3{0.0f, 2.0f, 0.0f} - cameraEye, cameraYaw, cameraPitch);
        return;
    }
    if (player->alive)
    {
        const Float speed = LengthXZ(player->move.velocity);
        if (player->move.onGround && speed > 1.0f)
        {
            bobPhase += delta_seconds * speed * 1.15f;
        }
        const Float bob = player->move.onGround ? std::sin(bobPhase * 2.0f) * 0.035f *
                                                      math::Clamp(speed / 9.5f, 0.0f, 1.0f)
                                                : 0.0f;
        cameraEye = EyePosition(player->move) + FVec3{0.0f, bob - landDip, 0.0f};
        cameraYaw = player->yaw;
        cameraPitch = player->pitch;
    }
    else
    {
        const Float rise = math::Clamp(realTime - deathStartTime, 0.0f, 1.5f);
        cameraEye = deathPosition + FVec3{0.0f, 1.0f + rise * 1.4f, 0.0f};
        cameraYaw = math::GetWrappedAngle(cameraYaw + delta_seconds * 0.25f);
        cameraPitch = -0.55f;
    }
    if (shake > 0.0f)
    {
        cameraEye = cameraEye + FVec3{
                                    world.random.NextRange(-1, 1), world.random.NextRange(-1, 1),
                                    world.random.NextRange(-1, 1)
                                } * (shake * 0.08f);
    }
}

void GameScene::UpdateCapture(const InputFrame &input)
{
    const Bool over_ui = imguiReady && ImGui::GetIO().WantCaptureMouse;
    if (screen != Screen::IN_MATCH)
    {
        input::SetMouseCaptured(false, over_ui);
        wasCaptured = false;
        return;
    }
    if (os::IsBrowser())
    {
        input::SetMouseCaptured(true, over_ui || showScoreboard);
        const Bool captured = input::IsMouseCaptured();
        menuOpen = !captured;
        if (captured && !wasCaptured)
        {
            fireArmed = false;
        }
        wasCaptured = captured;
    }
    else
    {
        if (input.escape)
        {
            menuOpen = !menuOpen;
            fireArmed = false;
        }
        else if (menuOpen && input.firePressed && !over_ui)
        {
            menuOpen = false;
            fireArmed = false;
        }
        input::SetMouseCaptured(!menuOpen, over_ui);
    }
    if (!input.fireHeld && !menuOpen)
    {
        fireArmed = true;
    }
}

void GameScene::Render()
{
    if (!rendererReady)
    {
        return;
    }
    renderer.BeginFrame();
    const IVec2 size = window::Manager::GetInstance()->GetWindowSize(0);
    const Float aspect = size.y > 0 ? static_cast<Float>(size.x) / static_cast<Float>(size.y) : 16.0f / 9.0f;
    renderer.SetCamera(cameraEye, cameraYaw, cameraPitch, HorizontalToVerticalFov(options.fieldOfView, aspect), aspect);
    DrawWorld();
    if (imguiReady)
    {
        DrawUi();
    }
}
} // namespace evo
