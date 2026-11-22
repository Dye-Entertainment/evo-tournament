#include "net/session.h"
#include "game/bot.h"
#include "net/byte_stream.h"

namespace evo::net
{
namespace
{
constexpr Float SNAPSHOT_INTERVAL = 0.05f;
constexpr Float ROSTER_INTERVAL = 1.0f;
constexpr Float HELLO_INTERVAL = 0.5f;
constexpr Float CLIENT_STATE_INTERVAL = 1.0f / 30.0f;
constexpr Float CONNECT_TIMEOUT = 20.0f;
constexpr Float SILENCE_TIMEOUT = 10.0f;
constexpr Float REMOTE_TIMEOUT = 12.0f;
constexpr size_t MAX_SNAPSHOT_EVENTS = 48;
constexpr UInt8 NO_PLAYER = 255;

constexpr UInt8 FLAG_ALIVE = 1 << 0;
constexpr UInt8 FLAG_BOT = 1 << 1;
constexpr UInt8 FLAG_GROUND = 1 << 2;
constexpr UInt8 FLAG_FIRING = 1 << 3;

std::vector<UInt8> SingleByte(const MessageType type)
{
    return std::vector<UInt8>{static_cast<UInt8>(type)};
}

Bool IsSane(const FVec3 &v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::fabs(v.x) < 100.0f &&
           std::fabs(v.y) < 100.0f && std::fabs(v.z) < 100.0f;
}
} // namespace

void Session::StartOffline(World &world, const MatchSettings &settings, const std::string &name)
{
    Shutdown(world);
    mode = SessionMode::OFFLINE;
    state = SessionState::ACTIVE;
    playerName = name;
    world.Initialize(settings);
    localPlayer = world.AddPlayer(name, PlayerControl::LOCAL, -1);
    FillBots(world);
}

Bool Session::StartHost(
    World &world, const MatchSettings &settings, const std::string &name,
    const std::string &host_address
)
{
    Shutdown(world);
    transport = CreateTransport();
    address = host_address;
    if (!transport->StartHost(host_address))
    {
        Fail(transport->Status());
        return false;
    }
    mode = SessionMode::HOST;
    state = SessionState::ACTIVE;
    playerName = name;
    world.Initialize(settings);
    localPlayer = world.AddPlayer(name, PlayerControl::LOCAL, -1);
    FillBots(world);
    return true;
}

Bool Session::StartClient(World &world, const std::string &name, const std::string &host_address)
{
    Shutdown(world);
    transport = CreateTransport();
    address = host_address;
    playerName = name;
    world.Initialize(MatchSettings{});
    mode = SessionMode::CLIENT;
    state = SessionState::CONNECTING;
    connectTimer = 0.0f;
    helloTimer = 0.0f;
    silenceTimer = 0.0f;
    transportConnected = false;
    hasEventSequence = false;
    if (!transport->StartClient(host_address))
    {
        Fail(transport->Status());
        return false;
    }
    return true;
}

void Session::Shutdown(World &world)
{
    if (transport)
    {
        if (mode == SessionMode::CLIENT && transportConnected)
        {
            transport->Send(0, SingleByte(MessageType::BYE));
        }
        else if (mode == SessionMode::HOST)
        {
            SendToRemotes(world, SingleByte(MessageType::BYE));
        }
        transport->Shutdown();
        transport.reset();
    }
    mode = SessionMode::NONE;
    state = SessionState::IDLE;
    localPlayer = -1;
    knownNames = {};
    messages.clear();
}

void Session::Fail(const std::string &reason)
{
    state = SessionState::FAILED;
    failureReason = reason.empty() ? std::string("Connection failed") : reason;
}

std::string Session::TransportStatus() const
{
    return transport ? transport->Status() : std::string();
}

int Session::RemotePlayerCount(const World &world) const
{
    return world.CountPlayers(PlayerControl::REMOTE);
}

std::vector<std::string> Session::TakeMessages()
{
    std::vector<std::string> result;
    result.swap(messages);
    return result;
}

void Session::Update(World &world, const Float delta_seconds)
{
    if (mode == SessionMode::HOST)
    {
        HostUpdate(world, delta_seconds);
    }
    else if (mode == SessionMode::CLIENT)
    {
        ClientUpdate(world, delta_seconds);
    }
}

void Session::FillBots(World &world)
{
    int bots = world.CountPlayers(PlayerControl::BOT);
    while (bots < world.settings.botCount)
    {
        std::string name;
        for (int index = 0; index < 64; ++index)
        {
            const std::string candidate = BotName(index);
            Bool taken = false;
            for (const Player &player : world.players)
            {
                taken |= player.active && player.name == candidate;
            }
            if (!taken)
            {
                name = candidate;
                break;
            }
        }
        if (world.AddPlayer(name, PlayerControl::BOT, -1) < 0)
        {
            break;
        }
        ++bots;
        rosterDirty = true;
    }
}

void Session::SendToRemotes(const World &world, const std::vector<UInt8> &data)
{
    if (!transport)
    {
        return;
    }
    for (const Player &player : world.players)
    {
        if (player.active && player.control == PlayerControl::REMOTE)
        {
            transport->Send(player.peer, data);
        }
    }
}

void Session::HostRemovePeer(World &world, const int peer)
{
    const int player_id = world.FindPlayerByPeer(peer);
    if (player_id < 0)
    {
        return;
    }
    messages.push_back(world.players[player_id].name + " left the match");
    world.RemovePlayer(player_id);
    rosterDirty = true;
    FillBots(world);
}

void Session::HostUpdate(World &world, const Float delta_seconds)
{
    TransportEvent event;
    while (transport && transport->Poll(event))
    {
        if (event.type == TransportEventType::DISCONNECTED)
        {
            HostRemovePeer(world, event.peer);
        }
        else if (event.type == TransportEventType::DATA)
        {
            HostReceive(world, event.peer, event.data);
        }
    }

    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        const Player &player = world.players[i];
        if (player.active && player.control == PlayerControl::REMOTE &&
            world.time - player.lastHeard > REMOTE_TIMEOUT)
        {
            HostRemovePeer(world, player.peer);
        }
    }

