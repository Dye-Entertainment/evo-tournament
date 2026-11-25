#pragma once
#include "game/math_util.h"

#include "compiled/warning_off.h"
#include <string>
#include <vector>
#include "compiled/warning_on.h"

namespace evo
{
enum class PickupKind : UInt8
{
    HEALTH,
    MEGA_HEALTH,
    ARMOR,
    WEAPON_NOVA,
    WEAPON_RAIL,
    AMMO_RIFLE,
    AMMO_NOVA,
    AMMO_RAIL,
    COUNT
};

// Selectable levels. The id travels over the network, so keep the order stable.
enum class ArenaId : UInt8
{
    BASTION,
    FOUNDRY,
    FROSTSPIRE,
    COUNT
};

constexpr int ARENA_COUNT = static_cast<int>(ArenaId::COUNT);

[[nodiscard]]
const Char *ArenaName(ArenaId id);

[[nodiscard]]
const Char *ArenaDescription(ArenaId id);

struct ArenaBox
{
    primitive::AxisAlignedBox bounds;
    FVec3 color;
    // 1 draws the panel pattern, 0 draws a plain (usually glowing) surface.
    Float grid = 1.0f;
    // How much the surface glows on its own (0..1). Glowing boxes also light their surroundings.
    Float glow = 0.0f;
    // Decorative trims don't block players, shots or particles.
    Bool solid = true;
};

// A light baked into the level geometry, with shadows from the level boxes.
struct ArenaLight
{
    FVec3 position;
    FVec3 color;
    Float radius = 10.0f;
};

// The look of a level: sky, fog and lighting.
struct ArenaTheme
{
    FVec3 skyZenith{0.07f, 0.06f, 0.20f};
    FVec3 skyHorizon{0.95f, 0.45f, 0.30f};
    FVec3 groundAmbient{0.20f, 0.17f, 0.15f};
    FVec3 skyAmbient{0.30f, 0.32f, 0.40f};
    FVec3 sunDirection{0.35f, 0.85f, 0.25f};
    FVec3 sunColor{0.85f, 0.78f, 0.68f};
    FVec3 fogColor{0.32f, 0.24f, 0.30f};
    Float fogDensity = 0.012f;
    Float stars = 0.0f;
    Float aurora = 0.0f;
    Float clouds = 0.5f;
    Float embers = 0.0f;
};

struct JumpPad
{
    primitive::AxisAlignedBox trigger;
    FVec3 launchVelocity;
};

struct SpawnPoint
{
    FVec3 position;
    Float yaw = 0.0f;
};

struct PickupSpot
{
    PickupKind kind;
    FVec3 position;
};

struct Waypoint
{
    FVec3 position;
    std::vector<int> links;
};

struct RayHit
{
    Float distance = 0.0f;
    FVec3 normal;
};

class Arena final
{
  public:
    void Build(ArenaId arena_id);

    [[nodiscard]]
    Bool RayCast(const FVec3 &origin, const FVec3 &direction, Float max_distance, RayHit &hit) const;

    [[nodiscard]]
    Bool HasLineOfSight(const FVec3 &a, const FVec3 &b) const;

    [[nodiscard]]
    Bool IsBlocked(const primitive::AxisAlignedBox &box) const;

    [[nodiscard]]
    int FindNearestWaypoint(const FVec3 &position, Bool require_sight) const;

    [[nodiscard]]
    std::vector<int> FindPath(int from, int to) const;

    ArenaId id = ArenaId::BASTION;
    std::string name;
    ArenaTheme theme;
    std::vector<ArenaBox> boxes;
    std::vector<ArenaLight> lights;
    std::vector<JumpPad> jumpPads;
    std::vector<SpawnPoint> spawnPoints;
    std::vector<PickupSpot> pickupSpots;
    std::vector<Waypoint> waypoints;

  private:
    void Clear();
    void BuildBastion();
    void BuildFoundry();
    void BuildFrostspire();
    void AddBox(const FVec3 &min, const FVec3 &max, const FVec3 &color, Float grid = 1.0f);
    void AddGlow(const FVec3 &min, const FVec3 &max, const FVec3 &color);
    // Stairs of 0.5 m steps along x (or z) that end at `top_edge` and climb in `direction` (+1 or -1).
    void AddStairsX(Float top_edge, Float direction, Float z0, Float z1, int steps, const FVec3 &color);
    void AddStairsZ(Float top_edge, Float direction, Float x0, Float x1, int steps, const FVec3 &color);
    void AddJumpPad(const FVec3 &center, const FVec3 &direction, const FVec3 &color);
    void AddSpawn(const FVec3 &position);
    int AddWaypoint(const FVec3 &position);
    void Link(int a, int b);
    void LinkOneWay(int from, int to);
};
} // namespace evo
