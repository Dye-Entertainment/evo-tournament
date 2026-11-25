#include "game/arena.h"

#include "compiled/warning_off.h"
#include <algorithm>
#include <limits>
#include <queue>
#include "compiled/warning_on.h"

namespace evo
{
namespace
{
// Levels are laid out compactly and then stretched this much horizontally (heights stay).
constexpr Float ARENA_SCALE = 1.35f;
const FVec3 FLOOR_COLOR{0.30f, 0.32f, 0.37f};
const FVec3 WALL_COLOR{0.40f, 0.38f, 0.36f};
const FVec3 KEEP_COLOR{0.58f, 0.46f, 0.30f};
const FVec3 DECK_COLOR{0.34f, 0.38f, 0.46f};
const FVec3 WING_COLOR{0.36f, 0.42f, 0.38f};
const FVec3 PILLAR_COLOR{0.50f, 0.50f, 0.54f};
const FVec3 COVER_COLOR{0.44f, 0.40f, 0.46f};
const FVec3 CRATE_COLOR{0.55f, 0.42f, 0.26f};
const FVec3 TRIM_COLOR{1.00f, 0.55f, 0.15f};
const FVec3 PAD_COLOR{0.20f, 0.95f, 1.00f};
const FVec3 STAIR_COLOR{0.46f, 0.44f, 0.42f};

constexpr Float HALF_SIZE = 46.0f;
constexpr Float WALL_HEIGHT = 16.0f;
constexpr Float DECK_HEIGHT = 6.0f;
constexpr Float DECK_THICKNESS = 0.5f;
constexpr Float DECK_EDGE = 38.0f;
constexpr Float KEEP_HALF = 8.0f;
constexpr Float KEEP_HEIGHT = 3.0f;
constexpr Float WING_INNER = 36.0f;
constexpr Float WING_HALF_LENGTH = 18.0f;
constexpr Float WING_HEIGHT = 3.0f;
constexpr Float BRIDGE_HALF_WIDTH = 2.0f;
constexpr Float PAD_HORIZONTAL_SPEED = 6.7f;
constexpr Float PAD_VERTICAL_SPEED = 18.0f;
} // namespace

const Char *ArenaName(const ArenaId id)
{
    switch (id)
    {
    case ArenaId::FOUNDRY:
        return "Foundry";
    case ArenaId::FROSTSPIRE:
        return "Frostspire";
    default:
        return "Bastion";
    }
}

const Char *ArenaDescription(const ArenaId id)
{
    switch (id)
    {
    case ArenaId::FOUNDRY:
        return "A molten forge hall. Hold the crucible, fight for the catwalk ring.";
    case ArenaId::FROSTSPIRE:
        return "Twin ice towers under the aurora, joined by a narrow sky bridge.";
    default:
        return "A sunset fortress with a central keep, high decks and a long bridge.";
    }
}

void Arena::Clear()
{
    boxes.clear();
    lights.clear();
    jumpPads.clear();
    spawnPoints.clear();
    pickupSpots.clear();
    waypoints.clear();
    hazards.clear();
    scale = 1.0f;
    theme = ArenaTheme{};
}

void Arena::Build(const ArenaId arena_id)
{
    Clear();
    id = arena_id;
    name = ArenaName(arena_id);
    switch (arena_id)
    {
    case ArenaId::FOUNDRY:
        BuildFoundry();
        break;
    case ArenaId::FROSTSPIRE:
        BuildFrostspire();
        break;
    default:
        id = ArenaId::BASTION;
        BuildBastion();
        break;
    }
    ScaleHorizontally(ARENA_SCALE);
    bounds = boxes.empty() ? primitive::AxisAlignedBox{} : boxes[0].bounds;
    AddHazards();
}

void Arena::ScaleHorizontally(const Float factor)
{
    scale = factor;
    const auto stretch = [factor](FVec3 &p)
    {
        p.x *= factor;
        p.z *= factor;
    };
    for (ArenaBox &box : boxes)
    {
        stretch(box.bounds.min);
        stretch(box.bounds.max);
    }
    for (ArenaLight &light : lights)
    {
        stretch(light.position);
        light.radius *= 1.0f + (factor - 1.0f) * 0.5f;
    }
    for (JumpPad &pad : jumpPads)
    {
        // Same flight time, so the horizontal speed grows with the distance to cover.
        stretch(pad.trigger.min);
        stretch(pad.trigger.max);
        stretch(pad.launchVelocity);
    }
    for (SpawnPoint &spawn : spawnPoints)
    {
        stretch(spawn.position);
    }
    for (PickupSpot &spot : pickupSpots)
    {
        stretch(spot.position);
    }
    for (Waypoint &waypoint : waypoints)
    {
        stretch(waypoint.position);
    }
    theme.fogDensity /= factor;
}

void Arena::CarveFloor(const Float x0, const Float z0, const Float x1, const Float z1)
{
    std::vector<ArenaBox> result;
    result.reserve(boxes.size() + 4);
    for (const ArenaBox &box : boxes)
    {
        const primitive::AxisAlignedBox &b = box.bounds;
        const Bool is_floor = box.solid && std::fabs(b.max.y) < 0.01f && b.min.y < -0.5f;
        const Bool overlaps = b.min.x < x1 && b.max.x > x0 && b.min.z < z1 && b.max.z > z0;
        if (!is_floor || !overlaps)
        {
            result.push_back(box);
            continue;
        }
        // Keep the four pieces around the hole.
        const auto piece = [&](const Float ax, const Float az, const Float bx, const Float bz)
        {
            if (bx - ax > 0.01f && bz - az > 0.01f)
            {
                ArenaBox part = box;
                part.bounds = primitive::AxisAlignedBox{{ax, b.min.y, az}, {bx, b.max.y, bz}};
                result.push_back(part);
            }
        };
        piece(b.min.x, b.min.z, b.max.x, std::max(b.min.z, z0));
        piece(b.min.x, std::min(b.max.z, z1), b.max.x, b.max.z);
        piece(b.min.x, std::max(b.min.z, z0), std::max(b.min.x, x0), std::min(b.max.z, z1));
        piece(std::min(b.max.x, x1), std::max(b.min.z, z0), b.max.x, std::min(b.max.z, z1));
    }
    boxes.swap(result);
}

void Arena::AddLavaPit(const Float x, const Float z, const Float half_x, const Float half_z)
{
    const Float cx = x * scale;
    const Float cz = z * scale;
    CarveFloor(cx - half_x, cz - half_z, cx + half_x, cz + half_z);
    // Pit floor, then a stone rim so the edge reads from a distance.
    AddBox({cx - half_x, -1.6f, cz - half_z}, {cx + half_x, -LAVA_DEPTH, cz + half_z}, {0.12f, 0.07f, 0.05f});
    const FVec3 rim{0.16f, 0.13f, 0.12f};
    AddGlow({cx - half_x - 0.3f, 0.0f, cz - half_z - 0.3f}, {cx + half_x + 0.3f, 0.06f, cz - half_z}, rim * 0.6f);
    AddGlow({cx - half_x - 0.3f, 0.0f, cz + half_z}, {cx + half_x + 0.3f, 0.06f, cz + half_z + 0.3f}, rim * 0.6f);
    AddGlow({cx - half_x - 0.3f, 0.0f, cz - half_z}, {cx - half_x, 0.06f, cz + half_z}, rim * 0.6f);
    AddGlow({cx + half_x, 0.0f, cz - half_z}, {cx + half_x + 0.3f, 0.06f, cz + half_z}, rim * 0.6f);
    lights.push_back(ArenaLight{{cx, 0.8f, cz}, {1.0f, 0.38f, 0.08f}, 6.0f + std::max(half_x, half_z) * 1.5f});
    Hazard hazard;
    hazard.kind = HazardKind::LAVA;
    hazard.area = primitive::AxisAlignedBox{{cx - half_x, -LAVA_DEPTH - 0.5f, cz - half_z},
                                            {cx + half_x, -0.25f, cz + half_z}};
    hazard.position = {cx, -LAVA_DEPTH, cz};
    hazards.push_back(hazard);
}

void Arena::AddSpikes(const Float x, const Float z, const Float half, const Float phase)
{
    const Float cx = x * scale;
    const Float cz = z * scale;
    Hazard hazard;
    hazard.kind = HazardKind::SPIKES;
    hazard.area = primitive::AxisAlignedBox{{cx - half, -0.1f, cz - half}, {cx + half, 1.0f, cz + half}};
    hazard.position = {cx, 0.0f, cz};
    hazard.phase = phase;
    hazards.push_back(hazard);
}

void Arena::AddMaw(const Float x, const Float z, const Float yaw)
{
    const Float cx = x * scale;
    const Float cz = z * scale;
    // The stone well blocks movement; the creature sits on top of it.
    AddBox({cx - MAW_WELL_HALF, 0.0f, cz - MAW_WELL_HALF}, {cx + MAW_WELL_HALF, MAW_WELL_HEIGHT, cz + MAW_WELL_HALF},
           {0.36f, 0.32f, 0.3f});
    lights.push_back(ArenaLight{{cx, MAW_WELL_HEIGHT + 2.5f, cz}, {0.7f, 0.15f, 0.25f}, 7.0f});
    Hazard hazard;
    hazard.kind = HazardKind::MAW;
    hazard.area = primitive::AxisAlignedBox{{cx - MAW_WELL_HALF, 0.0f, cz - MAW_WELL_HALF},
                                            {cx + MAW_WELL_HALF, MAW_WELL_HEIGHT, cz + MAW_WELL_HALF}};
    hazard.position = {cx, 0.0f, cz};
    hazard.yaw = yaw;
    hazards.push_back(hazard);
}

void Arena::AddHazards()
{
    // Positions come from a search for open floor away from the bot routes, pickups and
    // spawns (see the self-test, which checks this); each level is point-symmetric.
    constexpr Float PI = 3.14159265f;
    switch (id)
    {
    case ArenaId::FOUNDRY:
        AddLavaPit(26.5f, 15.0f, 1.8f, 3.2f);
        AddLavaPit(-26.5f, -15.0f, 1.8f, 3.2f);
        AddSpikes(24.0f, 26.0f, 1.6f, 0.0f);
        AddSpikes(-24.0f, -26.0f, 1.6f, 1.6f);
        AddMaw(5.0f, -29.5f, 0.0f);
        AddMaw(-5.0f, 29.5f, PI);
        break;
    case ArenaId::FROSTSPIRE:
        AddLavaPit(26.0f, -23.0f, 3.5f, 3.5f);
        AddLavaPit(-26.0f, 23.0f, 3.5f, 3.5f);
        AddSpikes(19.0f, -31.0f, 2.0f, 0.0f);
        AddSpikes(-19.0f, 31.0f, 2.0f, 1.6f);
        AddSpikes(9.0f, -28.0f, 1.5f, 0.8f);
        AddSpikes(-9.0f, 28.0f, 1.5f, 2.4f);
        AddMaw(28.0f, 35.0f, PI);
        AddMaw(-28.0f, -35.0f, 0.0f);
        break;
    default:
        AddLavaPit(39.0f, -30.0f, 3.0f, 4.0f);
        AddLavaPit(-39.0f, 30.0f, 3.0f, 4.0f);
        AddSpikes(8.0f, 39.5f, 2.0f, 0.0f);
        AddSpikes(-8.0f, -39.5f, 2.0f, 1.6f);
        AddSpikes(7.0f, 20.0f, 1.5f, 0.8f);
        AddSpikes(-7.0f, -20.0f, 1.5f, 2.4f);
        AddMaw(31.0f, 32.5f, PI * 1.25f);
        AddMaw(-31.0f, -32.5f, PI * 0.25f);
        break;
    }
}

Float SpikeExtension(const Hazard &hazard, const Float time)
{
    const Float t = std::fmod(std::max(0.0f, time + hazard.phase), SPIKE_PERIOD);
    if (t < 2.0f)
    {
        return 0.0f;
    }
    if (t < 2.6f)
    {
        // Warning: the tips peek out and rattle.
        return 0.07f + 0.03f * std::sin(t * 60.0f);
    }
    if (t < 2.7f)
    {
        return 0.1f + 0.9f * (t - 2.6f) / 0.1f;
    }
    if (t < 3.0f)
    {
        return 1.0f;
    }
    return std::max(0.0f, 1.0f - (t - 3.0f) / 0.2f);
}

FVec3 MawBiteCenter(const Hazard &hazard)
{
    return hazard.position + FVec3{std::sin(hazard.yaw), 0.0f, std::cos(hazard.yaw)} * (MAW_WELL_HALF + MAW_REACH * 0.55f);
}

void Arena::AddBox(const FVec3 &min, const FVec3 &max, const FVec3 &color, const Float grid)
{
    boxes.push_back(ArenaBox{primitive::AxisAlignedBox{min, max}, color, grid, 0.0f});
}

void Arena::AddGlow(const FVec3 &min, const FVec3 &max, const FVec3 &color)
{
    boxes.push_back(ArenaBox{primitive::AxisAlignedBox{min, max}, color, 0.0f, 1.0f, false});
}

void Arena::AddStairsX(
    const Float top_edge, const Float direction, const Float z0, const Float z1, const int steps,
    const FVec3 &color
)
{
    for (int k = 1; k <= steps; ++k)
    {
        const Float height = 0.5f * static_cast<Float>(k);
        const Float far_end = top_edge - direction * static_cast<Float>(steps + 1 - k);
        AddBox({std::min(top_edge, far_end), 0, z0}, {std::max(top_edge, far_end), height, z1}, color);
    }
}

void Arena::AddStairsZ(
    const Float top_edge, const Float direction, const Float x0, const Float x1, const int steps,
    const FVec3 &color
)
{
    for (int k = 1; k <= steps; ++k)
    {
        const Float height = 0.5f * static_cast<Float>(k);
        const Float far_end = top_edge - direction * static_cast<Float>(steps + 1 - k);
        AddBox({x0, 0, std::min(top_edge, far_end)}, {x1, height, std::max(top_edge, far_end)}, color);
    }
}

void Arena::AddJumpPad(const FVec3 &center, const FVec3 &direction, const FVec3 &color)
{
    boxes.push_back(ArenaBox{
        primitive::AxisAlignedBox{
            {center.x - 1.0f, 0.0f, center.z - 1.0f}, {center.x + 1.0f, 0.15f, center.z + 1.0f}
        },
        color, 0.0f, 1.0f
    });
    jumpPads.push_back(
        JumpPad{
            primitive::AxisAlignedBox{center - FVec3{1.0f, 0.0f, 1.0f}, center + FVec3{1.0f, 0.7f, 1.0f}},
            direction * PAD_HORIZONTAL_SPEED + FVec3{0.0f, PAD_VERTICAL_SPEED, 0.0f}
        }
    );
    lights.push_back(ArenaLight{center + FVec3{0.0f, 1.2f, 0.0f}, color * 0.9f, 7.0f});
}

void Arena::AddSpawn(const FVec3 &position)
{
    spawnPoints.push_back(SpawnPoint{position, std::atan2(-position.x, -position.z)});
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

void Arena::BuildBastion()
{
    theme.skyZenith = {0.07f, 0.06f, 0.20f};
    theme.skyHorizon = {0.95f, 0.45f, 0.30f};
    theme.skyAmbient = {0.30f, 0.30f, 0.42f};
    theme.groundAmbient = {0.22f, 0.16f, 0.14f};
    theme.sunDirection = math::GetNormalized(FVec3{0.50f, 0.62f, 0.35f});
    theme.sunColor = {0.95f, 0.72f, 0.52f};
    theme.fogColor = {0.36f, 0.24f, 0.28f};
    theme.fogDensity = 0.011f;
    theme.stars = 0.35f;
    theme.clouds = 0.8f;

    constexpr Float H = HALF_SIZE;
    constexpr Float DECK_BOTTOM = DECK_HEIGHT - DECK_THICKNESS;

    // Floor, outer walls and a glowing trim band.
    AddBox({-H, -1.0f, -H}, {H, 0.0f, H}, FLOOR_COLOR);
    AddBox({-H - 1, 0, H}, {H + 1, WALL_HEIGHT, H + 1}, WALL_COLOR);
    AddBox({-H - 1, 0, -H - 1}, {H + 1, WALL_HEIGHT, -H}, WALL_COLOR);
    AddBox({H, 0, -H}, {H + 1, WALL_HEIGHT, H}, WALL_COLOR);
    AddBox({-H - 1, 0, -H}, {-H, WALL_HEIGHT, H}, WALL_COLOR);
    AddGlow({-H, 11.8f, H - 0.15f}, {H, 12.1f, H}, TRIM_COLOR);
    AddGlow({-H, 11.8f, -H}, {H, 12.1f, -H + 0.15f}, TRIM_COLOR);
    AddGlow({H - 0.15f, 11.8f, -H}, {H, 12.1f, H}, TRIM_COLOR);
    AddGlow({-H, 11.8f, -H}, {-H + 0.15f, 12.1f, H}, TRIM_COLOR);
    for (const Float t : {-30.0f, 0.0f, 30.0f})
    {
        lights.push_back(ArenaLight{{t, 10.5f, H - 2.0f}, TRIM_COLOR * 0.8f, 14.0f});
        lights.push_back(ArenaLight{{t, 10.5f, -H + 2.0f}, TRIM_COLOR * 0.8f, 14.0f});
        lights.push_back(ArenaLight{{H - 2.0f, 10.5f, t}, TRIM_COLOR * 0.8f, 14.0f});
        lights.push_back(ArenaLight{{-H + 2.0f, 10.5f, t}, TRIM_COLOR * 0.8f, 14.0f});
    }
    lights.push_back(ArenaLight{{0.0f, 4.6f, 0.0f}, {1.0f, 0.6f, 0.3f}, 9.0f});

    // Central keep with stairs on its east and west faces and two low cover walls on top.
    AddBox({-KEEP_HALF, 0, -KEEP_HALF}, {KEEP_HALF, KEEP_HEIGHT, KEEP_HALF}, KEEP_COLOR);
    for (int k = 1; k <= 5; ++k)
    {
        const Float height = 0.5f * static_cast<Float>(k);
        const Float reach = KEEP_HALF + static_cast<Float>(7 - k);
        AddBox({KEEP_HALF, 0, -2.5f}, {reach, height, 2.5f}, STAIR_COLOR);
        AddBox({-reach, 0, -2.5f}, {-KEEP_HALF, height, 2.5f}, STAIR_COLOR);
    }
    AddBox({-3, KEEP_HEIGHT, 4.5f}, {3, KEEP_HEIGHT + 1.2f, 5}, COVER_COLOR);
    AddBox({-3, KEEP_HEIGHT, -5}, {3, KEEP_HEIGHT + 1.2f, -4.5f}, COVER_COLOR);

    // Upper decks along the north and south walls, joined by a bridge that passes over the keep.
    AddBox({-H, DECK_BOTTOM, DECK_EDGE}, {H, DECK_HEIGHT, H}, DECK_COLOR);
    AddBox({-H, DECK_BOTTOM, -H}, {H, DECK_HEIGHT, -DECK_EDGE}, DECK_COLOR);
    AddBox(
        {-BRIDGE_HALF_WIDTH, DECK_BOTTOM, -DECK_EDGE}, {BRIDGE_HALF_WIDTH, DECK_HEIGHT, DECK_EDGE},
        DECK_COLOR
    );
    AddGlow({-BRIDGE_HALF_WIDTH, DECK_HEIGHT, -0.4f}, {-BRIDGE_HALF_WIDTH + 0.2f, DECK_HEIGHT + 0.5f, 0.4f}, TRIM_COLOR);
    AddGlow({BRIDGE_HALF_WIDTH - 0.2f, DECK_HEIGHT, -0.4f}, {BRIDGE_HALF_WIDTH, DECK_HEIGHT + 0.5f, 0.4f}, TRIM_COLOR);
    lights.push_back(ArenaLight{{0.0f, DECK_HEIGHT + 1.2f, 0.0f}, TRIM_COLOR * 0.7f, 6.0f});
    for (const Float z : {-20.0f, 20.0f})
    {
        AddBox({-BRIDGE_HALF_WIDTH, 0, z - 0.5f}, {-BRIDGE_HALF_WIDTH + 0.5f, DECK_BOTTOM, z + 0.5f}, PILLAR_COLOR);
        AddBox({BRIDGE_HALF_WIDTH - 0.5f, 0, z - 0.5f}, {BRIDGE_HALF_WIDTH, DECK_BOTTOM, z + 0.5f}, PILLAR_COLOR);
    }
    for (const Float x : {-32.0f, -16.0f, 16.0f, 32.0f})
    {
        AddBox({x - 1, 0, DECK_EDGE}, {x + 1, DECK_BOTTOM, DECK_EDGE + 2}, PILLAR_COLOR);
        AddBox({x - 1, 0, -DECK_EDGE - 2}, {x + 1, DECK_BOTTOM, -DECK_EDGE}, PILLAR_COLOR);
    }

    // Long stairways up to the decks in the north-east and south-west corners.
    for (int k = 1; k <= 12; ++k)
    {
        const Float height = 0.5f * static_cast<Float>(k);
        const Float near_end = 25.0f + static_cast<Float>(k);
        AddBox({38, 0, near_end}, {44, height, DECK_EDGE}, STAIR_COLOR);
        AddBox({-44, 0, -DECK_EDGE}, {-38, height, -near_end}, STAIR_COLOR);
    }

    // Raised wings on the east and west walls, with stairs and a short parapet facing the middle.
    AddBox({WING_INNER, 0, -WING_HALF_LENGTH}, {H, WING_HEIGHT, WING_HALF_LENGTH}, WING_COLOR);
    AddBox({-H, 0, -WING_HALF_LENGTH}, {-WING_INNER, WING_HEIGHT, WING_HALF_LENGTH}, WING_COLOR);
    for (int k = 1; k <= 5; ++k)
    {
        const Float height = 0.5f * static_cast<Float>(k);
        const Float reach = WING_INNER - static_cast<Float>(6 - k);
        AddBox({reach, 0, -2.5f}, {WING_INNER, height, 2.5f}, STAIR_COLOR);
        AddBox({-WING_INNER, 0, -2.5f}, {-reach, height, 2.5f}, STAIR_COLOR);
    }
    for (const Float sz : {-1.0f, 1.0f})
    {
        const Float z0 = std::min(sz * 6.0f, sz * 14.0f);
        const Float z1 = std::max(sz * 6.0f, sz * 14.0f);
        AddBox({WING_INNER, WING_HEIGHT, z0}, {WING_INNER + 0.5f, WING_HEIGHT + 1.1f, z1}, COVER_COLOR);
        AddBox({-WING_INNER - 0.5f, WING_HEIGHT, z0}, {-WING_INNER, WING_HEIGHT + 1.1f, z1}, COVER_COLOR);
    }

    // Ground cover: an L-shaped wall and a pillar in every quadrant, plus crate stacks.
    for (const Float sx : {-1.0f, 1.0f})
    {
        for (const Float sz : {-1.0f, 1.0f})
        {
            const auto add_mirrored = [&](const Float x0, const Float z0, const Float x1, const Float z1,
                                          const Float height, const FVec3 &color)
            {
                AddBox(
                    {std::min(sx * x0, sx * x1), 0, std::min(sz * z0, sz * z1)},
                    {std::max(sx * x0, sx * x1), height, std::max(sz * z0, sz * z1)}, color
                );
            };
            add_mirrored(16, 18, 24, 19, 3.5f, COVER_COLOR);
            add_mirrored(23, 8, 24, 19, 3.5f, COVER_COLOR);
            add_mirrored(11, 23, 13, 25, 6.0f, PILLAR_COLOR);
        }
    }
    for (const Float s : {-1.0f, 1.0f})
    {
        AddBox({s * 30 - 1, 0, -s * 30 - 1}, {s * 30 + 1, 1.0f, -s * 30 + 1}, CRATE_COLOR);
        AddBox({s * 32.2f - 0.8f, 0, -s * 30 - 0.8f}, {s * 32.2f + 0.8f, 1.6f, -s * 30 + 0.8f}, CRATE_COLOR);
    }

    // Jump pads that throw you onto the decks.
    AddJumpPad({24, 0, 34}, {0, 0, 1}, PAD_COLOR);
    AddJumpPad({-24, 0, 34}, {0, 0, 1}, PAD_COLOR);
    AddJumpPad({24, 0, -34}, {0, 0, -1}, PAD_COLOR);
    AddJumpPad({-24, 0, -34}, {0, 0, -1}, PAD_COLOR);

    AddSpawn({30, 0, 12});
    AddSpawn({-30, 0, -12});
    AddSpawn({-12, 0, 32});
    AddSpawn({12, 0, -32});
    AddSpawn({41, WING_HEIGHT, -10});
    AddSpawn({-41, WING_HEIGHT, 10});
    AddSpawn({-24, DECK_HEIGHT, 42});
    AddSpawn({24, DECK_HEIGHT, -42});
    AddSpawn({14, 0, -6});
    AddSpawn({-14, 0, 6});
    AddSpawn({12, DECK_HEIGHT, 42});
    AddSpawn({-12, DECK_HEIGHT, -42});

    pickupSpots = {
        {PickupKind::WEAPON_RAIL, {0, KEEP_HEIGHT, 0}},
        {PickupKind::MEGA_HEALTH, {0, DECK_HEIGHT, 0}},
        {PickupKind::WEAPON_NOVA, {41, WING_HEIGHT, 0}},
        {PickupKind::WEAPON_NOVA, {-41, WING_HEIGHT, 0}},
        {PickupKind::ARMOR, {0, DECK_HEIGHT, 42}},
        {PickupKind::ARMOR, {0, DECK_HEIGHT, -42}},
        {PickupKind::HEALTH, {41, WING_HEIGHT, 14}},
        {PickupKind::HEALTH, {-41, WING_HEIGHT, -14}},
        {PickupKind::HEALTH, {18, 0, 0}},
        {PickupKind::HEALTH, {-18, 0, 0}},
        {PickupKind::HEALTH, {0, 0, 42}},
        {PickupKind::HEALTH, {0, 0, -42}},
        {PickupKind::AMMO_RIFLE, {20, 0, 13}},
        {PickupKind::AMMO_RIFLE, {-20, 0, -13}},
        {PickupKind::AMMO_NOVA, {32, 0, 24}},
        {PickupKind::AMMO_NOVA, {-32, 0, -24}},
        {PickupKind::AMMO_RAIL, {40, DECK_HEIGHT, 42}},
        {PickupKind::AMMO_RAIL, {-40, DECK_HEIGHT, -42}},
        {PickupKind::WEAPON_SCATTER, {30, 0, 22}},
        {PickupKind::WEAPON_SCATTER, {-30, 0, -22}},
        {PickupKind::WEAPON_FRAG, {-30, 0, 22}},
        {PickupKind::WEAPON_FRAG, {30, 0, -22}},
        {PickupKind::WEAPON_RIPPER, {24, DECK_HEIGHT, 42}},
        {PickupKind::WEAPON_RIPPER, {-24, DECK_HEIGHT, -42}},
        {PickupKind::GRENADES, {12, 0, 13}},
        {PickupKind::GRENADES, {-12, 0, -13}},
        {PickupKind::MINES, {-12, 0, 13}},
        {PickupKind::MINES, {12, 0, -13}},
        {PickupKind::AMMO_SCATTER, {0, 0, 30}},
        {PickupKind::AMMO_SCATTER, {0, 0, -30}},
        {PickupKind::AMMO_FRAG, {29, 0, 0}},
        {PickupKind::AMMO_FRAG, {-29, 0, 0}},
        {PickupKind::AMMO_RIPPER, {41, DECK_HEIGHT, 41}},
        {PickupKind::AMMO_RIPPER, {-41, DECK_HEIGHT, -41}},
    };

    // Bot navigation graph.
    const int keep = AddWaypoint({0, KEEP_HEIGHT, 0});
    const int keep_e = AddWaypoint({6, KEEP_HEIGHT, 0});
    const int keep_w = AddWaypoint({-6, KEEP_HEIGHT, 0});
    const int stair_e = AddWaypoint({18, 0, 0});
    const int stair_w = AddWaypoint({-18, 0, 0});
    const int center_n = AddWaypoint({0, 0, 13});
    const int center_s = AddWaypoint({0, 0, -13});
    const int quad_ne = AddWaypoint({12, 0, 13});
    const int quad_nw = AddWaypoint({-12, 0, 13});
    const int quad_se = AddWaypoint({12, 0, -13});
    const int quad_sw = AddWaypoint({-12, 0, -13});
    const int nook_ne = AddWaypoint({20, 0, 13});
    const int nook_nw = AddWaypoint({-20, 0, 13});
    const int nook_se = AddWaypoint({20, 0, -13});
    const int nook_sw = AddWaypoint({-20, 0, -13});
    const int wing_stair_e = AddWaypoint({29, 0, 0});
    const int wing_stair_w = AddWaypoint({-29, 0, 0});
    const int wing_e = AddWaypoint({41, WING_HEIGHT, 0});
    const int wing_w = AddWaypoint({-41, WING_HEIGHT, 0});
    const int wing_en = AddWaypoint({41, WING_HEIGHT, 14});
    const int wing_es = AddWaypoint({41, WING_HEIGHT, -14});
    const int wing_wn = AddWaypoint({-41, WING_HEIGHT, 14});
    const int wing_ws = AddWaypoint({-41, WING_HEIGHT, -14});
    const int outer_ne = AddWaypoint({30, 0, 22});
    const int outer_nw = AddWaypoint({-30, 0, 22});
    const int outer_se = AddWaypoint({30, 0, -22});
    const int outer_sw = AddWaypoint({-30, 0, -22});
    const int outer_n = AddWaypoint({0, 0, 30});
    const int outer_s = AddWaypoint({0, 0, -30});
    const int under_n = AddWaypoint({0, 0, 42});
    const int under_s = AddWaypoint({0, 0, -42});
    const int pad_ne = AddWaypoint({24, 0.15f, 34});
    const int pad_nw = AddWaypoint({-24, 0.15f, 34});
    const int pad_se = AddWaypoint({24, 0.15f, -34});
    const int pad_sw = AddWaypoint({-24, 0.15f, -34});
    const int deck_n = AddWaypoint({0, DECK_HEIGHT, 42});
    const int deck_s = AddWaypoint({0, DECK_HEIGHT, -42});
    const int deck_ne = AddWaypoint({24, DECK_HEIGHT, 42});
    const int deck_nw = AddWaypoint({-24, DECK_HEIGHT, 42});
    const int deck_se = AddWaypoint({24, DECK_HEIGHT, -42});
    const int deck_sw = AddWaypoint({-24, DECK_HEIGHT, -42});
    const int deck_ne_far = AddWaypoint({41, DECK_HEIGHT, 41});
    const int deck_sw_far = AddWaypoint({-41, DECK_HEIGHT, -41});
    const int deck_nw_far = AddWaypoint({-41, DECK_HEIGHT, 42});
    const int deck_se_far = AddWaypoint({41, DECK_HEIGHT, -42});
    const int bridge_mid = AddWaypoint({0, DECK_HEIGHT, 0});
    const int bridge_n = AddWaypoint({0, DECK_HEIGHT, 20});
    const int bridge_s = AddWaypoint({0, DECK_HEIGHT, -20});
    const int stairway_ne = AddWaypoint({41, 0, 23});
    const int stairway_sw = AddWaypoint({-41, 0, -23});

    Link(keep, keep_e);
    Link(keep, keep_w);
    Link(keep_e, stair_e);
    Link(keep_w, stair_w);
    Link(stair_e, quad_ne);
    Link(stair_e, quad_se);
    Link(stair_w, quad_nw);
    Link(stair_w, quad_sw);
    Link(center_n, quad_ne);
    Link(center_n, quad_nw);
    Link(center_s, quad_se);
    Link(center_s, quad_sw);
    Link(quad_ne, nook_ne);
    Link(quad_nw, nook_nw);
    Link(quad_se, nook_se);
    Link(quad_sw, nook_sw);
    Link(stair_e, wing_stair_e);
    Link(stair_w, wing_stair_w);
    Link(wing_stair_e, wing_e);
    Link(wing_stair_w, wing_w);
    Link(wing_e, wing_en);
    Link(wing_e, wing_es);
    Link(wing_w, wing_wn);
    Link(wing_w, wing_ws);
    Link(wing_stair_e, outer_ne);
    Link(wing_stair_e, outer_se);
    Link(wing_stair_w, outer_nw);
    Link(wing_stair_w, outer_sw);
    Link(outer_ne, outer_n);
    Link(outer_nw, outer_n);
    Link(outer_se, outer_s);
    Link(outer_sw, outer_s);
    Link(outer_n, center_n);
    Link(outer_s, center_s);
    Link(outer_n, under_n);
    Link(outer_s, under_s);
    Link(outer_n, pad_ne);
    Link(outer_n, pad_nw);
    Link(outer_s, pad_se);
    Link(outer_s, pad_sw);
    Link(outer_ne, stairway_ne);
    Link(outer_sw, stairway_sw);
    Link(stairway_ne, deck_ne_far);
    Link(stairway_sw, deck_sw_far);
    LinkOneWay(pad_ne, deck_ne);
    LinkOneWay(pad_nw, deck_nw);
    LinkOneWay(pad_se, deck_se);
    LinkOneWay(pad_sw, deck_sw);
    Link(deck_n, deck_ne);
    Link(deck_n, deck_nw);
    Link(deck_s, deck_se);
    Link(deck_s, deck_sw);
    Link(deck_ne, deck_ne_far);
    Link(deck_nw, deck_nw_far);
    Link(deck_se, deck_se_far);
    Link(deck_sw, deck_sw_far);
    Link(deck_n, bridge_n);
    Link(deck_s, bridge_s);
    Link(bridge_n, bridge_mid);
    Link(bridge_s, bridge_mid);
    LinkOneWay(deck_ne, outer_ne);
    LinkOneWay(deck_nw, outer_nw);
    LinkOneWay(deck_se, outer_se);
    LinkOneWay(deck_sw, outer_sw);
    LinkOneWay(wing_en, outer_ne);
    LinkOneWay(wing_es, outer_se);
    LinkOneWay(wing_wn, outer_nw);
    LinkOneWay(wing_ws, outer_sw);
    LinkOneWay(keep_e, quad_ne);
    LinkOneWay(keep_w, quad_sw);
}

Bool Arena::RayCast(
    const FVec3 &origin, const FVec3 &direction, const Float max_distance, RayHit &hit
) const
{
    Bool found = false;
    Float best = max_distance;
    for (const ArenaBox &box : boxes)
    {
        if (!box.solid)
        {
            continue;
        }
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
        if (arena_box.solid && primitive::Overlaps(box, arena_box.bounds))
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
