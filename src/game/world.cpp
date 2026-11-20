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
};

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
        return 12.0f;
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
    default:
        return "Pickup";
    }
}

void World::Initialize(const MatchSettings &match_settings)
{
    settings = match_settings;
    arena.BuildFoundry();
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
    const FVec3 right = Normalize(Cross(up_hint, direction));
    const FVec3 up = Cross(direction, right);
    const Float a = (static_cast<Float>(HashU32(seed) & 0xFFFF) / 65535.0f - 0.5f) * 2.0f;
    const Float b = (static_cast<Float>(HashU32(seed * 7919U + 17U) & 0xFFFF) / 65535.0f - 0.5f) * 2.0f;
    return Normalize(Add(direction, Add(Scale(right, a * spread), Scale(up, b * spread))));
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
        const Aabb bounds = Expand(PlayerBounds(player.move.position), FVec3{0.08f, 0.05f, 0.08f});
        Float distance = 0.0f;
        FVec3 normal;
        if (RayAabb(origin, direction, bounds, best, distance, normal) && distance < best)
        {
            best = distance;
            result.hitPlayer = i;
            result.normal = normal;
            const Float hit_height = origin.y + direction.y * distance - player.move.position.y;
            result.headshot = hit_height > PLAYER_HEAD_LINE;
        }
    }
    result.end = Add(origin, Scale(direction, best));
    return result;
}

void World::HostFire(const int player_id, const FVec3 &origin, const FVec3 &direction)
{
    Player &shooter = players[player_id];
    const WeaponDef &def = GetWeaponDef(shooter.weapon);
    shooter.fireFlash = 0.08f;
    GameEvent shot;
    shot.type = EventType::SHOT;
    shot.a = static_cast<UInt8>(player_id);
    shot.c = static_cast<UInt8>(shooter.weapon);
    shot.p0 = origin;

    if (def.hitscan)
    {
        const TraceResult trace = Trace(origin, direction, MAX_TRACE_DISTANCE, player_id);
        shot.p1 = trace.end;
        EmitEvent(shot);
        if (trace.hitPlayer >= 0)
        {
            const Float multiplier = trace.headshot ? def.headshotMultiplier : 1.0f;
            const auto amount = static_cast<Int16>(static_cast<Float>(def.damage) * multiplier);
            ApplyImpulse(trace.hitPlayer, Scale(direction, def.knockback));
            Damage(trace.hitPlayer, player_id, amount, shooter.weapon, trace.headshot && def.headshotMultiplier > 1.0f);
        }
        return;
    }

    Projectile projectile;
    projectile.id = nextProjectileId++;
    if (nextProjectileId == 0)
    {
        nextProjectileId = 1;
    }
    projectile.owner = static_cast<UInt8>(player_id);
    projectile.position = Add(origin, Scale(direction, 0.5f));
    projectile.velocity = Scale(direction, def.projectileSpeed);
    projectile.life = 4.0f;
    projectiles.push_back(projectile);
    shot.p1 = Add(origin, direction);
    EmitEvent(shot);
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
    player.hasWeapon = {true, false, false};
    player.ammo = {GetWeaponDef(WeaponId::RIFLE).startAmmo, 0, 0};
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
                HostFire(i, local_fire.origin, local_fire.direction);
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
                HostFire(i, fire.origin, direction);
            }
        }

        if (player.alive && player.move.position.y < -10.0f)
        {
            Kill(i, i, player.weapon, false);
        }
    }
}

