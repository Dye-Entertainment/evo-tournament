#include "game/world.h"
#include "game/bot.h"

namespace evo
{
namespace
{
const WeaponDef WEAPON_DEFS[WEAPON_COUNT] = {
    {"Pulse Rifle", 0.1f, 9, 200, 80, 50, true, 0.012f, 0.0f, 0.0f, 0, 1.2f, 1.0f,
     FVec3{0.25f, 0.9f, 1.0f}},
    {"Nova Launcher", 0.8f, 0, 30, 0, 10, false, 0.0f, 30.0f, 4.0f, 95, 15.0f, 1.0f,
     FVec3{1.0f, 0.55f, 0.1f}},
    {"Rail Lance", 1.25f, 80, 25, 0, 8, true, 0.0f, 0.0f, 0.0f, 0, 5.0f, 1.75f,
     FVec3{0.85f, 0.3f, 1.0f}},
    // Damage and knockback are per pellet.
    {"Scattergun", 0.95f, 8, 36, 0, 12, true, 0.0f, 0.0f, 0.0f, 0, 1.1f, 1.25f,
     FVec3{1.0f, 0.82f, 0.25f}, 12, 0.075f},
    {"Frag Launcher", 0.7f, 0, 30, 0, 10, false, 0.0f, 24.0f, 0.0f, 0, 0.0f, 1.0f,
     FVec3{0.45f, 1.0f, 0.3f}, 1, 0.0f, ProjectileKind::FRAG},
    // Damage is per disc hit; discs ricochet off walls.
    {"Ripper", 0.32f, 30, 60, 0, 20, false, 0.0f, 42.0f, 0.0f, 0, 4.0f, 1.5f,
     FVec3{1.0f, 0.3f, 0.55f}, 1, 0.0f, ProjectileKind::DISC},
};

const BlastDef BLAST_DEFS[static_cast<int>(ProjectileKind::COUNT)] = {
    {4.0f, 95, 15.0f, DamageSource::NOVA},
    {3.6f, 85, 13.0f, DamageSource::FRAG},
    {4.8f, 115, 17.0f, DamageSource::GRENADE},
    {3.6f, 100, 17.0f, DamageSource::MINE},
    {0.0f, 0, 0.0f, DamageSource::RIPPER},
};

constexpr Float GRAVITY = 18.0f;
constexpr Float THROW_COOLDOWN = 0.6f;
constexpr Float GRENADE_FUSE = 2.4f;
constexpr Float FRAG_FUSE = 2.0f;
constexpr Float MINE_ARM_SECONDS = 1.0f;
constexpr Float MINE_TRIGGER_RADIUS = 2.0f;
constexpr Float MINE_LIFETIME = 120.0f;
constexpr Float DISC_LIFETIME = 2.5f;
constexpr int DISC_MAX_BOUNCES = 3;

const FVec3 PLAYER_COLORS[MAX_PLAYERS] = {
    {0.20f, 0.55f, 1.00f}, {1.00f, 0.25f, 0.20f}, {0.25f, 0.90f, 0.35f},
    {1.00f, 0.80f, 0.15f}, {0.75f, 0.30f, 1.00f}, {1.00f, 0.50f, 0.10f},
    {0.10f, 0.90f, 0.90f}, {1.00f, 0.40f, 0.70f}, {0.65f, 0.85f, 0.25f},
    {0.95f, 0.95f, 0.95f}, {0.55f, 0.35f, 0.20f}, {0.40f, 0.40f, 0.95f},
};

constexpr Float EVENT_LOG_SECONDS = 1.0f;
constexpr Float PICKUP_RADIUS = 1.3f;
constexpr Float MATCH_END_SECONDS = 10.0f;
constexpr Float MAX_TRACE_DISTANCE = 250.0f;
constexpr UInt8 NO_PLAYER = 255;

UInt8 ToEventPlayer(const int player_id)
{
    return player_id < 0 ? NO_PLAYER : static_cast<UInt8>(player_id);
}
} // namespace

const WeaponDef &GetWeaponDef(const WeaponId weapon)
{
    return WEAPON_DEFS[static_cast<int>(weapon) % WEAPON_COUNT];
}

const BlastDef &GetBlastDef(const ProjectileKind kind)
{
    return BLAST_DEFS[static_cast<int>(kind) % static_cast<int>(ProjectileKind::COUNT)];
}

const Char *DamageSourceName(const DamageSource source)
{
    switch (source)
    {
    case DamageSource::NOVA:
        return "NOVA";
    case DamageSource::RAIL:
        return "RAIL";
    case DamageSource::SCATTER:
        return "SCATTER";
    case DamageSource::FRAG:
        return "FRAG";
    case DamageSource::RIPPER:
        return "RIPPER";
    case DamageSource::GRENADE:
        return "GRENADE";
    case DamageSource::MINE:
        return "MINE";
    default:
        return "PULSE";
    }
}

Bool IsExplosive(const DamageSource source)
{
    return source == DamageSource::NOVA || source == DamageSource::FRAG ||
           source == DamageSource::GRENADE || source == DamageSource::MINE;
}

FVec3 DamageSourceColor(const DamageSource source)
{
    if (source == DamageSource::GRENADE)
    {
        return {1.0f, 0.65f, 0.2f};
    }
    if (source == DamageSource::MINE)
    {
        return {1.0f, 0.2f, 0.15f};
    }
    return GetWeaponDef(static_cast<WeaponId>(source)).color;
}

FVec3 PelletDirection(const FVec3 &direction, const UInt32 seed, const int pellet, const Float spread)
{
    if (pellet == 0 || spread <= 0.0f)
    {
        return direction;
    }
    const FVec3 up_hint = std::fabs(direction.y) > 0.95f ? FVec3{1, 0, 0} : FVec3{0, 1, 0};
    const FVec3 right = math::GetNormalized(math::GetCrossProduct(up_hint, direction));
    const FVec3 up = math::GetCrossProduct(direction, right);
    const UInt32 key = seed * 2654435761U + static_cast<UInt32>(pellet) * 40503U + 7U;
    const Float angle = static_cast<Float>(math::GetHash(key) & 0xFFFF) / 65535.0f * 6.2831853f;
    // Square root keeps the pellets evenly spread over the cone instead of bunched in the middle.
    const Float radius =
        std::sqrt(static_cast<Float>(math::GetHash(key ^ 0x9E3779B9U) & 0xFFFF) / 65535.0f) * spread;
    return math::GetNormalized(direction + right * (std::cos(angle) * radius) + up * (std::sin(angle) * radius));
}

FVec3 PlayerColor(const UInt8 color_index)
{
    return PLAYER_COLORS[color_index % MAX_PLAYERS];
}

Float PickupRespawnTime(const PickupKind kind)
{
    switch (kind)
    {
    case PickupKind::HEALTH:
        return 20.0f;
    case PickupKind::MEGA_HEALTH:
        return 50.0f;
    case PickupKind::ARMOR:
        return 30.0f;
    case PickupKind::WEAPON_NOVA:
    case PickupKind::WEAPON_RAIL:
    case PickupKind::WEAPON_SCATTER:
    case PickupKind::WEAPON_FRAG:
    case PickupKind::WEAPON_RIPPER:
        return 12.0f;
    case PickupKind::GRENADES:
        return 18.0f;
    case PickupKind::MINES:
        return 25.0f;
    default:
        return 15.0f;
    }
}

const Char *PickupName(const PickupKind kind)
{
    switch (kind)
    {
    case PickupKind::HEALTH:
        return "Health Pack";
    case PickupKind::MEGA_HEALTH:
        return "Mega Health";
    case PickupKind::ARMOR:
        return "Shield Armor";
    case PickupKind::WEAPON_NOVA:
        return "Nova Launcher";
    case PickupKind::WEAPON_RAIL:
        return "Rail Lance";
    case PickupKind::AMMO_RIFLE:
        return "Pulse Cells";
    case PickupKind::AMMO_NOVA:
        return "Nova Charges";
    case PickupKind::AMMO_RAIL:
        return "Rail Slugs";
    case PickupKind::WEAPON_SCATTER:
        return "Scattergun";
    case PickupKind::WEAPON_FRAG:
        return "Frag Launcher";
    case PickupKind::WEAPON_RIPPER:
        return "Ripper";
    case PickupKind::AMMO_SCATTER:
        return "Scatter Shells";
    case PickupKind::AMMO_FRAG:
        return "Frag Shells";
    case PickupKind::AMMO_RIPPER:
        return "Ripper Discs";
    case PickupKind::GRENADES:
        return "Grenades";
    case PickupKind::MINES:
        return "Proximity Mines";
    default:
        return "Pickup";
    }
}

void World::Initialize(const MatchSettings &match_settings)
{
    settings = match_settings;
    arena.Build(settings.arena);
    players = {};
    projectiles.clear();
    eventLog.clear();
    newEvents.clear();
    pickups.clear();
    for (const PickupSpot &spot : arena.pickupSpots)
    {
        pickups.push_back(Pickup{spot.kind, spot.position, true, 0.0f});
    }
    match = MatchState{};
    match.timeRemaining = settings.timeLimitSeconds;
    time = 0.0f;
}

int World::AddPlayer(const std::string &player_name, const PlayerControl control, const int peer)
{
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        Player &player = players[i];
        if (player.active)
        {
            continue;
        }
        player = Player{};
        player.active = true;
        player.control = control;
        player.peer = peer;
        player.name = player_name.substr(0, MAX_NAME_LENGTH);
        player.colorIndex = static_cast<UInt8>(i);
        player.lastHeard = time;
        player.brain.skill = settings.botSkill;
        if (control != PlayerControl::REPLICA)
        {
            HostRespawn(i);
        }
        return i;
    }
    return -1;
}

