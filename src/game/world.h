#pragma once
#include "game/arena.h"
#include "game/movement.h"

#include "compiled/warning_off.h"
#include <array>
#include <deque>
#include <string>
#include <vector>
#include "compiled/warning_on.h"

namespace evo
{
constexpr int MAX_PLAYERS = 12;
constexpr int MAX_NAME_LENGTH = 15;
constexpr Int16 START_HEALTH = 100;
constexpr Int16 MAX_HEALTH = 100;
constexpr Int16 MAX_MEGA_HEALTH = 199;
constexpr Int16 MAX_ARMOR = 150;
constexpr Int16 MAX_GRENADES = 4;
constexpr Int16 MAX_MINES = 3;
// How many of a player's mines can be in the arena at once; placing another removes the oldest.
constexpr int MAX_ACTIVE_MINES = 3;

enum class WeaponId : UInt8
{
    RIFLE,
    NOVA,
    RAIL,
    SCATTER,
    FRAG,
    RIPPER,
    COUNT
};

constexpr int WEAPON_COUNT = static_cast<int>(WeaponId::COUNT);

// What kind of thing is in flight (or lying in the arena).
enum class ProjectileKind : UInt8
{
    NOVA,
    // Fired by the Frag Launcher: bounces, bursts on a direct hit or when its fuse runs out.
    FRAG,
    // Thrown by hand: bounces, bursts when its fuse runs out.
    GRENADE,
    MINE,
    // Ripper disc: ricochets off walls.
    DISC,
    COUNT
};

struct WeaponDef
{
    const Char *name;
    Float fireInterval;
    Int16 damage;
    Int16 maxAmmo;
    Int16 startAmmo;
    Int16 pickupAmmo;
    Bool hitscan;
    Float spread;
    Float projectileSpeed;
    Float splashRadius;
    Int16 splashDamage;
    Float knockback;
    Float headshotMultiplier;
    FVec3 color;
    // Hitscan pellets per shot (Scattergun) and how far they fan out.
    int pellets = 1;
    Float pelletSpread = 0.0f;
    ProjectileKind projectile = ProjectileKind::NOVA;
};

const WeaponDef &GetWeaponDef(WeaponId weapon);

// What dealt the damage, for the kill feed. The first values match WeaponId.
enum class DamageSource : UInt8
{
    RIFLE,
    NOVA,
    RAIL,
    SCATTER,
    FRAG,
    RIPPER,
    GRENADE,
    MINE,
    COUNT
};

[[nodiscard]]
inline DamageSource SourceOf(const WeaponId weapon)
{
    return static_cast<DamageSource>(weapon);
}

[[nodiscard]]
const Char *DamageSourceName(DamageSource source);

[[nodiscard]]
Bool IsExplosive(DamageSource source);

[[nodiscard]]
FVec3 DamageSourceColor(DamageSource source);

// Blast settings of things that explode.
struct BlastDef
{
    Float radius;
    Int16 damage;
    Float knockback;
    DamageSource source;
};

[[nodiscard]]
const BlastDef &GetBlastDef(ProjectileKind kind);

enum class PlayerControl : UInt8
{
    NONE,
    LOCAL,
    BOT,
    REMOTE,
    REPLICA
};

struct BotBrain
{
    int target = -1;
    Float targetLastSeen = -100.0f;
    FVec3 targetLastPosition;
    std::vector<int> path;
    int pathIndex = 0;
    FVec3 goalPosition;
    Bool hasGoal = false;
    Float repathTimer = 0.0f;
    Float stuckTimer = 0.0f;
    Float strafeTimer = 0.0f;
    Float strafeDirection = 1.0f;
    Float reactionTimer = 0.0f;
    Float thinkTimer = 0.0f;
    Float dodgeTimer = 3.0f;
    Float aimErrorYaw = 0.0f;
    Float aimErrorPitch = 0.0f;
    Float burstTimer = -1.0f;
    Float burstPause = 0.0f;
    Float throwTimer = 4.0f;
    Float mineTimer = 10.0f;
    Float skill = 0.5f;
};

struct Player
{
    Bool active = false;
    PlayerControl control = PlayerControl::NONE;
    int peer = -1;
    std::string name;
    UInt8 colorIndex = 0;
    MoveState move;
    Float yaw = 0.0f;
    Float pitch = 0.0f;
    Int16 health = START_HEALTH;
    Int16 armor = 0;
    Bool alive = false;
    Float deadTime = 0.0f;
    Bool wantsRespawn = false;
    WeaponId weapon = WeaponId::RIFLE;
    std::array<Bool, WEAPON_COUNT> hasWeapon{};
    std::array<Int16, WEAPON_COUNT> ammo{};
    Float fireCooldown = 0.0f;
    Int16 grenades = 0;
    Int16 mines = 0;
    Float throwCooldown = 0.0f;
    Int16 frags = 0;
    Int16 deaths = 0;
    UInt8 spawnCount = 0;
    Float lastHeard = 0.0f;
    Float healthDecayTimer = 0.0f;
    Int16 lastDamageFrom = -1;
    Float lastDamageTime = -100.0f;
    BotBrain brain;
    FVec3 renderPosition;
    Float renderYaw = 0.0f;
    Float renderPitch = 0.0f;
    Float walkPhase = 0.0f;
    Float fireFlash = 0.0f;
};

struct Projectile
{
    UInt16 id = 0;
    UInt8 owner = 0;
    ProjectileKind kind = ProjectileKind::NOVA;
    FVec3 position;
    FVec3 velocity;
    Float life = 0.0f;
    Float age = 0.0f;
    UInt8 bounces = 0;
    // Grenades come to rest; mines stick where they land.
    Bool resting = false;
    // Mines arm a moment after they land.
    Bool armed = false;
    // Set when a nearby blast sets this off; it bursts on the next tick.
    Bool detonate = false;
};

struct Pickup
{
    PickupKind kind = PickupKind::HEALTH;
    FVec3 position;
    Bool available = true;
    Float respawnTimer = 0.0f;
};

enum class EventType : UInt8
{
    KILL,
    SHOT,
    EXPLOSION,
    IMPULSE,
    PICKUP,
    HIT,
    SPAWN,
    MATCH_END,
    MATCH_START,
    // a = thrower, b = 0 for a grenade, 1 for a mine
    THROW,
    BOUNCE,
    MINE_ARMED
};

struct GameEvent
{
    UInt16 sequence = 0;
    EventType type = EventType::SHOT;
    UInt8 a = 0;
    UInt8 b = 0;
    UInt8 c = 0;
    FVec3 p0;
    FVec3 p1;
    Float time = 0.0f;
};

enum class MatchPhase : UInt8
{
    PLAYING,
    ENDED
};

struct MatchSettings
{
    Int16 fragLimit = 15;
    Float timeLimitSeconds = 480.0f;
    int botCount = 4;
    Float botSkill = 0.3f;
    ArenaId arena = ArenaId::BASTION;
};

struct MatchState
{
    MatchPhase phase = MatchPhase::PLAYING;
    Float timeRemaining = 480.0f;
    Float endTimer = 0.0f;
    int winner = -1;
};

struct FireRequest
{
    Bool fire = false;
    Bool throwGrenade = false;
    Bool placeMine = false;
    FVec3 origin;
    FVec3 direction;
    // Unspread view direction for grenades and mines.
    FVec3 throwDirection;
};

class World final
{
  public:
    void Initialize(const MatchSettings &settings);