    snapshotTimer += delta_seconds;
    if (snapshotTimer >= SNAPSHOT_INTERVAL)
    {
        snapshotTimer = 0.0f;
        if (world.CountPlayers(PlayerControl::REMOTE) > 0)
        {
            SendToRemotes(world, BuildSnapshot(world));
        }
    }
    rosterTimer += delta_seconds;
    if (rosterDirty || rosterTimer >= ROSTER_INTERVAL)
    {
        rosterTimer = 0.0f;
        rosterDirty = false;
        if (world.CountPlayers(PlayerControl::REMOTE) > 0)
        {
            SendToRemotes(world, BuildRoster(world));
        }
    }
}

void Session::HostReceive(World &world, const int peer, const std::vector<UInt8> &data)
{
    ByteReader reader(data.data(), data.size());
    const auto type = static_cast<MessageType>(reader.U8());
    const int player_id = world.FindPlayerByPeer(peer);
    if (player_id >= 0)
    {
        world.players[player_id].lastHeard = world.time;
    }

    switch (type)
    {
    case MessageType::HELLO:
    {
        const UInt8 version = reader.U8();
        std::string name = reader.String();
        if (reader.Failed())
        {
            return;
        }
        if (version != PROTOCOL_VERSION)
        {
            ByteWriter writer;
            writer.U8(static_cast<UInt8>(MessageType::REJECT));
            writer.String("Version mismatch: please refresh the page");
            transport->Send(peer, writer.data);
            return;
        }
        int id = player_id;
        if (id < 0)
        {
            Bool has_free_slot = false;
            for (const Player &player : world.players)
            {
                has_free_slot |= !player.active;
            }
            if (!has_free_slot)
            {
                for (int i = MAX_PLAYERS - 1; i >= 0; --i)
                {
                    if (world.players[i].active && world.players[i].control == PlayerControl::BOT)
                    {
                        world.RemovePlayer(i);
                        break;
                    }
                }
            }
            if (name.empty())
            {
                name = "Player";
            }
            id = world.AddPlayer(name, PlayerControl::REMOTE, peer);
            if (id < 0)
            {
                ByteWriter writer;
                writer.U8(static_cast<UInt8>(MessageType::REJECT));
                writer.String("The match is full");
                transport->Send(peer, writer.data);
                return;
            }
            messages.push_back(world.players[id].name + " joined the match");
            rosterDirty = true;
        }
        ByteWriter writer;
        writer.U8(static_cast<UInt8>(MessageType::WELCOME));
        writer.U8(static_cast<UInt8>(id));
        writer.I16(world.settings.fragLimit);
        writer.F32(world.settings.timeLimitSeconds);
        transport->Send(peer, writer.data);
        transport->Send(peer, BuildRoster(world));
        transport->Send(peer, BuildSnapshot(world));
        return;
    }
    case MessageType::CLIENT_STATE:
    {
        if (player_id < 0)
        {
            return;
        }
        Player &player = world.players[player_id];
        const UInt8 spawn_count = reader.U8();
        const FVec3 position = reader.Vec3();
        const FVec3 velocity = reader.Vec3();
        const Float yaw = reader.Angle();
        const Float pitch = reader.Angle();
        const UInt8 weapon = reader.U8();
        const UInt8 on_ground = reader.U8();
        if (reader.Failed() || spawn_count != player.spawnCount || !player.alive ||
            !IsSane(position) || !IsSane(velocity))
        {
            return;
        }
        player.move.position = position;
        player.move.velocity = velocity;
        player.move.onGround = on_ground != 0;
        player.yaw = yaw;
        player.pitch = pitch;
        if (weapon < WEAPON_COUNT && player.hasWeapon[weapon])
        {
            player.weapon = static_cast<WeaponId>(weapon);
        }
        return;
    }
    case MessageType::CLIENT_FIRE:
    {
        if (player_id < 0)
        {
            return;
        }
        Player &player = world.players[player_id];
        const UInt8 weapon = reader.U8();
        FVec3 origin = reader.Vec3();
        const FVec3 direction = math::GetNormalized(reader.Vec3());
        if (reader.Failed() || weapon >= WEAPON_COUNT || !IsSane(origin) ||
            math::GetLength(direction) < 0.5f)
        {
            return;
        }
        player.weapon = static_cast<WeaponId>(weapon);
        const Float tolerance = GetWeaponDef(player.weapon).fireInterval * 0.35f;
        if (player.fireCooldown > tolerance)
        {
            return;
        }
        player.fireCooldown = 0.0f;
        if (!world.CanFire(player))
        {
            return;
        }
        world.ConsumeShot(player);
        const FVec3 eye = EyePosition(player.move);
        if (math::GetLength(origin - eye) > 2.5f)
        {
            origin = eye;
        }
        world.HostFire(player_id, origin, direction);
        return;
    }
    case MessageType::CLIENT_RESPAWN:
        if (player_id >= 0)
        {
            world.players[player_id].wantsRespawn = true;
        }
        return;
    case MessageType::BYE:
        HostRemovePeer(world, peer);
        return;
    default:
        return;
    }
}