void World::RemovePlayer(const int player_id)
{
    if (player_id < 0 || player_id >= MAX_PLAYERS)
    {
        return;
    }
    players[player_id] = Player{};
    // Their shots, grenades and mines leave with them, so nobody who takes the slot gets the kills.
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
                                     [player_id](const Projectile &projectile)
                                     { return projectile.owner == player_id; }),
                      projectiles.end());
}

int World::CountPlayers(const PlayerControl control) const
{
    int count = 0;
    for (const Player &player : players)
    {
        if (player.active && player.control == control)
        {
            ++count;
        }
    }
    return count;
}

int World::FindPlayerByPeer(const int peer) const
{
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (players[i].active && players[i].control == PlayerControl::REMOTE &&
            players[i].peer == peer)
        {
            return i;
        }
    }
    return -1;
}

void World::HostTick(
    const Float delta_seconds, const MoveInput &local_input, const FireRequest &local_fire
)
{
    time += delta_seconds;
    StepMatch(delta_seconds);
    StepPlayers(delta_seconds, local_input, local_fire);
    StepProjectiles(delta_seconds);
    StepPickups(delta_seconds);
    while (!eventLog.empty() && eventLog.front().time < time - EVENT_LOG_SECONDS)
    {
        eventLog.pop_front();
    }
}

Bool World::CanFire(const Player &player) const
{
    const int weapon = static_cast<int>(player.weapon);
    return player.alive && player.fireCooldown <= 0.0f && player.hasWeapon[weapon] &&
           player.ammo[weapon] > 0 && match.phase == MatchPhase::PLAYING;
}

