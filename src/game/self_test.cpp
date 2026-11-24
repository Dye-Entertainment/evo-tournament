#include "game/self_test.h"
#include "audio/sfx.h"
#include "game/bot.h"
#include "game/world.h"

#include "compiled/warning_off.h"
#include <cstdio>
#include "compiled/warning_on.h"

namespace evo
{
namespace
{
int ValidateWaypoints(const Arena &arena)
{
    int failures = 0;
    const FVec3 lift{0.0f, PLAYER_EYE_HEIGHT, 0.0f};
    for (int i = 0; i < static_cast<int>(arena.waypoints.size()); ++i)
    {
        const FVec3 a = arena.waypoints[i].position;
        if (arena.IsBlocked(PlayerBounds(a + FVec3{0.0f, 0.01f, 0.0f})))
        {
            std::printf("waypoint %d is inside geometry\n", i);
            ++failures;
        }
        for (const int link : arena.waypoints[i].links)
        {
            const FVec3 b = arena.waypoints[link].position;
            const auto &back_links = arena.waypoints[link].links;
            if (std::find(back_links.begin(), back_links.end(), i) == back_links.end())
            {
                continue;
            }
            if (!arena.HasLineOfSight(a + lift, b + lift))
            {
                std::printf("waypoint link %d -> %d has no line of sight\n", i, link);
                ++failures;
            }
        }
    }
    for (const SpawnPoint &spawn : arena.spawnPoints)
    {
        if (arena.IsBlocked(PlayerBounds(spawn.position + FVec3{0.0f, 0.01f, 0.0f})))
        {
            std::printf("spawn at %.1f %.1f %.1f is inside geometry\n", spawn.position.x,
                        spawn.position.y, spawn.position.z);
            ++failures;
        }
    }
    return failures;
}

// A floor with one long wall along +x: jump beside it, run along it, then kick off.
int ValidateWallRun()
{
    Arena arena;
    arena.boxes.push_back(ArenaBox{{FVec3{-50.0f, -1.0f, -50.0f}, FVec3{50.0f, 0.0f, 50.0f}}, FVec3{1.0f}});
    arena.boxes.push_back(ArenaBox{{FVec3{-50.0f, 0.0f, 2.0f}, FVec3{50.0f, 8.0f, 3.0f}}, FVec3{1.0f}});

    constexpr Float STEP = 1.0f / 60.0f;
    MoveState state;
    state.position = FVec3{-30.0f, 0.001f, 2.0f - PLAYER_HALF_WIDTH - 0.01f};
    state.onGround = true;
    MoveInput input;
    input.forward = 1.0f;
    input.yaw = 3.14159265f * 0.5f; // facing +x, wall on the left
    input.allowWallRun = true;

    int failures = 0;
    Bool started = false;
    Float run_seconds = 0.0f;
    Float peak_height = 0.0f;
    for (int frame = 0; frame < 240 && (!state.onGround || frame < 30); ++frame)
    {
        input.jump = frame == 25;
        MoveEvents events;
        StepMovement(state, input, STEP, arena, events);
        started = started || events.wallRunStarted;
        run_seconds += state.wallRunning ? STEP : 0.0f;
        peak_height = std::max(peak_height, state.position.y);
    }
    if (!started || run_seconds < 0.8f)
    {
        std::printf("wall run did not start or ended early (%.2fs)\n", run_seconds);
        ++failures;
    }
    if (run_seconds > 1.5f)
    {
        std::printf("wall run lasted too long (%.2fs)\n", run_seconds);
        ++failures;
    }
    std::printf("wall run %.2f s, peak height %.2f m\n", run_seconds, peak_height);
    if (peak_height < 2.0f)
    {
        std::printf("wall run peaked at only %.2f\n", peak_height);
        ++failures;
    }

    // Kick off: the jump has to push away from the wall and give the double jump back.
    state = MoveState{};
    state.position = FVec3{-30.0f, 0.001f, 2.0f - PLAYER_HALF_WIDTH - 0.01f};
    state.onGround = true;
    Bool kicked = false;
    for (int frame = 0; frame < 120 && !kicked; ++frame)
    {
        input.jump = frame == 25 || (state.wallRunning && state.wallRunTime > 0.3f);
        MoveEvents events;
        StepMovement(state, input, STEP, arena, events);
        if (events.wallJumped)
        {
            kicked = true;
            if (state.velocity.z > -5.0f || state.airJumps != 1 || state.velocity.y < 7.0f)
            {
                std::printf("wall jump had the wrong velocity (%.1f %.1f %.1f)\n", state.velocity.x,
                            state.velocity.y, state.velocity.z);
                ++failures;
            }
        }
    }
    if (!kicked)
    {
        std::printf("wall jump never happened\n");
        ++failures;
    }

    // Without permission (bots) nothing happens.
    state = MoveState{};
    state.position = FVec3{-30.0f, 0.001f, 2.0f - PLAYER_HALF_WIDTH - 0.01f};
    state.onGround = true;
    input.allowWallRun = false;
    for (int frame = 0; frame < 90; ++frame)
    {
        input.jump = frame == 25;
        MoveEvents events;
        StepMovement(state, input, STEP, arena, events);
        if (state.wallRunning)
        {
            std::printf("wall run started without being allowed\n");
            ++failures;
            break;
        }
    }
    return failures;
}
} // namespace

int ValidateSounds()
{
    Sfx sfx;
    if (!sfx.Initialize())
    {
        std::printf("audio device unavailable, sound check skipped\n");
        return 0;
    }
    if (!sfx.AllSoundsLoaded())
    {
        std::printf("some sounds failed to load\n");
        return 1;
    }
    sfx.SetMasterVolume(0.0f);
    for (int sound = 0; sound < static_cast<int>(Sound::COUNT); ++sound)
    {
        sfx.Play(static_cast<Sound>(sound));
        sfx.PlayAt(static_cast<Sound>(sound), FVec3{4.0f, 0.0f, 3.0f});
    }
    return 0;
}

int RunSelfTest()
{
    World world;
    MatchSettings settings;
    settings.botCount = 8;
    settings.fragLimit = 1000;
    settings.timeLimitSeconds = 100000.0f;
    settings.botSkill = 0.6f;
    world.Initialize(settings);

    int failures = ValidateWaypoints(world.arena);
    failures += ValidateSounds();
    failures += ValidateWallRun();

    for (int i = 0; i < settings.botCount; ++i)
    {
        world.AddPlayer(BotName(i), PlayerControl::BOT, -1);
    }

    constexpr Float STEP = 1.0f / 60.0f;
    constexpr int SECONDS = 300;
    int kills = 0;
    int pickups = 0;
    int explosions = 0;
    int shots = 0;
    int ring_samples = 0;
    int altar_samples = 0;
    int samples = 0;
    std::array<FVec3, MAX_PLAYERS> last_positions{};
    std::array<int, MAX_PLAYERS> idle_seconds{};
    int max_idle = 0;
    for (int frame = 0; frame < SECONDS * 60; ++frame)
    {
        world.HostTick(STEP, MoveInput{}, FireRequest{});
        for (const GameEvent &event : world.TakeNewEvents())
        {
            kills += event.type == EventType::KILL ? 1 : 0;
            pickups += event.type == EventType::PICKUP ? 1 : 0;
            explosions += event.type == EventType::EXPLOSION ? 1 : 0;
            shots += event.type == EventType::SHOT ? 1 : 0;
        }
        if (frame % 60 != 0)
        {
            continue;
        }
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            const Player &player = world.players[i];
            if (!player.active)
            {
                continue;
            }
            const FVec3 p = player.move.position;
            if (std::isnan(p.x) || std::isnan(p.y) || std::isnan(p.z))
            {
                std::printf("NaN position for %s\n", player.name.c_str());
                ++failures;
            }
            if (world.arena.IsBlocked(PlayerBounds(p + FVec3{0.0f, 0.01f, 0.0f})))
            {
                std::printf("%s embedded in geometry at %.2f %.2f %.2f\n", player.name.c_str(), p.x, p.y, p.z);
                ++failures;
            }
            if (!player.alive)
            {
                idle_seconds[i] = 0;
                last_positions[i] = p;
                continue;
            }
            ++samples;
            ring_samples += p.y > 5.0f ? 1 : 0;
            altar_samples += (p.y > 2.5f && p.y < 3.5f && std::fabs(p.x) < 6 && std::fabs(p.z) < 6) ? 1 : 0;
            if (math::GetLength(p - last_positions[i]) < 0.5f)
            {
                ++idle_seconds[i];
                max_idle = std::max(max_idle, idle_seconds[i]);
            }
            else
            {
                idle_seconds[i] = 0;
            }
            last_positions[i] = p;
        }
    }