std::vector<UInt8> Session::BuildSnapshot(const World &world) const
{
    ByteWriter writer;
    writer.U8(static_cast<UInt8>(MessageType::SNAPSHOT));
    writer.U8(static_cast<UInt8>(world.match.phase));
    writer.F32(world.match.timeRemaining);
    writer.U8(world.match.winner < 0 ? NO_PLAYER : static_cast<UInt8>(world.match.winner));
    writer.I16(world.settings.fragLimit);

    UInt8 count = 0;
    for (const Player &player : world.players)
    {
        count = static_cast<UInt8>(count + (player.active ? 1 : 0));
    }
    writer.U8(count);
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        const Player &player = world.players[i];
        if (!player.active)
        {
            continue;
        }
        UInt8 flags = 0;
        flags |= player.alive ? FLAG_ALIVE : 0;
        flags |= player.control == PlayerControl::BOT ? FLAG_BOT : 0;
        flags |= player.move.onGround ? FLAG_GROUND : 0;
        flags |= player.fireFlash > 0.0f ? FLAG_FIRING : 0;
        writer.U8(static_cast<UInt8>(i));
        writer.U8(flags);
        writer.Vec3(player.move.position);
        writer.Vec3(player.move.velocity);
        writer.Angle(player.yaw);
        writer.Angle(player.pitch);
        writer.I16(player.health);
        writer.I16(player.armor);
        writer.U8(static_cast<UInt8>(player.weapon));
        UInt8 weapons = 0;
        for (int w = 0; w < WEAPON_COUNT; ++w)
        {
            weapons |= player.hasWeapon[w] ? static_cast<UInt8>(1 << w) : 0;
        }
        writer.U8(weapons);
        for (int w = 0; w < WEAPON_COUNT; ++w)
        {
            writer.I16(player.ammo[w]);
        }
        writer.I16(player.frags);
        writer.I16(player.deaths);
        writer.U8(player.spawnCount);
    }

    const size_t projectile_count = std::min<size_t>(world.projectiles.size(), 64);
    writer.U8(static_cast<UInt8>(projectile_count));
    for (size_t i = 0; i < projectile_count; ++i)
    {
        const Projectile &projectile = world.projectiles[i];
        writer.U16(projectile.id);
        writer.U8(projectile.owner);
        writer.Vec3(projectile.position);
        writer.Vec3(projectile.velocity);
    }

    UInt32 pickup_mask = 0;
    for (size_t i = 0; i < world.pickups.size() && i < 32; ++i)
    {
        pickup_mask |= world.pickups[i].available ? (1U << i) : 0U;
    }
    writer.U32(pickup_mask);

    const size_t event_count = std::min(world.eventLog.size(), MAX_SNAPSHOT_EVENTS);
    writer.U8(static_cast<UInt8>(event_count));
    for (size_t i = world.eventLog.size() - event_count; i < world.eventLog.size(); ++i)
    {
        const GameEvent &event = world.eventLog[i];
        writer.U16(event.sequence);
        writer.U8(static_cast<UInt8>(event.type));
        writer.U8(event.a);
        writer.U8(event.b);
        writer.U8(event.c);
        writer.Vec3(event.p0);
        writer.Vec3(event.p1);
    }
    return writer.data;
}