void World::ConsumeShot(Player &player) const
{
    const int weapon = static_cast<int>(player.weapon);
    player.ammo[weapon] = static_cast<Int16>(player.ammo[weapon] - 1);
    player.fireCooldown = GetWeaponDef(player.weapon).fireInterval;
}

FVec3 World::ApplySpread(const Player &player, const FVec3 &direction, const UInt32 seed) const
{
    const Float spread = GetWeaponDef(player.weapon).spread;
    if (spread <= 0.0f)
    {
        return direction;
    }
    const FVec3 up_hint = std::fabs(direction.y) > 0.95f ? FVec3{1, 0, 0} : FVec3{0, 1, 0};
    const FVec3 right = math::GetNormalized(math::GetCrossProduct(up_hint, direction));
    const FVec3 up = math::GetCrossProduct(direction, right);
    const Float a = (static_cast<Float>(math::GetHash(seed) & 0xFFFF) / 65535.0f - 0.5f) * 2.0f;
    const Float b =
        (static_cast<Float>(math::GetHash(seed * 7919U + 17U) & 0xFFFF) / 65535.0f - 0.5f) * 2.0f;
    return math::GetNormalized(direction + (right * (a * spread) + up * (b * spread)));
}

World::TraceResult World::Trace(
    const FVec3 &origin, const FVec3 &direction, const Float max_distance, const int ignore_player
) const
{
    TraceResult result;
    Float best = max_distance;
    RayHit hit;
    if (arena.RayCast(origin, direction, max_distance, hit))
    {
        best = hit.distance;
        result.normal = hit.normal;
    }
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        const Player &player = players[i];
        if (!player.active || !player.alive || i == ignore_player)
        {
            continue;
        }
        const primitive::AxisAlignedBox bounds =
            primitive::GetExpanded(PlayerBounds(player.move.position), FVec3{0.08f, 0.05f, 0.08f});
        Float distance = 0.0f;
        FVec3 normal;
        if (primitive::IntersectRay(bounds, origin, direction, best, distance, normal) &&
            distance < best)
        {
            best = distance;
            result.hitPlayer = i;
            result.normal = normal;
            const Float hit_height = origin.y + direction.y * distance - player.move.position.y;
            result.headshot = hit_height > PLAYER_HEAD_LINE;
        }
    }
    result.end = origin + direction * best;
    return result;
}

void World::HostFire(
    const int player_id, const FVec3 &origin, const FVec3 &direction, const UInt32 pellet_seed
)
{
    Player &shooter = players[player_id];
    const WeaponDef &def = GetWeaponDef(shooter.weapon);
    shooter.fireFlash = 0.08f;
    GameEvent shot;
    shot.type = EventType::SHOT;
    shot.a = static_cast<UInt8>(player_id);
    shot.c = static_cast<UInt8>(shooter.weapon);
    shot.p0 = origin;

    if (def.pellets > 1)
    {
        // All pellets are traced here; clients redraw them from the same seed.
        const UInt8 seed = static_cast<UInt8>(pellet_seed);
        shot.b = seed;
        shot.p1 = origin + direction * 10.0f;
        EmitEvent(shot);
        std::array<int, MAX_PLAYERS> hits{};
        std::array<int, MAX_PLAYERS> head_hits{};
        for (int pellet = 0; pellet < def.pellets; ++pellet)
        {
            const FVec3 pellet_direction = PelletDirection(direction, seed, pellet, def.pelletSpread);
            const TraceResult trace = Trace(origin, pellet_direction, MAX_TRACE_DISTANCE, player_id);
            if (trace.hitPlayer >= 0)
            {
                ++hits[trace.hitPlayer];
                head_hits[trace.hitPlayer] += trace.headshot ? 1 : 0;
            }
        }
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            if (hits[i] == 0)
            {
                continue;
            }
            const Float amount = static_cast<Float>(def.damage) *
                                 (static_cast<Float>(hits[i] - head_hits[i]) +
                                  static_cast<Float>(head_hits[i]) * def.headshotMultiplier);
            ApplyImpulse(i, direction * (def.knockback * static_cast<Float>(hits[i])));
            // It only counts as a headshot when most of the blast took the head off.
            Damage(i, player_id, static_cast<Int16>(amount + 0.5f), SourceOf(shooter.weapon),
                   head_hits[i] * 2 > def.pellets);
        }
        return;
    }

    if (def.hitscan)
    {
        const TraceResult trace = Trace(origin, direction, MAX_TRACE_DISTANCE, player_id);
        shot.p1 = trace.end;
        EmitEvent(shot);
        if (trace.hitPlayer >= 0)
        {
            const Float multiplier = trace.headshot ? def.headshotMultiplier : 1.0f;
            const auto amount = static_cast<Int16>(static_cast<Float>(def.damage) * multiplier);
            ApplyImpulse(trace.hitPlayer, direction * def.knockback);
            Damage(trace.hitPlayer, player_id, amount, SourceOf(shooter.weapon), trace.headshot && def.headshotMultiplier > 1.0f);
        }
        return;
    }

    FVec3 velocity = direction * def.projectileSpeed;
    Float life = 4.0f;
    if (def.projectile == ProjectileKind::FRAG)
    {
        velocity.y += 3.0f;
        life = FRAG_FUSE;
    }
    else if (def.projectile == ProjectileKind::DISC)
    {
        life = DISC_LIFETIME;
    }
    SpawnProjectile(player_id, def.projectile, origin + direction * 0.5f, velocity, life);
    shot.p1 = origin + direction;
    EmitEvent(shot);
}

