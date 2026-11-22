#include "game/arena.h"

#include "compiled/warning_off.h"
#include <limits>
#include <queue>
#include "compiled/warning_on.h"

namespace evo
{
namespace
{
const FVec3 FLOOR_COLOR{0.30f, 0.32f, 0.37f};
const FVec3 WALL_COLOR{0.40f, 0.38f, 0.36f};
const FVec3 ALTAR_COLOR{0.58f, 0.46f, 0.30f};
const FVec3 RING_COLOR{0.34f, 0.38f, 0.46f};
const FVec3 PILLAR_COLOR{0.50f, 0.50f, 0.54f};
const FVec3 TRIM_COLOR{1.00f, 0.55f, 0.15f};
const FVec3 PAD_COLOR{0.20f, 0.95f, 1.00f};
const FVec3 STAIR_COLOR{0.46f, 0.44f, 0.42f};

constexpr Float RING_HEIGHT = 6.0f;
constexpr Float RING_THICKNESS = 0.5f;
constexpr Float WALL_HEIGHT = 14.0f;
constexpr Float HALF_SIZE = 30.0f;
constexpr Float PAD_HORIZONTAL_SPEED = 6.7f;
constexpr Float PAD_VERTICAL_SPEED = 18.0f;
} // namespace

void Arena::AddBox(const FVec3 &min, const FVec3 &max, const FVec3 &color, const Float grid)
{
    boxes.push_back(ArenaBox{primitive::AxisAlignedBox{min, max}, color, grid});
}

int Arena::AddWaypoint(const FVec3 &position)
{
    waypoints.push_back(Waypoint{position, {}});
    return static_cast<int>(waypoints.size()) - 1;
}

void Arena::Link(const int a, const int b)
{
    LinkOneWay(a, b);
    LinkOneWay(b, a);
}

void Arena::LinkOneWay(const int from, const int to)
{
    waypoints[from].links.push_back(to);
}

void Arena::BuildFoundry()
{
    name = "Foundry";
    boxes.clear();
    jumpPads.clear();
    spawnPoints.clear();
    pickupSpots.clear();
    waypoints.clear();

    AddBox({-HALF_SIZE, -1.0f, -HALF_SIZE}, {HALF_SIZE, 0.0f, HALF_SIZE}, FLOOR_COLOR);
    AddBox({-HALF_SIZE - 1, 0, HALF_SIZE}, {HALF_SIZE + 1, WALL_HEIGHT, HALF_SIZE + 1}, WALL_COLOR);
    AddBox({-HALF_SIZE - 1, 0, -HALF_SIZE - 1}, {HALF_SIZE + 1, WALL_HEIGHT, -HALF_SIZE}, WALL_COLOR);
    AddBox({HALF_SIZE, 0, -HALF_SIZE}, {HALF_SIZE + 1, WALL_HEIGHT, HALF_SIZE}, WALL_COLOR);
    AddBox({-HALF_SIZE - 1, 0, -HALF_SIZE}, {-HALF_SIZE, WALL_HEIGHT, HALF_SIZE}, WALL_COLOR);

    AddBox({-HALF_SIZE, 9.8f, HALF_SIZE - 0.15f}, {HALF_SIZE, 10.1f, HALF_SIZE}, TRIM_COLOR, 0.0f);
    AddBox({-HALF_SIZE, 9.8f, -HALF_SIZE}, {HALF_SIZE, 10.1f, -HALF_SIZE + 0.15f}, TRIM_COLOR, 0.0f);
    AddBox({HALF_SIZE - 0.15f, 9.8f, -HALF_SIZE}, {HALF_SIZE, 10.1f, HALF_SIZE}, TRIM_COLOR, 0.0f);
    AddBox({-HALF_SIZE, 9.8f, -HALF_SIZE}, {-HALF_SIZE + 0.15f, 10.1f, HALF_SIZE}, TRIM_COLOR, 0.0f);

    AddBox({-6, 0, -6}, {6, 3, 6}, ALTAR_COLOR);
    AddBox({-5, 3, -1}, {-4, 4.2f, 1}, PILLAR_COLOR);
    AddBox({4, 3, -1}, {5, 4.2f, 1}, PILLAR_COLOR);
    for (int k = 1; k <= 5; ++k)
    {
        const Float height = 0.5f * static_cast<Float>(k);
        const Float reach = 6.0f + static_cast<Float>(6 - k) * 0.8f;
        AddBox({-2.5f, 0, 6}, {2.5f, height, reach}, STAIR_COLOR);
        AddBox({-2.5f, 0, -reach}, {2.5f, height, -6}, STAIR_COLOR);
    }

    const Float ring_bottom = RING_HEIGHT - RING_THICKNESS;
    AddBox({24, ring_bottom, -HALF_SIZE}, {HALF_SIZE, RING_HEIGHT, HALF_SIZE}, RING_COLOR);
    AddBox({-HALF_SIZE, ring_bottom, -HALF_SIZE}, {-24, RING_HEIGHT, HALF_SIZE}, RING_COLOR);
    AddBox({-24, ring_bottom, 22}, {24, RING_HEIGHT, HALF_SIZE}, RING_COLOR);
    AddBox({-24, ring_bottom, -HALF_SIZE}, {24, RING_HEIGHT, -22}, RING_COLOR);

    AddBox({24, RING_HEIGHT, -2}, {24.4f, RING_HEIGHT + 1.1f, 2}, TRIM_COLOR, 0.0f);
    AddBox({-24.4f, RING_HEIGHT, -2}, {-24, RING_HEIGHT + 1.1f, 2}, TRIM_COLOR, 0.0f);

    for (const Float z : {-18.0f, -6.0f, 6.0f, 18.0f})
    {
        AddBox({24, 0, z - 0.5f}, {25, ring_bottom, z + 0.5f}, PILLAR_COLOR);
        AddBox({-25, 0, z - 0.5f}, {-24, ring_bottom, z + 0.5f}, PILLAR_COLOR);
    }
    for (const Float x : {-16.0f, -6.0f, 6.0f, 16.0f})
    {
        AddBox({x - 0.5f, 0, 22}, {x + 0.5f, ring_bottom, 23}, PILLAR_COLOR);
        AddBox({x - 0.5f, 0, -23}, {x + 0.5f, ring_bottom, -22}, PILLAR_COLOR);
    }

    for (int k = 1; k <= 11; ++k)
    {
        const Float height = 0.5f * static_cast<Float>(k);
        AddBox({8.0f + static_cast<Float>(k), 0, 18}, {20, height, 22}, STAIR_COLOR);
        AddBox({-20, 0, -22}, {-8.0f - static_cast<Float>(k), height, -18}, STAIR_COLOR);
    }

    for (const Float sx : {-1.0f, 1.0f})
    {
        for (const Float sz : {-1.0f, 1.0f})
        {
            AddBox({sx * 14 - 1, 0, sz * 9 - 1}, {sx * 14 + 1, 4, sz * 9 + 1}, PILLAR_COLOR);
        }
    }

    const FVec3 pad_extent{1.0f, 0.0f, 1.0f};
    const auto add_pad = [&](const FVec3 &center, const FVec3 &direction)
    {
        AddBox(
            {center.x - 1.0f, 0.0f, center.z - 1.0f}, {center.x + 1.0f, 0.15f, center.z + 1.0f},
            PAD_COLOR, 0.0f
        );
        jumpPads.push_back(
            JumpPad{
                primitive::AxisAlignedBox{
                    center - FVec3{1.0f, 0.0f, 1.0f}, center + FVec3{1.0f, 0.7f, 1.0f}
                },
                direction * PAD_HORIZONTAL_SPEED + FVec3{0.0f, PAD_VERTICAL_SPEED, 0.0f}
            }
        );
    };
    add_pad({20, 0, 0}, {1, 0, 0});
    add_pad({-20, 0, 0}, {-1, 0, 0});
    add_pad({0, 0, 18}, {0, 0, 1});
    add_pad({0, 0, -18}, {0, 0, -1});

    const auto add_spawn = [&](const FVec3 &position)
    {
        spawnPoints.push_back(SpawnPoint{position, std::atan2(-position.x, -position.z)});
    };
    add_spawn({15, 0, 15});
    add_spawn({-15, 0, -15});
    add_spawn({15, 0, -15});
    add_spawn({-15, 0, 15});
    add_spawn({-9, 0, 8});
    add_spawn({9, 0, -8});
    add_spawn({27, RING_HEIGHT, 10});
    add_spawn({27, RING_HEIGHT, -10});
    add_spawn({-27, RING_HEIGHT, 10});
    add_spawn({-27, RING_HEIGHT, -10});
    add_spawn({8, RING_HEIGHT, 26});
    add_spawn({-8, RING_HEIGHT, -26});

    pickupSpots = {
        {PickupKind::WEAPON_RAIL, {0, 3, 0}},
        {PickupKind::WEAPON_NOVA, {27, RING_HEIGHT, 0}},
        {PickupKind::WEAPON_NOVA, {-27, RING_HEIGHT, 0}},
        {PickupKind::MEGA_HEALTH, {0, RING_HEIGHT, 27}},
        {PickupKind::ARMOR, {0, RING_HEIGHT, -27}},
        {PickupKind::HEALTH, {15, 0, 0}},
        {PickupKind::HEALTH, {-15, 0, 0}},
        {PickupKind::HEALTH, {27, 0, -27}},
        {PickupKind::HEALTH, {-27, 0, 27}},
        {PickupKind::AMMO_RIFLE, {9, 0, 9}},
        {PickupKind::AMMO_RIFLE, {-9, 0, -9}},
        {PickupKind::AMMO_NOVA, {27, 0, 20}},
        {PickupKind::AMMO_NOVA, {-27, 0, -20}},
        {PickupKind::AMMO_RAIL, {19.5f, 5.5f, 20}},
        {PickupKind::AMMO_RAIL, {-19.5f, 5.5f, -20}},
    };

    const int altar = AddWaypoint({0, 3, 0});
    const int altar_north = AddWaypoint({0, 0, 11.5f});
    const int altar_south = AddWaypoint({0, 0, -11.5f});
    const int ground_ne = AddWaypoint({17, 0, 13});
    const int ground_nw = AddWaypoint({-17, 0, 13});
    const int ground_se = AddWaypoint({17, 0, -13});
    const int ground_sw = AddWaypoint({-17, 0, -13});
    const int ground_e = AddWaypoint({16.5f, 0, 0});
    const int ground_w = AddWaypoint({-16.5f, 0, 0});
    const int ground_n = AddWaypoint({0, 0, 15.5f});
    const int ground_s = AddWaypoint({0, 0, -15.5f});
    const int pad_e = AddWaypoint({20, 0.15f, 0});
    const int pad_w = AddWaypoint({-20, 0.15f, 0});
    const int pad_n = AddWaypoint({0, 0.15f, 18});
    const int pad_s = AddWaypoint({0, 0.15f, -18});
    const int balcony_e = AddWaypoint({27, RING_HEIGHT, 0});
    const int balcony_ne = AddWaypoint({27, RING_HEIGHT, 26});
    const int balcony_se = AddWaypoint({27, RING_HEIGHT, -26});
    const int balcony_w = AddWaypoint({-27, RING_HEIGHT, 0});
    const int balcony_nw = AddWaypoint({-27, RING_HEIGHT, 26});
    const int balcony_sw = AddWaypoint({-27, RING_HEIGHT, -26});
    const int bridge_n = AddWaypoint({0, RING_HEIGHT, 26});
    const int bridge_s = AddWaypoint({0, RING_HEIGHT, -26});
    const int bridge_ne = AddWaypoint({19.5f, RING_HEIGHT, 26});
    const int bridge_sw = AddWaypoint({-19.5f, RING_HEIGHT, -26});
    const int stair_ne_bottom = AddWaypoint({7.5f, 0, 20});
    const int stair_ne_top = AddWaypoint({19.5f, 5.5f, 20});
    const int stair_sw_bottom = AddWaypoint({-7.5f, 0, -20});
    const int stair_sw_top = AddWaypoint({-19.5f, 5.5f, -20});
    const int under_ne = AddWaypoint({27, 0, 12});
    const int under_se = AddWaypoint({27, 0, -12});
    const int under_nw = AddWaypoint({-27, 0, 12});
    const int under_sw = AddWaypoint({-27, 0, -12});

    Link(altar, altar_north);
    Link(altar, altar_south);
    Link(altar_north, ground_n);
    Link(altar_north, ground_ne);
    Link(altar_north, ground_nw);
    Link(altar_south, ground_s);
    Link(altar_south, ground_se);
    Link(altar_south, ground_sw);
    Link(ground_ne, ground_e);
    Link(ground_se, ground_e);
    Link(ground_nw, ground_w);
    Link(ground_sw, ground_w);
    Link(ground_ne, ground_n);
    Link(ground_nw, ground_n);
    Link(ground_se, ground_s);
    Link(ground_sw, ground_s);
    Link(ground_e, pad_e);
    Link(ground_w, pad_w);
    Link(ground_n, pad_n);
    Link(ground_s, pad_s);
    LinkOneWay(pad_e, balcony_e);
    LinkOneWay(pad_w, balcony_w);
    LinkOneWay(pad_n, bridge_n);
    LinkOneWay(pad_s, bridge_s);
    Link(balcony_e, balcony_ne);
    Link(balcony_e, balcony_se);
    Link(balcony_w, balcony_nw);
    Link(balcony_w, balcony_sw);
    Link(balcony_ne, bridge_ne);
    Link(bridge_ne, bridge_n);
    Link(bridge_n, balcony_nw);
    Link(balcony_se, bridge_s);
    Link(bridge_s, bridge_sw);
    Link(bridge_sw, balcony_sw);
    Link(ground_n, stair_ne_bottom);
    Link(stair_ne_bottom, stair_ne_top);
    Link(stair_ne_top, bridge_ne);
    Link(ground_s, stair_sw_bottom);
    Link(stair_sw_bottom, stair_sw_top);
    Link(stair_sw_top, bridge_sw);
    LinkOneWay(balcony_e, ground_e);
    LinkOneWay(balcony_w, ground_w);
    LinkOneWay(bridge_n, ground_n);
    LinkOneWay(bridge_s, ground_s);
    LinkOneWay(altar, ground_e);
    LinkOneWay(altar, ground_w);
    Link(under_ne, ground_e);
    Link(under_ne, ground_ne);
    Link(under_se, ground_e);
    Link(under_se, ground_se);
    Link(under_nw, ground_w);
    Link(under_nw, ground_nw);
    Link(under_sw, ground_w);
    Link(under_sw, ground_sw);
}

Bool Arena::RayCast(
    const FVec3 &origin, const FVec3 &direction, const Float max_distance, RayHit &hit
) const
{
    Bool found = false;
    Float best = max_distance;
    for (const ArenaBox &box : boxes)
    {
        Float distance = 0.0f;
        FVec3 normal;
        if (primitive::IntersectRay(box.bounds, origin, direction, best, distance, normal) &&
            distance <= best)
        {
            if (primitive::Contains(box.bounds, origin))
            {
                continue;
            }
            best = distance;
            hit.distance = distance;
            hit.normal = normal;
            found = true;
        }
    }
    return found;
}

Bool Arena::HasLineOfSight(const FVec3 &a, const FVec3 &b) const
{
    const FVec3 delta = b - a;
    const Float distance = math::GetLength(delta);
    if (distance < 1e-4f)
    {
        return true;
    }
    RayHit hit;
    return !RayCast(a, delta * (1.0f / distance), distance - 0.02f, hit);
}

Bool Arena::IsBlocked(const primitive::AxisAlignedBox &box) const
{
    for (const ArenaBox &arena_box : boxes)
    {
        if (primitive::Overlaps(box, arena_box.bounds))
        {
            return true;
        }
    }
    return false;
}

int Arena::FindNearestWaypoint(const FVec3 &position, const Bool require_sight) const
{
    int best_index = -1;
    Float best_score = std::numeric_limits<Float>::max();
    const FVec3 eye = position + FVec3{0.0f, 1.0f, 0.0f};
    for (int i = 0; i < static_cast<int>(waypoints.size()); ++i)
    {
        const FVec3 delta = waypoints[i].position - position;
        const Float score = LengthXZ(delta) + std::fabs(delta.y) * 3.0f;
        if (score >= best_score)
        {
            continue;
        }
        if (require_sight && !HasLineOfSight(eye, waypoints[i].position + FVec3{0.0f, 1.0f, 0.0f}))
        {
            continue;
        }
        best_score = score;
        best_index = i;
    }
    return best_index;
}

std::vector<int> Arena::FindPath(const int from, const int to) const
{
    const int count = static_cast<int>(waypoints.size());
    if (from < 0 || to < 0 || from >= count || to >= count)
    {
        return {};
    }
    std::vector<Float> cost(count, std::numeric_limits<Float>::max());
    std::vector<int> previous(count, -1);
    using Entry = std::pair<Float, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
    cost[from] = 0.0f;
    open.push({0.0f, from});
    while (!open.empty())
    {
        const auto [estimate, current] = open.top();
        open.pop();
        if (current == to)
        {
            break;
        }
        for (const int next : waypoints[current].links)
        {
            const Float step =
                math::GetLength(waypoints[next].position - waypoints[current].position);
            const Float candidate = cost[current] + step;
            if (candidate < cost[next])
            {
                cost[next] = candidate;
                previous[next] = current;
                const Float heuristic =
                    math::GetLength(waypoints[to].position - waypoints[next].position);
                open.push({candidate + heuristic, next});
            }
        }
    }
    if (from != to && previous[to] == -1)
    {
        return {};
    }
    std::vector<int> path;
    for (int node = to; node != -1; node = previous[node])
    {
        path.push_back(node);
        if (node == from)
        {
            break;
        }
    }
    std::reverse(path.begin(), path.end());
    return path;
}
} // namespace evo