std::vector<UInt8> Session::BuildRoster(const World &world) const
{
    ByteWriter writer;
    writer.U8(static_cast<UInt8>(MessageType::ROSTER));
    UInt8 count = 0;
    for (const Player &player : world.players)
    {
        count = static_cast<UInt8>(count + (player.active ? 1 : 0));
    }
    writer.U8(count);
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        const Player &player = world.players[i];
        if (!player.active)
        {
            continue;
        }
        writer.U8(static_cast<UInt8>(i));
        writer.U8(player.control == PlayerControl::BOT ? 1 : 0);
        writer.U8(player.colorIndex);
        writer.String(player.name);
    }
    return writer.data;
}

void Session::ClientUpdate(World &world, const Float delta_seconds)
{
    world.time += delta_seconds;
    TransportEvent event;
    while (transport && transport->Poll(event))
    {
        if (event.type == TransportEventType::CONNECTED)
        {
            transportConnected = true;
            helloTimer = 0.0f;
        }
        else if (event.type == TransportEventType::DISCONNECTED)
        {
            transportConnected = false;
            if (state == SessionState::ACTIVE)
            {
                Fail("Connection to the host was lost");
            }
            else
            {
                Fail(transport->Status());
            }
            return;
        }
        else
        {
            silenceTimer = 0.0f;
            ClientReceive(world, event.data);
        }
    }

    if (state == SessionState::CONNECTING)
    {
        connectTimer += delta_seconds;
        if (connectTimer > CONNECT_TIMEOUT)
        {
            Fail("Timed out while connecting");
            return;
        }
        if (transportConnected)
        {
            helloTimer -= delta_seconds;
            if (helloTimer <= 0.0f)
            {
                helloTimer = HELLO_INTERVAL;
                ByteWriter writer;
                writer.U8(static_cast<UInt8>(MessageType::HELLO));
                writer.U8(PROTOCOL_VERSION);
                writer.String(playerName);
                transport->Send(0, writer.data);
            }
        }
        return;
    }

    if (state != SessionState::ACTIVE)
    {
        return;
    }

    silenceTimer += delta_seconds;
    if (silenceTimer > SILENCE_TIMEOUT)
    {
        Fail("The host stopped responding");
        return;
    }

    stateTimer += delta_seconds;
    if (stateTimer >= CLIENT_STATE_INTERVAL && localPlayer >= 0)
    {
        stateTimer = 0.0f;
        const Player &player = world.players[localPlayer];
        ByteWriter writer;
        writer.U8(static_cast<UInt8>(MessageType::CLIENT_STATE));
        writer.U8(player.spawnCount);
        writer.Vec3(player.move.position);
        writer.Vec3(player.move.velocity);
        writer.Angle(player.yaw);
        writer.Angle(player.pitch);
        writer.U8(static_cast<UInt8>(player.weapon));
        writer.U8(player.move.onGround ? 1 : 0);
        transport->Send(0, writer.data);
    }
}