UInt16 World::SpawnProjectile(
    const int owner, const ProjectileKind kind, const FVec3 &position, const FVec3 &velocity,
    const Float life
)
{
    Projectile projectile;
    projectile.id = nextProjectileId++;
    if (nextProjectileId == 0)
    {
        nextProjectileId = 1;
    }
    projectile.owner = static_cast<UInt8>(owner);
    projectile.kind = kind;
    projectile.position = position;
    projectile.velocity = velocity;
    projectile.life = life;
    projectiles.push_back(projectile);
    return projectile.id;
}

Bool World::CanThrow(const Player &player, const Bool mine) const
{
    return player.alive && player.throwCooldown <= 0.0f && match.phase == MatchPhase::PLAYING &&
           (mine ? player.mines : player.grenades) > 0;
}

Bool World::HostThrow(const int player_id, const Bool mine, const FVec3 &origin, const FVec3 &direction)
{
    Player &player = players[player_id];
    if (!CanThrow(player, mine))
    {
        return false;
    }
    player.throwCooldown = THROW_COOLDOWN;
    FVec3 start = origin;
    // Keep the throw from starting inside a wall when standing right against one.
    RayHit wall;
    if (arena.RayCast(origin, direction, 0.6f, wall))
    {
        start = origin + direction * std::max(0.0f, wall.distance - 0.25f);
    }
    else
    {
        start = origin + direction * 0.5f;
    }
    FVec3 inherited = player.move.velocity * 0.5f;
    inherited.y = std::max(0.0f, inherited.y) * 0.5f;
    if (mine)
    {
        player.mines = static_cast<Int16>(player.mines - 1);
        // Only so many mines per player: the oldest one fizzles out.
        int count = 0;
        int oldest = -1;
        for (int i = 0; i < static_cast<int>(projectiles.size()); ++i)
        {
            if (projectiles[i].kind == ProjectileKind::MINE && projectiles[i].owner == player_id)
            {
                ++count;
                if (oldest < 0 || projectiles[i].age > projectiles[oldest].age)
                {
                    oldest = i;
                }
            }
        }
        if (count >= MAX_ACTIVE_MINES && oldest >= 0)
        {
            projectiles.erase(projectiles.begin() + oldest);
        }
        SpawnProjectile(player_id, ProjectileKind::MINE, start,
                        direction * 7.0f + FVec3{0.0f, 2.5f, 0.0f} + inherited, MINE_LIFETIME);
    }
    else
    {
        player.grenades = static_cast<Int16>(player.grenades - 1);
        SpawnProjectile(player_id, ProjectileKind::GRENADE, start,
                        direction * 19.0f + FVec3{0.0f, 4.5f, 0.0f} + inherited, GRENADE_FUSE);
    }
    GameEvent event;
    event.type = EventType::THROW;
    event.a = static_cast<UInt8>(player_id);
    event.b = mine ? 1 : 0;
    event.p0 = start;
    EmitEvent(event);
    return true;
}

void World::HostRespawn(const int player_id)
{
    Player &player = players[player_id];
    Float yaw = 0.0f;
    const FVec3 position = PickSpawn(player_id, yaw);
    player.move = MoveState{};
    player.move.position = position;
    player.move.onGround = true;
    player.yaw = yaw;
    player.pitch = 0.0f;
    player.renderPosition = position;
    player.renderYaw = yaw;
    player.health = START_HEALTH;
    player.armor = 0;
    player.alive = true;
    player.deadTime = 0.0f;
    player.wantsRespawn = false;
    player.weapon = WeaponId::RIFLE;
    player.hasWeapon = {};
    player.hasWeapon[static_cast<int>(WeaponId::RIFLE)] = true;
    player.ammo = {};
    player.ammo[static_cast<int>(WeaponId::RIFLE)] = GetWeaponDef(WeaponId::RIFLE).startAmmo;
    player.grenades = 2;
    player.mines = 1;
    player.throwCooldown = 0.5f;
    player.fireCooldown = 0.3f;
    player.spawnCount = static_cast<UInt8>(player.spawnCount + 1);
    player.brain.path.clear();
    player.brain.hasGoal = false;
    player.brain.target = -1;
    GameEvent event;
    event.type = EventType::SPAWN;
    event.a = static_cast<UInt8>(player_id);
    event.p0 = position;
    EmitEvent(event);
}

void World::RestartMatch()
{
    match.phase = MatchPhase::PLAYING;
    match.timeRemaining = settings.timeLimitSeconds;
    match.winner = -1;
    projectiles.clear();
    for (Pickup &pickup : pickups)
    {
        pickup.available = true;
        pickup.respawnTimer = 0.0f;
    }
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (!players[i].active)
        {
            continue;
        }
        players[i].frags = 0;
        players[i].deaths = 0;
        HostRespawn(i);
    }
    GameEvent event;
    event.type = EventType::MATCH_START;
    EmitEvent(event);
}

void World::EmitEvent(GameEvent event)
{
    event.sequence = nextEventSequence++;
    if (nextEventSequence == 0)
    {
        nextEventSequence = 1;
    }
    event.time = time;
    eventLog.push_back(event);
    newEvents.push_back(event);
}

std::vector<GameEvent> World::TakeNewEvents()
{
    std::vector<GameEvent> events;
    events.swap(newEvents);
    return events;
}