void World::StepProjectiles(const Float delta_seconds)
{
    std::vector<Projectile> survivors;
    survivors.reserve(projectiles.size());
    for (Projectile projectile : projectiles)
    {
        projectile.life -= delta_seconds;
        const FVec3 motion = Scale(projectile.velocity, delta_seconds);
        const Float distance = Length(motion);
        const FVec3 direction = Scale(motion, distance > 0.0f ? 1.0f / distance : 0.0f);
        Float best = distance;
        Bool hit = false;
        RayHit arena_hit;
        if (arena.RayCast(projectile.position, direction, distance, arena_hit))
        {
            best = arena_hit.distance;
            hit = true;
        }
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            const Player &player = players[i];
            if (!player.active || !player.alive || i == projectile.owner)
            {
                continue;
            }
            const Aabb bounds = Expand(PlayerBounds(player.move.position), FVec3{0.15f, 0.1f, 0.15f});
            Float player_distance = 0.0f;
            FVec3 normal;
            if (RayAabb(projectile.position, direction, bounds, best, player_distance, normal) &&
                player_distance <= best)
            {
                best = player_distance;
                hit = true;
            }
        }
        if (hit || projectile.life <= 0.0f)
        {
            const FVec3 position =
                hit ? Add(projectile.position, Scale(direction, std::max(0.0f, best - 0.05f)))
                    : projectile.position;
            Explode(projectile, position);
            continue;
        }
        projectile.position = Add(projectile.position, motion);
        survivors.push_back(projectile);
    }
    projectiles.swap(survivors);
}

void World::Explode(const Projectile &projectile, const FVec3 &position)
{
    const WeaponDef &def = GetWeaponDef(WeaponId::NOVA);
    GameEvent event;
    event.type = EventType::EXPLOSION;
    event.a = projectile.owner;
    event.p0 = position;
    EmitEvent(event);

    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        const Player &player = players[i];
        if (!player.active || !player.alive)
        {
            continue;
        }
        const Aabb bounds = PlayerBounds(player.move.position);
        const FVec3 closest{
            Clamp(position.x, bounds.min.x, bounds.max.x),
            Clamp(position.y, bounds.min.y, bounds.max.y),
            Clamp(position.z, bounds.min.z, bounds.max.z)
        };
        const Float distance = Length(Sub(closest, position));
        if (distance > def.splashRadius)
        {
            continue;
        }
        const FVec3 center = Add(player.move.position, FVec3{0.0f, 0.9f, 0.0f});
        if (distance > 0.6f && !arena.HasLineOfSight(position, center))
        {
            continue;
        }
        const Bool self = i == projectile.owner;
        const Float scale = 1.0f - 0.65f * (distance / def.splashRadius);
        Float amount = static_cast<Float>(def.splashDamage) * scale;
        if (self)
        {
            amount *= 0.45f;
        }
        const FVec3 push_direction = Normalize(Add(Sub(center, position), FVec3{0.0f, 0.4f, 0.0f}));
        ApplyImpulse(i, Scale(push_direction, def.knockback * scale * (self ? 1.25f : 1.0f)));
        Damage(i, projectile.owner, static_cast<Int16>(amount), WeaponId::NOVA, false);
    }
}

void World::ApplyImpulse(const int player_id, const FVec3 &impulse)
{
    Player &player = players[player_id];
    FVec3 push = impulse;
    if (Length(push) > 3.0f)
    {
        push.y = std::max(push.y, 2.5f);
    }
    if (player.control == PlayerControl::LOCAL || player.control == PlayerControl::BOT)
    {
        player.move.velocity = Add(player.move.velocity, push);
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
    const int victim, const int attacker, const Int16 amount, const WeaponId weapon,
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
    event.p0 = Add(player.move.position, FVec3{0.0f, 1.2f, 0.0f});
    EmitEvent(event);

    if (player.health <= 0)
    {
        Kill(victim, attacker, weapon, headshot);
    }
}

void World::Kill(const int victim, const int attacker, const WeaponId weapon, const Bool headshot)
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
    event.c = static_cast<UInt8>(static_cast<UInt8>(weapon) | (headshot ? 0x80 : 0x00));
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
        const FVec3 pickup_center = Add(pickup.position, FVec3{0.0f, 0.6f, 0.0f});
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            Player &player = players[i];
            if (!player.active || !player.alive)
            {
                continue;
            }
            const FVec3 center = Add(player.move.position, FVec3{0.0f, 0.9f, 0.0f});
            if (Length(Sub(center, pickup_center)) > PICKUP_RADIUS)
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
            nearest = std::min(nearest, Length(Sub(player.move.position, spawn)));
        }
        if (nearest < 1.5f)
        {
            continue;
        }
        const Float score = nearest + random.Range(0.0f, 8.0f);
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