void Session::ClientReceive(World &world, const std::vector<UInt8> &data)
{
    if (data.empty())
    {
        return;
    }
    const auto type = static_cast<MessageType>(data[0]);
    switch (type)
    {
    case MessageType::WELCOME:
    {
        ByteReader reader(data.data(), data.size());
        reader.U8();
        const UInt8 id = reader.U8();
        const Int16 frag_limit = reader.I16();
        const Float time_limit = reader.F32();
        if (reader.Failed() || id >= MAX_PLAYERS)
        {
            return;
        }
        if (state == SessionState::CONNECTING)
        {
            localPlayer = id;
            world.settings.fragLimit = frag_limit;
            world.settings.timeLimitSeconds = time_limit;
            Player &player = world.players[id];
            player = Player{};
            player.active = true;
            player.control = PlayerControl::LOCAL;
            player.name = playerName;
            player.colorIndex = id;
            state = SessionState::ACTIVE;
            silenceTimer = 0.0f;
        }
        return;
    }
    case MessageType::REJECT:
    {
        ByteReader reader(data.data(), data.size());
        reader.U8();
        Fail(reader.String());
        return;
    }
    case MessageType::SNAPSHOT:
        if (state == SessionState::ACTIVE)
        {
            ApplySnapshot(world, data);
        }
        return;
    case MessageType::ROSTER:
        if (state == SessionState::ACTIVE)
        {
            ApplyRoster(world, data);
        }
        return;
    case MessageType::BYE:
        Fail("The host ended the match");
        return;
    default:
        return;
    }
}