void World::StepPlayers(
    const Float delta_seconds, const MoveInput &local_input, const FireRequest &local_fire
)
{
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        Player &player = players[i];
        if (!player.active)
        {
            continue;
        }
        player.fireCooldown = std::max(0.0f, player.fireCooldown - delta_seconds);
        player.throwCooldown = std::max(0.0f, player.throwCooldown - delta_seconds);
        player.fireFlash = std::max(0.0f, player.fireFlash - delta_seconds);

        if (!player.alive)
        {
            player.deadTime += delta_seconds;
            if (match.phase != MatchPhase::PLAYING)
            {
                continue;
            }
            const Bool requested = player.wantsRespawn && player.deadTime > 1.0f;
            Bool respawn = false;
            switch (player.control)
            {
            case PlayerControl::LOCAL:
                respawn = requested || player.deadTime > 5.0f;
                break;
            case PlayerControl::BOT:
                respawn = player.deadTime > 2.0f;
                break;
            case PlayerControl::REMOTE:
                respawn = requested || player.deadTime > 6.0f;
                break;
            default:
                break;
            }
            if (respawn)
            {
                HostRespawn(i);
            }
            continue;
        }

        if (player.health > MAX_HEALTH)
        {
            player.healthDecayTimer += delta_seconds;
            while (player.healthDecayTimer >= 1.0f && player.health > MAX_HEALTH)
            {
                player.healthDecayTimer -= 1.0f;
                player.health = static_cast<Int16>(player.health - 1);
            }
        }
        else
        {
            player.healthDecayTimer = 0.0f;
        }

        if (player.control == PlayerControl::LOCAL)
        {
            MoveEvents events;
            StepMovement(player.move, local_input, delta_seconds, arena, events);
            localMoveEvents = events;
            if (local_fire.fire && CanFire(player))
            {
                ConsumeShot(player);
                HostFire(i, local_fire.origin, local_fire.direction, random.Next());
            }
            if (local_fire.throwGrenade || local_fire.placeMine)
            {
                HostThrow(i, local_fire.placeMine, local_fire.origin, local_fire.throwDirection);
            }
        }
        else if (player.control == PlayerControl::BOT)
        {
            MoveInput input;
            FireRequest fire;
            UpdateBot(*this, i, delta_seconds, input, fire);
            MoveEvents events;
            StepMovement(player.move, input, delta_seconds, arena, events);
            if (fire.fire && CanFire(player))
            {
                ConsumeShot(player);
                const FVec3 direction = ApplySpread(player, fire.direction, random.Next());
                HostFire(i, fire.origin, direction, random.Next());
            }
            if (fire.throwGrenade || fire.placeMine)
            {
                HostThrow(i, fire.placeMine, fire.origin, fire.throwDirection);
            }
        }

        if (player.alive && player.move.position.y < -10.0f)
        {
            Kill(i, i, SourceOf(player.weapon), false);
        }
    }
}