    int AddPlayer(const std::string &player_name, PlayerControl control, int peer);
    void RemovePlayer(int player_id);
    [[nodiscard]]
    int CountPlayers(PlayerControl control) const;
    [[nodiscard]]
    int FindPlayerByPeer(int peer) const;

    void HostTick(Float delta_seconds, const MoveInput &local_input, const FireRequest &local_fire);

    // `pellet_seed` picks the Scattergun's pellet pattern.
    void HostFire(int player_id, const FVec3 &origin, const FVec3 &direction, UInt32 pellet_seed = 0);
    // Throws a grenade or drops a mine if the player has one and the throw cooldown allows it.
    Bool HostThrow(int player_id, Bool mine, const FVec3 &origin, const FVec3 &direction);
    [[nodiscard]]
    Bool CanThrow(const Player &player, Bool mine) const;
    void HostRespawn(int player_id);
    void RestartMatch();

    [[nodiscard]]
    Bool CanFire(const Player &player) const;
    void ConsumeShot(Player &player) const;
    [[nodiscard]]
    FVec3 ApplySpread(const Player &player, const FVec3 &direction, UInt32 seed) const;

    struct TraceResult
    {
        FVec3 end;
        int hitPlayer = -1;
        Bool headshot = false;
        FVec3 normal;
    };
    [[nodiscard]]
    TraceResult Trace(const FVec3 &origin, const FVec3 &direction, Float max_distance,
                      int ignore_player) const;

    void EmitEvent(GameEvent event);
    std::vector<GameEvent> TakeNewEvents();

    Arena arena;
    MatchSettings settings;
    MatchState match;
    std::array<Player, MAX_PLAYERS> players;
    std::vector<Projectile> projectiles;
    std::vector<Pickup> pickups;
    std::deque<GameEvent> eventLog;
    std::vector<GameEvent> newEvents;
    UInt16 nextEventSequence = 1;
    UInt16 nextProjectileId = 1;
    Float time = 0.0f;
    math::Random random;
    MoveEvents localMoveEvents;

  private:
    void StepPlayers(Float delta_seconds, const MoveInput &local_input, const FireRequest &local_fire);
    void StepProjectiles(Float delta_seconds);
    void StepPickups(Float delta_seconds);
    void StepMatch(Float delta_seconds);
    void Explode(const Projectile &projectile, const FVec3 &position);
    UInt16 SpawnProjectile(int owner, ProjectileKind kind, const FVec3 &position, const FVec3 &velocity, Float life);
    void Damage(int victim, int attacker, Int16 amount, DamageSource source, Bool headshot);
    void ApplyImpulse(int player_id, const FVec3 &impulse);
    void Kill(int victim, int attacker, DamageSource source, Bool headshot);
    Bool TryGivePickup(Player &player, PickupKind kind);
    [[nodiscard]]
    FVec3 PickSpawn(int player_id, Float &yaw);
};

FVec3 PlayerColor(UInt8 color_index);
// Direction of one Scattergun pellet; pellet 0 flies straight.
FVec3 PelletDirection(const FVec3 &direction, UInt32 seed, int pellet, Float spread);
Float PickupRespawnTime(PickupKind kind);
const Char *PickupName(PickupKind kind);
} // namespace evo