void Session::ApplyRoster(World &world, const std::vector<UInt8> &data)
{
    ByteReader reader(data.data(), data.size());
    reader.U8();
    const UInt8 count = reader.U8();
    std::array<Bool, MAX_PLAYERS> listed{};
    for (int n = 0; n < count; ++n)
    {
        const UInt8 id = reader.U8();
        const UInt8 is_bot = reader.U8();
        const UInt8 color = reader.U8();
        const std::string name = reader.String();
        if (reader.Failed() || id >= MAX_PLAYERS)
        {
            return;
        }
        listed[id] = true;
        Player &player = world.players[id];
        if (!player.active)
        {
            player = Player{};
            player.active = true;
            player.control = PlayerControl::REPLICA;
        }
        if (id != localPlayer && is_bot == 0 && knownNames[id] != name)
        {
            messages.push_back(name + " is in the match");
        }
        knownNames[id] = name;
        player.name = name;
        player.colorIndex = color;
        if (id != localPlayer)
        {
            player.control = is_bot != 0 ? PlayerControl::BOT : PlayerControl::REPLICA;
        }
    }
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (!listed[i] && i != localPlayer && world.players[i].active)
        {
            world.players[i] = Player{};
            knownNames[i].clear();
        }
    }
}

void Session::ApplySnapshot(World &world, const std::vector<UInt8> &data)
{
    ByteReader reader(data.data(), data.size());
    reader.U8();
    const auto phase = static_cast<MatchPhase>(reader.U8());
    const Float time_remaining = reader.F32();
    const UInt8 winner = reader.U8();
    const Int16 frag_limit = reader.I16();
    if (reader.Failed())
    {
        return;
    }
    world.match.phase = phase;
    world.match.timeRemaining = time_remaining;
    world.match.winner = winner == NO_PLAYER ? -1 : winner;
    world.settings.fragLimit = frag_limit;

    const UInt8 count = reader.U8();
    std::array<Bool, MAX_PLAYERS> seen{};
    for (int n = 0; n < count; ++n)
    {
        const UInt8 id = reader.U8();
        const UInt8 flags = reader.U8();
        const FVec3 position = reader.Vec3();
        const FVec3 velocity = reader.Vec3();
        const Float yaw = reader.Angle();
        const Float pitch = reader.Angle();
        const Int16 health = reader.I16();
        const Int16 armor = reader.I16();
        const UInt8 weapon = reader.U8();
        const UInt8 weapons = reader.U8();
        std::array<Int16, WEAPON_COUNT> ammo{};
        for (int w = 0; w < WEAPON_COUNT; ++w)
        {
            ammo[w] = reader.I16();
        }
        const Int16 frags = reader.I16();
        const Int16 deaths = reader.I16();
        const UInt8 spawn_count = reader.U8();
        if (reader.Failed() || id >= MAX_PLAYERS)
        {
            return;
        }
        seen[id] = true;
        Player &player = world.players[id];
        const Bool alive = (flags & FLAG_ALIVE) != 0;
        if (!player.active)
        {
            player = Player{};
            player.active = true;
            player.control = (flags & FLAG_BOT) != 0 ? PlayerControl::BOT : PlayerControl::REPLICA;
            player.renderPosition = position;
            player.renderYaw = yaw;
        }
        std::array<Bool, WEAPON_COUNT> has_weapon{};
        for (int w = 0; w < WEAPON_COUNT; ++w)
        {
            has_weapon[w] = (weapons & (1 << w)) != 0;
        }

        if (id == localPlayer)
        {
            if (spawn_count != player.spawnCount)
            {
                player.move = MoveState{};
                player.move.position = position;
                player.move.onGround = true;
                player.yaw = yaw;
                player.pitch = 0.0f;
                player.renderPosition = position;
                player.weapon = WeaponId::RIFLE;
                player.spawnCount = spawn_count;
                player.hasWeapon = has_weapon;
            }
            for (int w = 0; w < WEAPON_COUNT; ++w)
            {
                if (has_weapon[w] && !player.hasWeapon[w])
                {
                    player.weapon = static_cast<WeaponId>(w);
                }
            }
            player.hasWeapon = has_weapon;
            player.alive = alive;
            player.health = health;
            player.armor = armor;
            player.ammo = ammo;
            player.frags = frags;
            player.deaths = deaths;
            continue;
        }

        player.move.position = position;
        player.move.velocity = velocity;
        player.move.onGround = (flags & FLAG_GROUND) != 0;
        player.yaw = yaw;
        player.pitch = pitch;
        if (!player.alive && alive)
        {
            player.renderPosition = position;
        }
        player.alive = alive;
        player.health = health;
        player.armor = armor;
        player.weapon = static_cast<WeaponId>(weapon % WEAPON_COUNT);
        player.hasWeapon = has_weapon;
        player.ammo = ammo;
        player.frags = frags;
        player.deaths = deaths;
        player.spawnCount = spawn_count;
        player.lastHeard = world.time;
        if ((flags & FLAG_FIRING) != 0)
        {
            player.fireFlash = 0.08f;
        }
    }
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (!seen[i] && i != localPlayer && world.players[i].active)
        {
            world.players[i] = Player{};
        }
    }

    const UInt8 projectile_count = reader.U8();
    std::vector<Projectile> projectiles;
    for (int n = 0; n < projectile_count; ++n)
    {
        Projectile projectile;
        projectile.id = reader.U16();
        projectile.owner = reader.U8();
        projectile.position = reader.Vec3();
        projectile.velocity = reader.Vec3();
        projectile.life = 1.0f;
        projectiles.push_back(projectile);
    }
    const UInt32 pickup_mask = reader.U32();
    if (reader.Failed())
    {
        return;
    }
    world.projectiles = projectiles;
    for (size_t i = 0; i < world.pickups.size() && i < 32; ++i)
    {
        world.pickups[i].available = (pickup_mask & (1U << i)) != 0;
    }

    const UInt8 event_count = reader.U8();
    UInt16 newest = lastEventSequence;
    for (int n = 0; n < event_count; ++n)
    {
        GameEvent event;
        event.sequence = reader.U16();
        event.type = static_cast<EventType>(reader.U8());
        event.a = reader.U8();
        event.b = reader.U8();
        event.c = reader.U8();
        event.p0 = reader.Vec3();
        event.p1 = reader.Vec3();
        event.time = world.time;
        if (reader.Failed())
        {
            return;
        }
        const auto delta = static_cast<Int16>(event.sequence - lastEventSequence);
        if (static_cast<Int16>(event.sequence - newest) > 0)
        {
            newest = event.sequence;
        }
        if (!hasEventSequence || delta <= 0)
        {
            continue;
        }
        if (event.type == EventType::SHOT && event.a == localPlayer)
        {
            continue;
        }
        if (event.type == EventType::IMPULSE)
        {
            if (event.a == localPlayer && localPlayer >= 0)
            {
                Player &player = world.players[localPlayer];
                player.move.velocity = player.move.velocity + event.p0;
                if (event.p0.y > 0.5f)
                {
                    player.move.onGround = false;
                }
            }
            continue;
        }
        world.newEvents.push_back(event);
    }
    lastEventSequence = newest;
    hasEventSequence = true;
}

void Session::SendFire(const WeaponId weapon, const FVec3 &origin, const FVec3 &direction)
{
    if (mode != SessionMode::CLIENT || state != SessionState::ACTIVE || !transport)
    {
        return;
    }
    ByteWriter writer;
    writer.U8(static_cast<UInt8>(MessageType::CLIENT_FIRE));
    writer.U8(static_cast<UInt8>(weapon));
    writer.Vec3(origin);
    writer.Vec3(direction);
    transport->Send(0, writer.data);
}

void Session::SendRespawnRequest()
{
    if (mode != SessionMode::CLIENT || state != SessionState::ACTIVE || !transport)
    {
        return;
    }
    transport->Send(0, SingleByte(MessageType::CLIENT_RESPAWN));
}
} // namespace evo::net