void World::StepProjectiles(const Float delta_seconds)
{
    std::vector<Projectile> survivors;
    survivors.reserve(projectiles.size());
    // Explosions can set off other explosives, so blasts are collected and applied after the
    // loop over a stable list.
    std::vector<std::pair<Projectile, FVec3>> blasts;
    for (Projectile projectile : projectiles)
    {
        projectile.life -= delta_seconds;
        projectile.age += delta_seconds;
        const ProjectileKind kind = projectile.kind;
        const Bool explosive = GetBlastDef(kind).radius > 0.0f;

        if (projectile.detonate)
        {
            blasts.emplace_back(projectile, projectile.position);
            continue;
        }

        if (kind == ProjectileKind::MINE && projectile.resting)
        {
            if (projectile.life <= 0.0f)
            {
                continue;
            }
            if (!projectile.armed && projectile.age >= MINE_ARM_SECONDS)
            {
                projectile.armed = true;
                GameEvent event;
                event.type = EventType::MINE_ARMED;
                event.a = projectile.owner;
                event.p0 = projectile.position;
                EmitEvent(event);
            }
            Bool triggered = false;
            for (int i = 0; projectile.armed && i < MAX_PLAYERS && !triggered; ++i)
            {
                const Player &player = players[i];
                if (!player.active || !player.alive || i == projectile.owner)
                {
                    continue;
                }
                const FVec3 center = player.move.position + FVec3{0.0f, 0.8f, 0.0f};
                triggered = math::GetLength(center - projectile.position) < MINE_TRIGGER_RADIUS &&
                            arena.HasLineOfSight(projectile.position + FVec3{0.0f, 0.3f, 0.0f}, center);
            }
            if (triggered)
            {
                blasts.emplace_back(projectile, projectile.position);
                continue;
            }
            survivors.push_back(projectile);
            continue;
        }

        const Bool falls = kind == ProjectileKind::FRAG || kind == ProjectileKind::GRENADE ||
                           kind == ProjectileKind::MINE;
        if (projectile.resting)
        {
            if (projectile.life <= 0.0f)
            {
                blasts.emplace_back(projectile, projectile.position);
                continue;
            }
            survivors.push_back(projectile);
            continue;
        }
        if (falls)
        {
            projectile.velocity.y -= GRAVITY * delta_seconds;
        }

        const FVec3 motion = projectile.velocity * delta_seconds;
        const Float distance = math::GetLength(motion);
        const FVec3 direction = motion * (distance > 0.0f ? 1.0f / distance : 0.0f);
        Float best = distance;
        Bool hit_arena = false;
        int hit_player = -1;
        Bool headshot = false;
        FVec3 normal{0.0f, 1.0f, 0.0f};
        RayHit arena_hit;
        if (distance > 0.0f && arena.RayCast(projectile.position, direction, distance, arena_hit))
        {
            best = arena_hit.distance;
            hit_arena = true;
            normal = arena_hit.normal;
        }
        // Mines in flight pass through players; a ricocheting disc can come back for its owner.
        const Bool hits_players = kind != ProjectileKind::MINE;
        for (int i = 0; hits_players && distance > 0.0f && i < MAX_PLAYERS; ++i)
        {
            const Player &player = players[i];
            const Bool owner_safe = kind != ProjectileKind::DISC || projectile.bounces == 0;
            if (!player.active || !player.alive || (i == projectile.owner && owner_safe))
            {
                continue;
            }
            const primitive::AxisAlignedBox bounds = primitive::GetExpanded(
                PlayerBounds(player.move.position), FVec3{0.15f, 0.1f, 0.15f}
            );
            Float player_distance = 0.0f;
            FVec3 player_normal;
            if (primitive::IntersectRay(
                    bounds, projectile.position, direction, best, player_distance, player_normal
                ) &&
                player_distance <= best)
            {
                best = player_distance;
                hit_player = i;
                hit_arena = false;
                normal = player_normal;
                const Float hit_height = projectile.position.y + direction.y * player_distance - player.move.position.y;
                headshot = hit_height > PLAYER_HEAD_LINE;
            }
        }
        const FVec3 contact = projectile.position + direction * std::max(0.0f, best - 0.05f);

        if (kind == ProjectileKind::DISC)
        {
            if (hit_player >= 0)
            {
                const WeaponDef &def = GetWeaponDef(WeaponId::RIPPER);
                const Float multiplier = headshot ? def.headshotMultiplier : 1.0f;
                ApplyImpulse(hit_player, direction * def.knockback);
                Damage(hit_player, projectile.owner, static_cast<Int16>(static_cast<Float>(def.damage) * multiplier),
                       DamageSource::RIPPER, headshot);
                continue;
            }
            if (projectile.life <= 0.0f)
            {
                continue;
            }
            if (hit_arena)
            {
                if (projectile.bounces >= DISC_MAX_BOUNCES)
                {
                    GameEvent event;
                    event.type = EventType::BOUNCE;
                    event.b = static_cast<UInt8>(kind);
                    event.c = 1;
                    event.p0 = contact;
                    event.p1 = normal;
                    EmitEvent(event);
                    continue;
                }
                projectile.bounces = static_cast<UInt8>(projectile.bounces + 1);
                projectile.velocity = projectile.velocity -
                                      normal * (2.0f * math::GetDotProduct(projectile.velocity, normal));
                projectile.position = contact + normal * 0.05f;
                GameEvent event;
                event.type = EventType::BOUNCE;
                event.b = static_cast<UInt8>(kind);
                event.p0 = contact;
                event.p1 = normal;
                EmitEvent(event);
                survivors.push_back(projectile);
                continue;
            }
            projectile.position = projectile.position + motion;
            survivors.push_back(projectile);
            continue;
        }

        if (kind == ProjectileKind::NOVA || (kind == ProjectileKind::FRAG && hit_player >= 0))
        {
            if (hit_arena || hit_player >= 0 || projectile.life <= 0.0f)
            {
                blasts.emplace_back(projectile, hit_arena || hit_player >= 0 ? contact : projectile.position);
                continue;
            }
            projectile.position = projectile.position + motion;
            survivors.push_back(projectile);
            continue;
        }

        if (explosive && kind != ProjectileKind::MINE && projectile.life <= 0.0f)
        {
            blasts.emplace_back(projectile, projectile.position);
            continue;
        }
        if (kind == ProjectileKind::MINE && projectile.life <= 0.0f)
        {
            continue;
        }

        if (hit_arena || hit_player >= 0)
        {
            if (kind == ProjectileKind::MINE)
            {
                // Mines stick to whatever they land on.
                projectile.position = contact + normal * 0.08f;
                projectile.velocity = FVec3{};
                projectile.resting = true;
                projectile.age = 0.0f;
                projectile.life = MINE_LIFETIME;
                GameEvent event;
                event.type = EventType::BOUNCE;
                event.b = static_cast<UInt8>(kind);
                event.p0 = projectile.position;
                event.p1 = normal;
                EmitEvent(event);
                survivors.push_back(projectile);
                continue;
            }
            const Float into = math::GetDotProduct(projectile.velocity, normal);
            const FVec3 reflected = projectile.velocity - normal * (1.6f * into);
            // Rolling friction along the surface.
            projectile.velocity = reflected * 0.72f;
            projectile.position = contact + normal * 0.06f;
            projectile.bounces = static_cast<UInt8>(std::min(255, projectile.bounces + 1));
            if (-into > 3.0f)
            {
                GameEvent event;
                event.type = EventType::BOUNCE;
                event.b = static_cast<UInt8>(kind);
                event.p0 = contact;
                event.p1 = normal;
                EmitEvent(event);
            }
            if (normal.y > 0.7f && math::GetLength(projectile.velocity) < 1.6f)
            {
                projectile.resting = true;
                projectile.velocity = FVec3{};
            }
            survivors.push_back(projectile);
            continue;
        }
        projectile.position = projectile.position + motion;
        if (projectile.position.y < -20.0f)
        {
            continue;
        }
        survivors.push_back(projectile);
    }
    projectiles.swap(survivors);

    for (const auto &[projectile, position] : blasts)
    {
        Explode(projectile, position);
    }
}