    std::printf("simulated %d s with %d bots\n", SECONDS, settings.botCount);
    std::printf("shots %d explosions %d kills %d pickups %d\n", shots, explosions, kills, pickups);
    std::printf("time on ring %.1f%% on altar %.1f%% longest idle %d s\n",
                100.0f * static_cast<Float>(ring_samples) / static_cast<Float>(std::max(samples, 1)),
                100.0f * static_cast<Float>(altar_samples) / static_cast<Float>(std::max(samples, 1)),
                max_idle);
    for (const Player &player : world.players)
    {
        if (player.active)
        {
            std::printf("  %-8s frags %3d deaths %3d\n", player.name.c_str(), player.frags, player.deaths);
        }
    }
    if (kills < 20)
    {
        std::printf("too few kills\n");
        ++failures;
    }
    if (pickups < 20)
    {
        std::printf("too few pickups\n");
        ++failures;
    }
    if (ring_samples == 0)
    {
        std::printf("bots never reached the upper ring\n");
        ++failures;
    }
    if (max_idle > 8)
    {
        std::printf("a bot stayed in place for %d seconds\n", max_idle);
        ++failures;
    }
    std::printf(failures == 0 ? "SELFTEST PASS\n" : "SELFTEST FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
} // namespace evo

#ifndef __EMSCRIPTEN__
#include "net/session.h"

namespace evo
{
int RunNetTest()
{
    World host_world;
    World client_world;
    net::Session host;
    net::Session client;
    MatchSettings settings;
    settings.botCount = 3;
    int failures = 0;

    if (!host.StartHost(host_world, settings, "Host", "27991"))
    {
        std::printf("host failed: %s\n", host.failureReason.c_str());
        return 1;
    }
    if (!client.StartClient(client_world, "Client", "127.0.0.1:27991"))
    {
        std::printf("client failed: %s\n", client.failureReason.c_str());
        return 1;
    }

    constexpr Float STEP = 1.0f / 60.0f;
    int client_events = 0;
    int remote_shots = 0;
    int fired = 0;
    FVec3 start_position;
    for (int frame = 0; frame < 60 * 6; ++frame)
    {
        host_world.HostTick(STEP, MoveInput{}, FireRequest{});
        host.Update(host_world, STEP);
        for (const GameEvent &event : host_world.TakeNewEvents())
        {
            if (event.type == EventType::SHOT && client.localPlayer >= 0 && event.a == client.localPlayer)
            {
                ++remote_shots;
            }
        }
        client.Update(client_world, STEP);
        client_events += static_cast<int>(client_world.TakeNewEvents().size());

        if (client.state == net::SessionState::ACTIVE && client.localPlayer >= 0)
        {
            Player &me = client_world.players[client.localPlayer];
            if (frame == 120)
            {
                start_position = me.move.position;
            }
            if (frame > 120 && frame < 240 && me.alive)
            {
                MoveInput input;
                input.forward = 1.0f;
                input.yaw = me.yaw;
                MoveEvents events;
                StepMovement(me.move, input, STEP, client_world.arena, events);
            }
            if (frame % 30 == 0 && frame > 60 && me.alive)
            {
                client.SendFire(
                    WeaponId::RIFLE, EyePosition(me.move),
                    math::GetDirectionFromYawPitch(me.yaw, 0.0f)
                );
                ++fired;
            }
        }
    }

    const int remote_id = client.localPlayer;
    std::printf("client state %d local id %d events %d fired %d host saw shots %d\n",
                static_cast<int>(client.state), remote_id, client_events, fired, remote_shots);
    if (client.state != net::SessionState::ACTIVE || remote_id < 0)
    {
        std::printf("client never became active: %s\n", client.failureReason.c_str());
        return 1;
    }
    int client_active = 0;
    for (const Player &player : client_world.players)
    {
        client_active += player.active ? 1 : 0;
    }
    std::printf("client sees %d players, host has %d remote\n", client_active,
                host_world.CountPlayers(PlayerControl::REMOTE));
    if (client_active != 5 || host_world.CountPlayers(PlayerControl::REMOTE) != 1)
    {
        ++failures;
    }
    const FVec3 client_position = client_world.players[remote_id].move.position;
    const FVec3 host_position = host_world.players[remote_id].move.position;
    const Float moved = math::GetLength(client_position - start_position);
    const Float drift = math::GetLength(client_position - host_position);
    std::printf("client moved %.2f m, host/client drift %.2f m\n", moved, drift);
    if (client_world.players[remote_id].alive && (moved < 1.0f || drift > 1.0f))
    {
        ++failures;
    }
    if (client_events == 0 || remote_shots == 0)
    {
        ++failures;
    }
    if (client_world.players[remote_id].name != "Client" || client_world.players[0].name != "Host")
    {
        std::printf("roster names wrong\n");
        ++failures;
    }

    client.Shutdown(client_world);
    for (int frame = 0; frame < 30; ++frame)
    {
        host_world.HostTick(STEP, MoveInput{}, FireRequest{});
        host.Update(host_world, STEP);
    }
    std::printf("after disconnect host has %d remote players\n", host_world.CountPlayers(PlayerControl::REMOTE));
    if (host_world.CountPlayers(PlayerControl::REMOTE) != 0)
    {
        ++failures;
    }
    host.Shutdown(host_world);
    std::printf(failures == 0 ? "NETTEST PASS\n" : "NETTEST FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
} // namespace evo
#endif
