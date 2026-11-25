// The Foundry and Frostspire levels. Bastion lives in arena.cpp.
#include "game/arena.h"

namespace evo
{
namespace
{
FVec3 Mirror(const FVec3 &v, const Float sx, const Float sz)
{
    return {v.x * sx, v.y, v.z * sz};
}
} // namespace

// A closed forge hall: a raised crucible in the middle, four furnaces, and a catwalk ring at
// 5 m reached by stairs on the east and west and jump pads on the north and south.
void Arena::BuildFoundry()
{
    const FVec3 floor_color{0.24f, 0.22f, 0.21f};
    const FVec3 wall_color{0.30f, 0.27f, 0.25f};
    const FVec3 iron{0.26f, 0.27f, 0.30f};
    const FVec3 rust{0.42f, 0.27f, 0.18f};
    const FVec3 furnace_color{0.20f, 0.18f, 0.18f};
    const FVec3 stair_color{0.34f, 0.32f, 0.31f};
    const FVec3 crate_color{0.45f, 0.33f, 0.20f};
    const FVec3 molten{1.0f, 0.30f, 0.04f};
    const FVec3 hot{1.0f, 0.50f, 0.14f};
    const FVec3 pad_color{1.0f, 0.75f, 0.25f};

    theme.skyZenith = {0.05f, 0.03f, 0.03f};
    theme.skyHorizon = {0.55f, 0.20f, 0.08f};
    theme.skyAmbient = {0.20f, 0.17f, 0.17f};
    theme.groundAmbient = {0.24f, 0.12f, 0.06f};
    theme.sunDirection = math::GetNormalized(FVec3{-0.35f, 0.80f, 0.45f});
    theme.sunColor = {0.55f, 0.40f, 0.30f};
    theme.fogColor = {0.30f, 0.14f, 0.07f};
    theme.fogDensity = 0.022f;
    theme.clouds = 1.0f;
    theme.embers = 1.0f;

    constexpr Float H = 34.0f;
    constexpr Float WALL = 14.0f;
    constexpr Float RING = 5.0f;
    constexpr Float RING_BOTTOM = 4.5f;
    constexpr Float RING_IN = 28.0f;
    constexpr Float CRUCIBLE = 6.0f;
    constexpr Float CRUCIBLE_HEIGHT = 2.5f;

    AddBox({-H, -1.0f, -H}, {H, 0.0f, H}, floor_color);
    AddBox({-H - 1, 0, H}, {H + 1, WALL, H + 1}, wall_color);
    AddBox({-H - 1, 0, -H - 1}, {H + 1, WALL, -H}, wall_color);
    AddBox({H, 0, -H}, {H + 1, WALL, H}, wall_color);
    AddBox({-H - 1, 0, -H}, {-H, WALL, H}, wall_color);
    // A molten seam high on the walls.
    AddGlow({-H, 10.0f, H - 0.12f}, {H, 10.25f, H}, molten);
    AddGlow({-H, 10.0f, -H}, {H, 10.25f, -H + 0.12f}, molten);
    AddGlow({H - 0.12f, 10.0f, -H}, {H, 10.25f, H}, molten);
    AddGlow({-H, 10.0f, -H}, {-H + 0.12f, 10.25f, H}, molten);

    // The catwalk ring, its glowing inner edge and the pillars under it.
    AddBox({-H, RING_BOTTOM, RING_IN}, {H, RING, H}, iron);
    AddBox({-H, RING_BOTTOM, -H}, {H, RING, -RING_IN}, iron);
    AddBox({RING_IN, RING_BOTTOM, -RING_IN}, {H, RING, RING_IN}, iron);
    AddBox({-H, RING_BOTTOM, -RING_IN}, {-RING_IN, RING, RING_IN}, iron);
    AddGlow({-RING_IN, 4.6f, RING_IN - 0.06f}, {RING_IN, 4.85f, RING_IN}, hot);
    AddGlow({-RING_IN, 4.6f, -RING_IN}, {RING_IN, 4.85f, -RING_IN + 0.06f}, hot);
    AddGlow({RING_IN - 0.06f, 4.6f, -RING_IN}, {RING_IN, 4.85f, RING_IN}, hot);
    AddGlow({-RING_IN, 4.6f, -RING_IN}, {-RING_IN + 0.06f, 4.85f, RING_IN}, hot);
    for (const Float t : {-20.0f, -10.0f, 10.0f, 20.0f})
    {
        AddBox({t - 0.6f, 0, RING_IN}, {t + 0.6f, RING_BOTTOM, RING_IN + 1.2f}, rust);
        AddBox({t - 0.6f, 0, -RING_IN - 1.2f}, {t + 0.6f, RING_BOTTOM, -RING_IN}, rust);
        AddBox({RING_IN, 0, t - 0.6f}, {RING_IN + 1.2f, RING_BOTTOM, t + 0.6f}, rust);
        AddBox({-RING_IN - 1.2f, 0, t - 0.6f}, {-RING_IN, RING_BOTTOM, t + 0.6f}, rust);
    }
    for (const Float sx : {-1.0f, 1.0f})
    {
        for (const Float sz : {-1.0f, 1.0f})
        {
            const FVec3 a = Mirror({RING_IN, 0, RING_IN}, sx, sz);
            const FVec3 b = Mirror({RING_IN + 1.2f, RING_BOTTOM, RING_IN + 1.2f}, sx, sz);
            AddBox({std::min(a.x, b.x), 0, std::min(a.z, b.z)}, {std::max(a.x, b.x), RING_BOTTOM, std::max(a.z, b.z)}, rust);
        }
    }

    // Stairs up to the ring on the east and west.
    AddStairsX(RING_IN, 1.0f, -2.5f, 2.5f, 10, stair_color);
    AddStairsX(-RING_IN, -1.0f, -2.5f, 2.5f, 10, stair_color);

    // The crucible: a raised block with stairs on its north and south faces and a molten rim.
    AddBox({-CRUCIBLE, 0, -CRUCIBLE}, {CRUCIBLE, CRUCIBLE_HEIGHT, CRUCIBLE}, rust);
    AddGlow({-CRUCIBLE - 0.05f, 2.05f, -CRUCIBLE - 0.05f}, {CRUCIBLE + 0.05f, 2.25f, CRUCIBLE + 0.05f}, molten);
    AddStairsZ(CRUCIBLE, -1.0f, -2.0f, 2.0f, 5, stair_color);
    AddStairsZ(-CRUCIBLE, 1.0f, -2.0f, 2.0f, 5, stair_color);
    AddBox({4.8f, CRUCIBLE_HEIGHT, -3.0f}, {5.3f, CRUCIBLE_HEIGHT + 1.1f, 3.0f}, iron);
    AddBox({-5.3f, CRUCIBLE_HEIGHT, -3.0f}, {-4.8f, CRUCIBLE_HEIGHT + 1.1f, 3.0f}, iron);
    lights.push_back(ArenaLight{{0.0f, CRUCIBLE_HEIGHT + 2.0f, 0.0f}, hot * 0.9f, 10.0f});

    // Molten channels around the crucible.
    AddGlow({-8.6f, 0.0f, 8.0f}, {8.6f, 0.04f, 8.6f}, molten);
    AddGlow({-8.6f, 0.0f, -8.6f}, {8.6f, 0.04f, -8.0f}, molten);
    AddGlow({8.0f, 0.0f, -8.0f}, {8.6f, 0.04f, 8.0f}, molten);
    AddGlow({-8.6f, 0.0f, -8.0f}, {-8.0f, 0.04f, 8.0f}, molten);
    for (const Float sx : {-1.0f, 1.0f})
    {
        for (const Float sz : {-1.0f, 1.0f})
        {
            lights.push_back(ArenaLight{{sx * 8.3f, 0.8f, sz * 8.3f}, molten, 7.0f});
        }
    }

    // Four furnaces with glowing mouths that face the middle, and a chimney on each.
    for (const Float sx : {-1.0f, 1.0f})
    {
        for (const Float sz : {-1.0f, 1.0f})
        {
            const Float x0 = std::min(sx * 13.0f, sx * 19.0f);
            const Float x1 = std::max(sx * 13.0f, sx * 19.0f);
            const Float z0 = std::min(sz * 13.0f, sz * 19.0f);
            const Float z1 = std::max(sz * 13.0f, sz * 19.0f);
            AddBox({x0, 0, z0}, {x1, 9.0f, z1}, furnace_color);
            AddBox({x0 + 1.5f, 9.0f, z0 + 1.5f}, {x1 - 1.5f, 13.0f, z1 - 1.5f}, iron);
            AddGlow({x0 + 1.4f, 12.6f, z0 + 1.4f}, {x1 - 1.4f, 12.8f, z1 - 1.4f}, molten);
            const Float face_x = sx * 13.0f;
            const Float face_z = sz * 13.0f;
            AddGlow(
                {std::min(face_x, face_x - sx * 0.1f), 0.5f, std::min(sz * 14.5f, sz * 17.5f)},
                {std::max(face_x, face_x - sx * 0.1f), 3.5f, std::max(sz * 14.5f, sz * 17.5f)}, hot
            );
            AddGlow(
                {std::min(sx * 14.5f, sx * 17.5f), 0.5f, std::min(face_z, face_z - sz * 0.1f)},
                {std::max(sx * 14.5f, sx * 17.5f), 3.5f, std::max(face_z, face_z - sz * 0.1f)}, hot
            );
            lights.push_back(ArenaLight{{sx * 11.5f, 2.0f, sz * 16.0f}, hot, 9.0f});
            lights.push_back(ArenaLight{{sx * 16.0f, 2.0f, sz * 11.5f}, hot, 9.0f});
        }
    }

    // Cover walls between the crucible and the pads, and crate stacks.
    for (const Float sx : {-1.0f, 1.0f})
    {
        for (const Float sz : {-1.0f, 1.0f})
        {
            AddBox(
                {std::min(sx * 7.5f, sx * 8.5f), 0, std::min(sz * 16.0f, sz * 19.0f)},
                {std::max(sx * 7.5f, sx * 8.5f), 2.0f, std::max(sz * 16.0f, sz * 19.0f)}, iron
            );
        }
    }
    for (const Float s : {-1.0f, 1.0f})
    {
        AddBox({s * 21 - 1, 0, -s * 22 - 1}, {s * 21 + 1, 1.2f, -s * 22 + 1}, crate_color);
        AddBox({s * 23.2f - 0.8f, 0, -s * 22 - 0.8f}, {s * 23.2f + 0.8f, 2.4f, -s * 22 + 0.8f}, crate_color);
    }

    AddJumpPad({0, 0, 23.5f}, {0, 0, 1}, pad_color);
    AddJumpPad({0, 0, -23.5f}, {0, 0, -1}, pad_color);

    for (const FVec3 &p : std::initializer_list<FVec3>{
             {31, RING, 15}, {-31, RING, -15}, {15, RING, 31}, {-15, RING, -31}, {22, 0, 6}, {-22, 0, -6},
             {6, 0, 22}, {-6, 0, -22}, {24, 0, -14}, {-24, 0, 14}, {3, CRUCIBLE_HEIGHT, 3},
             {-3, CRUCIBLE_HEIGHT, -3}})
    {
        AddSpawn(p);
    }

    pickupSpots = {
        {PickupKind::MEGA_HEALTH, {0, CRUCIBLE_HEIGHT, 0}},
        {PickupKind::WEAPON_RAIL, {0, RING, 31}},
        {PickupKind::ARMOR, {0, RING, -31}},
        {PickupKind::WEAPON_NOVA, {10, 0, -10}},
        {PickupKind::WEAPON_NOVA, {-10, 0, 10}},
        {PickupKind::HEALTH, {31, RING, 24}},
        {PickupKind::HEALTH, {-31, RING, -24}},
        {PickupKind::HEALTH, {22, 0, 10}},
        {PickupKind::HEALTH, {-22, 0, -10}},
        {PickupKind::HEALTH, {0, 0, 16}},
        {PickupKind::HEALTH, {0, 0, -16}},
        {PickupKind::AMMO_RIFLE, {11, 0, 22}},
        {PickupKind::AMMO_RIFLE, {-11, 0, -22}},
        {PickupKind::AMMO_NOVA, {24, 0, -6}},
        {PickupKind::AMMO_NOVA, {-24, 0, 6}},
        {PickupKind::AMMO_RAIL, {31, RING, 31}},
        {PickupKind::AMMO_RAIL, {-31, RING, -31}},
        {PickupKind::WEAPON_SCATTER, {22, 0, 20}},
        {PickupKind::WEAPON_SCATTER, {-22, 0, -20}},
        {PickupKind::WEAPON_FRAG, {-22, 0, 20}},
        {PickupKind::WEAPON_FRAG, {22, 0, -20}},
        {PickupKind::WEAPON_RIPPER, {31, RING, -31}},
        {PickupKind::WEAPON_RIPPER, {-31, RING, 31}},
        {PickupKind::GRENADES, {16, 0, 0}},
        {PickupKind::GRENADES, {-16, 0, 0}},
        {PickupKind::MINES, {10, 0, 10}},
        {PickupKind::MINES, {-10, 0, -10}},
        {PickupKind::AMMO_SCATTER, {31, RING, 0}},
        {PickupKind::AMMO_SCATTER, {-31, RING, 0}},
        {PickupKind::AMMO_FRAG, {0, 0.2f, 23.5f}},
        {PickupKind::AMMO_FRAG, {0, 0.2f, -23.5f}},
        {PickupKind::AMMO_RIPPER, {31, RING, -24}},
        {PickupKind::AMMO_RIPPER, {-31, RING, 24}},
    };

    // Bot navigation graph.
    const int c = AddWaypoint({0, CRUCIBLE_HEIGHT, 0});
    const int cn = AddWaypoint({0, CRUCIBLE_HEIGHT, 4});
    const int cs = AddWaypoint({0, CRUCIBLE_HEIGHT, -4});
    const int cnb = AddWaypoint({0, 0, 13});
    const int csb = AddWaypoint({0, 0, -13});
    const int q_ne = AddWaypoint({10, 0, 10});
    const int q_nw = AddWaypoint({-10, 0, 10});
    const int q_se = AddWaypoint({10, 0, -10});
    const int q_sw = AddWaypoint({-10, 0, -10});
    const int e_bot = AddWaypoint({16, 0, 0});
    const int e_mid = AddWaypoint({23.5f, 3.0f, 0});
    const int e_top = AddWaypoint({28.6f, RING, 0});
    const int w_bot = AddWaypoint({-16, 0, 0});
    const int w_mid = AddWaypoint({-23.5f, 3.0f, 0});
    const int w_top = AddWaypoint({-28.6f, RING, 0});
    const int o_ne = AddWaypoint({22, 0, 20});
    const int o_nw = AddWaypoint({-22, 0, 20});
    const int o_se = AddWaypoint({22, 0, -20});
    const int o_sw = AddWaypoint({-22, 0, -20});
    const int pad_n = AddWaypoint({0, 0.15f, 23.5f});
    const int pad_s = AddWaypoint({0, 0.15f, -23.5f});
    const int r_e = AddWaypoint({31, RING, 0});
    const int r_ne = AddWaypoint({31, RING, 31});
    const int r_n = AddWaypoint({0, RING, 31});
    const int r_nw = AddWaypoint({-31, RING, 31});
    const int r_w = AddWaypoint({-31, RING, 0});
    const int r_sw = AddWaypoint({-31, RING, -31});
    const int r_s = AddWaypoint({0, RING, -31});
    const int r_se = AddWaypoint({31, RING, -31});

    Link(c, cn);
    Link(c, cs);
    Link(cn, cnb);
    Link(cs, csb);
    Link(cnb, q_ne);
    Link(cnb, q_nw);
    Link(csb, q_se);
    Link(csb, q_sw);
    Link(q_ne, e_bot);
    Link(q_se, e_bot);
    Link(q_nw, w_bot);
    Link(q_sw, w_bot);
    Link(e_bot, e_mid);
    Link(e_mid, e_top);
    Link(w_bot, w_mid);
    Link(w_mid, w_top);
    Link(e_top, r_e);
    Link(w_top, r_w);
    Link(o_ne, e_bot);
    Link(o_se, e_bot);
    Link(o_nw, w_bot);
    Link(o_sw, w_bot);
    Link(o_ne, pad_n);
    Link(o_nw, pad_n);
    Link(o_se, pad_s);
    Link(o_sw, pad_s);
    Link(cnb, pad_n);
    Link(csb, pad_s);
    LinkOneWay(pad_n, r_n);
    LinkOneWay(pad_s, r_s);
    Link(r_e, r_ne);
    Link(r_ne, r_n);
    Link(r_n, r_nw);
    Link(r_nw, r_w);
    Link(r_w, r_sw);
    Link(r_sw, r_s);
    Link(r_s, r_se);
    Link(r_se, r_e);
    LinkOneWay(r_ne, o_ne);
    LinkOneWay(r_nw, o_nw);
    LinkOneWay(r_se, o_se);
    LinkOneWay(r_sw, o_sw);
}

// An open snowfield at night: two ice towers joined by a sky bridge, ice monoliths in the middle
// and a raised ledge against the north and south walls.
void Arena::BuildFrostspire()
{
    const FVec3 snow{0.62f, 0.68f, 0.76f};
    const FVec3 wall_color{0.34f, 0.40f, 0.50f};
    const FVec3 tower_color{0.40f, 0.46f, 0.58f};
    const FVec3 ice{0.45f, 0.62f, 0.78f};
    const FVec3 stone{0.38f, 0.40f, 0.46f};
    const FVec3 stair_color{0.50f, 0.55f, 0.64f};
    const FVec3 crate_color{0.30f, 0.34f, 0.42f};
    const FVec3 cyan{0.25f, 0.85f, 1.0f};
    const FVec3 violet{0.65f, 0.40f, 1.0f};

    theme.skyZenith = {0.01f, 0.02f, 0.06f};
    theme.skyHorizon = {0.10f, 0.20f, 0.32f};
    theme.skyAmbient = {0.26f, 0.32f, 0.46f};
    theme.groundAmbient = {0.16f, 0.18f, 0.24f};
    theme.sunDirection = math::GetNormalized(FVec3{-0.40f, 0.62f, -0.55f});
    theme.sunColor = {0.55f, 0.66f, 0.90f};
    theme.fogColor = {0.10f, 0.16f, 0.24f};
    theme.fogDensity = 0.010f;
    theme.stars = 1.0f;
    theme.aurora = 1.0f;
    theme.clouds = 0.15f;

    constexpr Float H = 40.0f;
    constexpr Float WALL = 9.0f;
    constexpr Float TOWER = 6.0f;
    constexpr Float LEDGE = 3.0f;

    AddBox({-H, -1.0f, -H}, {H, 0.0f, H}, snow);
    AddBox({-H - 1, 0, H}, {H + 1, WALL, H + 1}, wall_color);
    AddBox({-H - 1, 0, -H - 1}, {H + 1, WALL, -H}, wall_color);
    AddBox({H, 0, -H}, {H + 1, WALL, H}, wall_color);
    AddBox({-H - 1, 0, -H}, {-H, WALL, H}, wall_color);
    AddGlow({-H, WALL - 0.4f, H - 0.12f}, {H, WALL - 0.2f, H}, cyan);
    AddGlow({-H, WALL - 0.4f, -H}, {H, WALL - 0.2f, -H + 0.12f}, cyan);
    AddGlow({H - 0.12f, WALL - 0.4f, -H}, {H, WALL - 0.2f, H}, cyan);
    AddGlow({-H, WALL - 0.4f, -H}, {-H + 0.12f, WALL - 0.2f, H}, cyan);

    // Towers, their stairs, and the bridge between them.
    for (const Float s : {-1.0f, 1.0f})
    {
        AddBox({std::min(s * 20, s * 32), 0, -7}, {std::max(s * 20, s * 32), TOWER, 7}, tower_color);
        AddGlow({std::min(s * 19.95f, s * 32.05f), TOWER - 0.45f, -7.05f}, {std::max(s * 19.95f, s * 32.05f), TOWER - 0.3f, 7.05f}, cyan);
        AddBox({std::min(s * 29, s * 31), TOWER, -5}, {std::max(s * 29, s * 31), TOWER + 1.2f, -3}, ice);
        AddBox({std::min(s * 29, s * 31), TOWER, 3}, {std::max(s * 29, s * 31), TOWER + 1.2f, 5}, ice);
        lights.push_back(ArenaLight{{s * 26, TOWER + 2.0f, 0}, cyan * 0.8f, 10.0f});
    }
    AddStairsZ(7.0f, -1.0f, 24.0f, 28.0f, 12, stair_color);
    AddStairsZ(-7.0f, 1.0f, -28.0f, -24.0f, 12, stair_color);
    AddBox({-20, TOWER - 0.5f, -2}, {20, TOWER, 2}, stone);
    AddGlow({-20, TOWER, 1.85f}, {20, TOWER + 0.2f, 2.0f}, violet);
    AddGlow({-20, TOWER, -2.0f}, {20, TOWER + 0.2f, -1.85f}, violet);
    for (const Float x : {-10.0f, 10.0f})
    {
        AddBox({x - 0.6f, 0, -0.6f}, {x + 0.6f, TOWER - 0.5f, 0.6f}, stone);
        lights.push_back(ArenaLight{{x, TOWER + 1.0f, 0}, violet * 0.8f, 7.0f});
    }
    lights.push_back(ArenaLight{{0, TOWER + 1.0f, 0}, violet * 0.8f, 7.0f});

    // Ice monoliths flanking the middle.
    for (const Float s : {-1.0f, 1.0f})
    {
        AddBox({-2, 0, std::min(s * 8, s * 12)}, {2, 11, std::max(s * 8, s * 12)}, ice);
        AddGlow({-0.3f, 0.5f, std::min(s * 7.95f, s * 12.05f)}, {0.3f, 10.5f, std::max(s * 7.95f, s * 12.05f)}, cyan);
        AddGlow({-2.05f, 0.5f, s * 10 - 0.3f}, {2.05f, 10.5f, s * 10 + 0.3f}, cyan);
        lights.push_back(ArenaLight{{0, 3.0f, s * 6.0f}, cyan, 9.0f});
        lights.push_back(ArenaLight{{3.5f, 3.0f, s * 10}, cyan * 0.7f, 7.0f});
        lights.push_back(ArenaLight{{-3.5f, 3.0f, s * 10}, cyan * 0.7f, 7.0f});
    }

    // Ledges against the north and south walls with stairs up the middle.
    for (const Float s : {-1.0f, 1.0f})
    {
        AddBox({-8, 0, std::min(s * 33, s * H)}, {8, LEDGE, std::max(s * 33, s * H)}, stone);
        AddGlow({-8.05f, LEDGE - 0.3f, std::min(s * 32.95f, s * H)}, {8.05f, LEDGE - 0.15f, std::max(s * 32.95f, s * H)}, violet);
        lights.push_back(ArenaLight{{0, LEDGE + 1.5f, s * 36}, violet * 0.7f, 8.0f});
    }
    AddStairsZ(33.0f, 1.0f, -2.0f, 2.0f, 6, stair_color);
    AddStairsZ(-33.0f, -1.0f, -2.0f, 2.0f, 6, stair_color);

    // L-shaped snow walls in every quadrant, and crates behind the towers.
    for (const Float sx : {-1.0f, 1.0f})
    {
        for (const Float sz : {-1.0f, 1.0f})
        {
            const auto add_mirrored = [&](const Float x0, const Float z0, const Float x1, const Float z1, const Float height)
            {
                AddBox(
                    {std::min(sx * x0, sx * x1), 0, std::min(sz * z0, sz * z1)},
                    {std::max(sx * x0, sx * x1), height, std::max(sz * z0, sz * z1)}, wall_color
                );
            };
            add_mirrored(10, 22, 16, 23, 2.5f);
            add_mirrored(10, 17, 11, 23, 2.5f);
        }
    }
    AddBox({33, 0, 29}, {35, 1.2f, 31}, crate_color);
    AddBox({-35, 0, -31}, {-33, 1.2f, -29}, crate_color);

    AddJumpPad({15.5f, 0, 4}, {1, 0, 0}, cyan);
    AddJumpPad({-15.5f, 0, -4}, {-1, 0, 0}, cyan);

    for (const FVec3 &p : std::initializer_list<FVec3>{
             {36, 0, 10}, {-36, 0, -10}, {26, TOWER, -4}, {-26, TOWER, 4}, {5, LEDGE, 37}, {-5, LEDGE, -37},
             {14, 0, -6}, {-14, 0, 6}, {20, 0, 28}, {-20, 0, -28}, {30, 0, -34}, {-30, 0, 34}})
    {
        AddSpawn(p);
    }

    pickupSpots = {
        {PickupKind::MEGA_HEALTH, {0, 0, 0}},
        {PickupKind::WEAPON_RAIL, {0, TOWER, 0}},
        {PickupKind::WEAPON_NOVA, {28, TOWER, 0}},
        {PickupKind::WEAPON_NOVA, {-28, TOWER, 0}},
        {PickupKind::ARMOR, {0, LEDGE, 36}},
        {PickupKind::ARMOR, {0, LEDGE, -36}},
        {PickupKind::HEALTH, {36, 0, 0}},
        {PickupKind::HEALTH, {-36, 0, 0}},
        {PickupKind::HEALTH, {12, 0, 12}},
        {PickupKind::HEALTH, {-12, 0, -12}},
        {PickupKind::AMMO_RIFLE, {-12, 0, 12}},
        {PickupKind::AMMO_RIFLE, {12, 0, -12}},
        {PickupKind::AMMO_NOVA, {30, 0, 22}},
        {PickupKind::AMMO_NOVA, {-30, 0, -22}},
        {PickupKind::AMMO_RAIL, {5, LEDGE, 36}},
        {PickupKind::AMMO_RAIL, {-5, LEDGE, -36}},
        {PickupKind::WEAPON_SCATTER, {12, 0, -4}},
        {PickupKind::WEAPON_SCATTER, {-12, 0, 4}},
        {PickupKind::WEAPON_FRAG, {26, 0, 20.5f}},
        {PickupKind::WEAPON_FRAG, {-26, 0, -20.5f}},
        {PickupKind::WEAPON_RIPPER, {19, TOWER, 0}},
        {PickupKind::WEAPON_RIPPER, {-19, TOWER, 0}},
        {PickupKind::GRENADES, {0, 0, 25.5f}},
        {PickupKind::GRENADES, {0, 0, -25.5f}},
        {PickupKind::MINES, {34, 0, -12}},
        {PickupKind::MINES, {-34, 0, 12}},
        {PickupKind::AMMO_SCATTER, {26, 3, 13.5f}},
        {PickupKind::AMMO_SCATTER, {-26, 3, -13.5f}},
        {PickupKind::AMMO_FRAG, {15.5f, 0.2f, 4}},
        {PickupKind::AMMO_FRAG, {-15.5f, 0.2f, -4}},
        {PickupKind::AMMO_RIPPER, {26, TOWER, 7.4f}},
        {PickupKind::AMMO_RIPPER, {-26, TOWER, -7.4f}},
    };

    // Bot navigation graph.
    const int m = AddWaypoint({0, 0, 0});
    const int ge = AddWaypoint({12, 0, -4});
    const int gw = AddWaypoint({-12, 0, 4});
    const int pad_e = AddWaypoint({15.5f, 0.15f, 4});
    const int pad_w = AddWaypoint({-15.5f, 0.15f, -4});
    const int q_ne = AddWaypoint({12, 0, 12});
    const int q_nw = AddWaypoint({-12, 0, 12});
    const int q_se = AddWaypoint({12, 0, -12});
    const int q_sw = AddWaypoint({-12, 0, -12});
    const int t_e = AddWaypoint({26, TOWER, 0});
    const int t_w = AddWaypoint({-26, TOWER, 0});
    const int b_e = AddWaypoint({19, TOWER, 0});
    const int b_mid = AddWaypoint({0, TOWER, 0});
    const int b_w = AddWaypoint({-19, TOWER, 0});
    const int ste_bot = AddWaypoint({26, 0, 20.5f});
    const int ste_mid = AddWaypoint({26, 3.0f, 13.5f});
    const int ste_top = AddWaypoint({26, TOWER, 7.4f});
    const int stw_bot = AddWaypoint({-26, 0, -20.5f});
    const int stw_mid = AddWaypoint({-26, 3.0f, -13.5f});
    const int stw_top = AddWaypoint({-26, TOWER, -7.4f});
    const int pn_bot = AddWaypoint({0, 0, 25.5f});
    const int pn = AddWaypoint({0, LEDGE, 36});
    const int ps_bot = AddWaypoint({0, 0, -25.5f});
    const int ps = AddWaypoint({0, LEDGE, -36});
    const int be = AddWaypoint({36, 0, 0});
    const int bw = AddWaypoint({-36, 0, 0});
    const int se_out = AddWaypoint({34, 0, -12});
    const int nw_out = AddWaypoint({-34, 0, 12});

    Link(m, ge);
    Link(m, gw);
    Link(m, q_ne);
    Link(m, q_nw);
    Link(m, q_se);
    Link(m, q_sw);
    Link(m, pad_e);
    Link(m, pad_w);
    Link(ge, pad_e);
    Link(gw, pad_w);
    Link(ge, q_se);
    Link(gw, q_nw);
    LinkOneWay(pad_e, t_e);
    LinkOneWay(pad_w, t_w);
    Link(t_e, b_e);
    Link(b_e, b_mid);
    Link(b_mid, b_w);
    Link(b_w, t_w);
    Link(t_e, ste_top);
    Link(ste_top, ste_mid);
    Link(ste_mid, ste_bot);
    Link(t_w, stw_top);
    Link(stw_top, stw_mid);
    Link(stw_mid, stw_bot);
    Link(q_ne, ste_bot);
    Link(q_sw, stw_bot);
    Link(q_ne, pn_bot);
    Link(q_nw, pn_bot);
    Link(q_se, ps_bot);
    Link(q_sw, ps_bot);
    Link(pn_bot, pn);
    Link(ps_bot, ps);
    Link(ste_bot, be);
    Link(be, se_out);
    Link(se_out, q_se);
    Link(stw_bot, bw);
    Link(bw, nw_out);
    Link(nw_out, q_nw);
    LinkOneWay(t_e, be);
    LinkOneWay(t_w, bw);
    LinkOneWay(t_e, q_se);
    LinkOneWay(t_w, q_nw);
}
} // namespace evo