void World::Explode(const Projectile &projectile, const FVec3 &position)
{
    const BlastDef &def = GetBlastDef(projectile.kind);
    GameEvent event;
    event.type = EventType::EXPLOSION;
    event.a = projectile.owner;
    event.b = static_cast<UInt8>(projectile.kind);
    event.p0 = position;
    EmitEvent(event);

    // Grenades and mines caught in the blast go off too (a moment later, on the next tick).
    for (Projectile &other : projectiles)
    {
        if (other.id == projectile.id || GetBlastDef(other.kind).radius <= 0.0f ||
            other.kind == ProjectileKind::NOVA)
        {
            continue;
        }
        if (math::GetLength(other.position - position) < def.radius * 0.8f &&
            arena.HasLineOfSight(position, other.position + FVec3{0.0f, 0.2f, 0.0f}))
        {
            other.detonate = true;
        }
    }

    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        const Player &player = players[i];
        if (!player.active || !player.alive)
        {
            continue;
        }
        const primitive::AxisAlignedBox bounds = PlayerBounds(player.move.position);
        const FVec3 closest{
            math::Clamp(position.x, bounds.min.x, bounds.max.x),
            math::Clamp(position.y, bounds.min.y, bounds.max.y),
            math::Clamp(position.z, bounds.min.z, bounds.max.z)
        };
        const Float distance = math::GetLength(closest - position);
        if (distance > def.radius)
        {
            continue;
        }
        const FVec3 center = player.move.position + FVec3{0.0f, 0.9f, 0.0f};
        if (distance > 0.6f && !arena.HasLineOfSight(position, center))
        {
            continue;
        }
        const Bool self = i == projectile.owner;
        const Float scale = 1.0f - 0.65f * (distance / def.radius);
        Float amount = static_cast<Float>(def.damage) * scale;
        if (self)
        {
            amount *= 0.45f;
        }
        const FVec3 push_direction =
            math::GetNormalized(center - position + FVec3{0.0f, 0.4f, 0.0f});
        ApplyImpulse(i, push_direction * (def.knockback * scale * (self ? 1.25f : 1.0f)));
        Damage(i, projectile.owner, static_cast<Int16>(amount), def.source, false);
    }
}

void World::ApplyImpulse(const int player_id, const FVec3 &impulse)
{
    Player &player = players[player_id];
    FVec3 push = impulse;
    if (math::GetLength(push) > 3.0f)
    {
        push.y = std::max(push.y, 2.5f);
    }
    if (player.control == PlayerControl::LOCAL || player.control == PlayerControl::BOT)
    {
        player.move.velocity = player.move.velocity + push;
        if (push.y > 0.5f)
        {
            player.move.onGround = false;
        }
        return;
    }
    if (player.control == PlayerControl::REMOTE)
    {
        GameEvent event;
        event.type = EventType::IMPULSE;
        event.a = static_cast<UInt8>(player_id);
        event.p0 = push;
        EmitEvent(event);
    }
}

void World::Damage(
    const int victim, const int attacker, const Int16 amount, const DamageSource source,
    const Bool headshot
)
{
    Player &player = players[victim];
    if (!player.alive || match.phase != MatchPhase::PLAYING || amount <= 0)
    {
        return;
    }
    const auto absorbed = static_cast<Int16>(
        std::min<int>(player.armor, static_cast<int>(static_cast<Float>(amount) * 2.0f / 3.0f))
    );
    player.armor = static_cast<Int16>(player.armor - absorbed);
    player.health = static_cast<Int16>(player.health - (amount - absorbed));
    player.lastDamageFrom = static_cast<Int16>(attacker);
    player.lastDamageTime = time;

    GameEvent event;
    event.type = EventType::HIT;
    event.a = ToEventPlayer(attacker);
    event.b = static_cast<UInt8>(victim);
    event.c = static_cast<UInt8>(std::min<int>(amount, 255));
    event.p0 = player.move.position + FVec3{0.0f, 1.2f, 0.0f};
    // Where the damage came from, so clients can spray blood away from it.
    event.p1 = attacker >= 0 && attacker != victim ? EyePosition(players[attacker].move)
                                                   : event.p0 + FVec3{0.0f, -1.0f, 0.0f};
    EmitEvent(event);

    if (player.health <= 0)
    {
        Kill(victim, attacker, source, headshot);
    }
}

void World::Kill(const int victim, const int attacker, const DamageSource source, const Bool headshot)
{
    Player &player = players[victim];
    player.alive = false;
    player.deadTime = 0.0f;
    player.wantsRespawn = false;
    player.health = std::min<Int16>(player.health, 0);
    player.deaths = static_cast<Int16>(player.deaths + 1);

    const Bool suicide = attacker < 0 || attacker == victim;
    if (suicide)
    {
        player.frags = static_cast<Int16>(player.frags - 1);
    }
    else
    {
        players[attacker].frags = static_cast<Int16>(players[attacker].frags + 1);
    }

    GameEvent event;
    event.type = EventType::KILL;
    event.a = suicide ? static_cast<UInt8>(victim) : ToEventPlayer(attacker);
    event.b = static_cast<UInt8>(victim);
    event.c = static_cast<UInt8>(static_cast<UInt8>(source) | (headshot ? 0x80 : 0x00));
    event.p0 = player.move.position;
    event.p1 = player.move.velocity;
    EmitEvent(event);

    if (!suicide && players[attacker].frags >= settings.fragLimit)
    {
        match.phase = MatchPhase::ENDED;
        match.endTimer = MATCH_END_SECONDS;
        match.winner = attacker;
        GameEvent end;
        end.type = EventType::MATCH_END;
        end.a = static_cast<UInt8>(attacker);
        EmitEvent(end);
    }
}

void World::StepMatch(const Float delta_seconds)
{
    if (match.phase == MatchPhase::PLAYING)
    {
        match.timeRemaining -= delta_seconds;
        if (match.timeRemaining <= 0.0f)
        {
            match.timeRemaining = 0.0f;
            int best = -1;
            for (int i = 0; i < MAX_PLAYERS; ++i)
            {
                if (players[i].active && (best < 0 || players[i].frags > players[best].frags))
                {
                    best = i;
                }
            }
            match.phase = MatchPhase::ENDED;
            match.endTimer = MATCH_END_SECONDS;
            match.winner = best;
            GameEvent end;
            end.type = EventType::MATCH_END;
            end.a = ToEventPlayer(best);
            EmitEvent(end);
        }
        return;
    }
    match.endTimer -= delta_seconds;
    if (match.endTimer <= 0.0f)
    {
        RestartMatch();
    }
}

