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

struct ArenaBox
{
    Aabb bounds;
    FVec3 color;
    Float grid = 1.0f;
};

struct JumpPad
{
    Aabb trigger;
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
    void BuildFoundry();

    [[nodiscard]]
    Bool RayCast(const FVec3 &origin, const FVec3 &direction, Float max_distance, RayHit &hit) const;

    [[nodiscard]]
    Bool HasLineOfSight(const FVec3 &a, const FVec3 &b) const;

    [[nodiscard]]
    Bool IsBlocked(const Aabb &box) const;

    [[nodiscard]]
    int FindNearestWaypoint(const FVec3 &position, Bool require_sight) const;

    [[nodiscard]]
    std::vector<int> FindPath(int from, int to) const;

    std::string name;
    std::vector<ArenaBox> boxes;
    std::vector<JumpPad> jumpPads;
    std::vector<SpawnPoint> spawnPoints;
    std::vector<PickupSpot> pickupSpots;
    std::vector<Waypoint> waypoints;

  private:
    void AddBox(const FVec3 &min, const FVec3 &max, const FVec3 &color, Float grid = 1.0f);
    int AddWaypoint(const FVec3 &position);
    void Link(int a, int b);
    void LinkOneWay(int from, int to);
};
} // namespace evo