Bool World::TryGivePickup(Player &player, const PickupKind kind)
{
    const auto give_ammo = [&player](const WeaponId weapon, const Int16 amount)
    {
        const int index = static_cast<int>(weapon);
        const Int16 max_ammo = GetWeaponDef(weapon).maxAmmo;
        if (player.ammo[index] >= max_ammo)
        {
            return false;
        }
        player.ammo[index] = std::min<Int16>(max_ammo, static_cast<Int16>(player.ammo[index] + amount));
        return true;
    };
    const auto give_weapon = [&player, &give_ammo](const WeaponId weapon)
    {
        const int index = static_cast<int>(weapon);
        const Bool is_new = !player.hasWeapon[index];
        player.hasWeapon[index] = true;
        const Bool got_ammo = give_ammo(weapon, GetWeaponDef(weapon).pickupAmmo);
        if (is_new && player.control != PlayerControl::REMOTE)
        {
            player.weapon = weapon;
        }
        return is_new || got_ammo;
    };

    switch (kind)
    {
    case PickupKind::HEALTH:
        if (player.health >= MAX_HEALTH)
        {
            return false;
        }
        player.health = std::min<Int16>(MAX_HEALTH, static_cast<Int16>(player.health + 25));
        return true;
    case PickupKind::MEGA_HEALTH:
        if (player.health >= MAX_MEGA_HEALTH)
        {
            return false;
        }
        player.health = std::min<Int16>(MAX_MEGA_HEALTH, static_cast<Int16>(player.health + 100));
        return true;
    case PickupKind::ARMOR:
        if (player.armor >= MAX_ARMOR)
        {
            return false;
        }
        player.armor = std::min<Int16>(MAX_ARMOR, static_cast<Int16>(player.armor + 50));
        return true;
    case PickupKind::WEAPON_NOVA:
        return give_weapon(WeaponId::NOVA);
    case PickupKind::WEAPON_RAIL:
        return give_weapon(WeaponId::RAIL);
    case PickupKind::AMMO_RIFLE:
        return give_ammo(WeaponId::RIFLE, GetWeaponDef(WeaponId::RIFLE).pickupAmmo);
    case PickupKind::AMMO_NOVA:
        return give_ammo(WeaponId::NOVA, GetWeaponDef(WeaponId::NOVA).pickupAmmo);
    case PickupKind::AMMO_RAIL:
        return give_ammo(WeaponId::RAIL, GetWeaponDef(WeaponId::RAIL).pickupAmmo);
    case PickupKind::WEAPON_SCATTER:
        return give_weapon(WeaponId::SCATTER);
    case PickupKind::WEAPON_FRAG:
        return give_weapon(WeaponId::FRAG);
    case PickupKind::WEAPON_RIPPER:
        return give_weapon(WeaponId::RIPPER);
    case PickupKind::AMMO_SCATTER:
        return give_ammo(WeaponId::SCATTER, GetWeaponDef(WeaponId::SCATTER).pickupAmmo);
    case PickupKind::AMMO_FRAG:
        return give_ammo(WeaponId::FRAG, GetWeaponDef(WeaponId::FRAG).pickupAmmo);
    case PickupKind::AMMO_RIPPER:
        return give_ammo(WeaponId::RIPPER, GetWeaponDef(WeaponId::RIPPER).pickupAmmo);
    case PickupKind::GRENADES:
        if (player.grenades >= MAX_GRENADES)
        {
            return false;
        }
        player.grenades = std::min<Int16>(MAX_GRENADES, static_cast<Int16>(player.grenades + 2));
        return true;
    case PickupKind::MINES:
        if (player.mines >= MAX_MINES)
        {
            return false;
        }
        player.mines = std::min<Int16>(MAX_MINES, static_cast<Int16>(player.mines + 2));
        return true;
    default:
        return false;
    }
}

void World::StepPickups(const Float delta_seconds)
{
    for (size_t index = 0; index < pickups.size(); ++index)
    {
        Pickup &pickup = pickups[index];
        if (!pickup.available)
        {
            pickup.respawnTimer -= delta_seconds;
            if (pickup.respawnTimer <= 0.0f)
            {
                pickup.available = true;
            }
            continue;
        }
        const FVec3 pickup_center = pickup.position + FVec3{0.0f, 0.6f, 0.0f};
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            Player &player = players[i];
            if (!player.active || !player.alive)
            {
                continue;
            }
            const FVec3 center = player.move.position + FVec3{0.0f, 0.9f, 0.0f};
            if (math::GetLength(center - pickup_center) > PICKUP_RADIUS)
            {
                continue;
            }
            if (!TryGivePickup(player, pickup.kind))
            {
                continue;
            }
            pickup.available = false;
            pickup.respawnTimer = PickupRespawnTime(pickup.kind);
            GameEvent event;
            event.type = EventType::PICKUP;
            event.a = static_cast<UInt8>(i);
            event.b = static_cast<UInt8>(pickup.kind);
            event.c = static_cast<UInt8>(index);
            event.p0 = pickup.position;
            EmitEvent(event);
            break;
        }
    }
}

FVec3 World::PickSpawn(const int player_id, Float &yaw)
{
    int best_index = 0;
    Float best_score = -1e9f;
    for (int s = 0; s < static_cast<int>(arena.spawnPoints.size()); ++s)
    {
        const FVec3 &spawn = arena.spawnPoints[s].position;
        Float nearest = 100.0f;
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            const Player &player = players[i];
            if (i == player_id || !player.active || !player.alive)
            {
                continue;
            }
            nearest = std::min(nearest, math::GetLength(player.move.position - spawn));
        }
        if (nearest < 1.5f)
        {
            continue;
        }
        const Float score = nearest + random.NextRange(0.0f, 8.0f);
        if (score > best_score)
        {
            best_score = score;
            best_index = s;
        }
    }
    yaw = arena.spawnPoints[best_index].yaw;
    return arena.spawnPoints[best_index].position;
}
} // namespace evo
