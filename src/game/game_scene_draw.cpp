#include "game/game_scene.h"

namespace evo
{
namespace
{
struct Part
{
    FVec3 offset;
    FVec3 size;
    FVec3 color;
    Float emissive;
    MeshShape shape;
};

const FVec3 GUNMETAL{0.20f, 0.22f, 0.26f};
const FVec3 DARK{0.10f, 0.10f, 0.12f};

std::vector<Part> WeaponParts(const WeaponId weapon)
{
    const FVec3 color = GetWeaponDef(weapon).color;
    switch (weapon)
    {
    case WeaponId::NOVA:
        return {
            {{0.0f, 0.0f, 0.0f},
             {0.13f, 0.12f, 0.36f},
             {0.32f, 0.20f, 0.14f},
             0.0f,
             MeshShape::CUBE},
            {{0.0f, 0.0f, 0.21f}, {0.15f, 0.15f, 0.15f}, color, 1.0f, MeshShape::SPHERE},
            {{0.08f, 0.03f, -0.05f}, {0.04f, 0.07f, 0.22f}, color * 0.7f, 0.6f, MeshShape::CUBE},
            {{-0.08f, 0.03f, -0.05f}, {0.04f, 0.07f, 0.22f}, color * 0.7f, 0.6f, MeshShape::CUBE},
            {{0.0f, -0.09f, -0.08f}, {0.05f, 0.11f, 0.07f}, DARK, 0.0f, MeshShape::CUBE},
        };
    case WeaponId::RAIL:
        return {
            {{0.0f, 0.0f, -0.02f}, {0.06f, 0.08f, 0.56f}, {0.22f, 0.17f, 0.28f}, 0.0f, MeshShape::CUBE},
            {{0.045f, 0.0f, 0.08f}, {0.016f, 0.022f, 0.52f}, color, 1.0f, MeshShape::CUBE},
            {{-0.045f, 0.0f, 0.08f}, {0.016f, 0.022f, 0.52f}, color, 1.0f, MeshShape::CUBE},
            {{0.0f, 0.055f, 0.0f}, {0.025f, 0.025f, 0.36f}, {0.95f, 0.85f, 1.0f}, 1.0f, MeshShape::CUBE},
            {{0.0f, -0.09f, -0.14f}, {0.05f, 0.11f, 0.07f}, DARK, 0.0f, MeshShape::CUBE},
        };
    default:
        return {
            {{0.0f, 0.0f, 0.0f}, {0.07f, 0.09f, 0.42f}, GUNMETAL, 0.0f, MeshShape::CUBE},
            {{0.0f, 0.01f, 0.29f},
             {0.035f, 0.035f, 0.2f},
             {0.12f, 0.12f, 0.14f},
             0.0f,
             MeshShape::CUBE},
            {{0.0f, 0.052f, 0.02f}, {0.03f, 0.015f, 0.32f}, color, 1.0f, MeshShape::CUBE},
            {{0.0f, -0.08f, -0.08f}, {0.05f, 0.1f, 0.07f}, DARK, 0.0f, MeshShape::CUBE},
            {{0.0f, -0.06f, 0.1f}, {0.04f, 0.08f, 0.06f}, color * 0.5f, 0.3f, MeshShape::CUBE},
        };
    }
}

Float MuzzleDistance(const WeaponId weapon)
{
    switch (weapon)
    {
    case WeaponId::NOVA:
        return 0.3f;
    case WeaponId::RAIL:
        return 0.36f;
    default:
        return 0.4f;
    }
}
} // namespace

void GameScene::DrawWeaponModel(
    const WeaponId weapon, const FVec3 &position, const Float yaw, const Float pitch, const Float scale
)
{
    const FVec3 forward = math::GetDirectionFromYawPitch(yaw, pitch);
    const FVec3 right = RightXZ(yaw);
    const FVec3 up = math::GetCrossProduct(forward, right);
    for (const Part &part : WeaponParts(weapon))
    {
        const FVec3 center =
            position + (right * (part.offset.x * scale) + up * (part.offset.y * scale) +
                        forward * (part.offset.z * scale));
        renderer.DrawShape(
            part.shape, center, part.size * scale, part.color, part.emissive, yaw, pitch
        );
    }
}

FVec3 GameScene::ViewmodelMuzzle() const
{
    const Player *player = LocalPlayer();
    const WeaponId weapon = player != nullptr ? player->weapon : WeaponId::RIFLE;
    const FVec3 forward = math::GetDirectionFromYawPitch(cameraYaw, cameraPitch);
    const FVec3 right = RightXZ(cameraYaw);
    const FVec3 up = math::GetCrossProduct(forward, right);
    const FVec3 base = cameraEye + (right * 0.16f + up * -0.15f + forward * 0.34f);
    return base + forward * (MuzzleDistance(weapon) * 0.75f);
}

void GameScene::DrawViewmodel()
{
    const Player *player = LocalPlayer();
    if (player == nullptr || !player->alive)
    {
        return;
    }
    const FVec3 forward = math::GetDirectionFromYawPitch(cameraYaw, cameraPitch);
    const FVec3 right = RightXZ(cameraYaw);
    const FVec3 up = math::GetCrossProduct(forward, right);
    const Float speed = LengthXZ(player->move.velocity);
    const Float sway = std::sin(bobPhase) * 0.012f * math::Clamp(speed / 9.5f, 0.0f, 1.0f);
    const Float lift =
        std::fabs(std::cos(bobPhase)) * 0.01f * math::Clamp(speed / 9.5f, 0.0f, 1.0f);
    const FVec3 position =
        cameraEye + (right * (0.16f + sway) + up * (-0.15f + lift - recoil * 0.012f) +
                     forward * (0.34f - recoil * 0.05f));
    DrawWeaponModel(player->weapon, position, cameraYaw, cameraPitch + recoil * 0.06f, 0.75f);
}

void GameScene::DrawPlayer(const Player &player)
{
    const FVec3 base = player.renderPosition;
    const Float yaw = player.renderYaw;
    const FVec3 forward = ForwardXZ(yaw);
    const FVec3 right = RightXZ(yaw);
    const FVec3 up{0.0f, 1.0f, 0.0f};
    const FVec3 color = PlayerColor(player.colorIndex);
    const Float speed = math::Clamp(LengthXZ(player.move.velocity) / 9.5f, 0.0f, 1.0f);
    const Float swing = player.move.onGround ? std::sin(player.walkPhase * 2.0f) * 0.18f * speed : 0.0f;
    const Float tuck = player.move.onGround ? 0.0f : 0.12f;

    const auto place = [&](const FVec3 &offset)
    { return base + (right * offset.x + up * offset.y + forward * offset.z); };

    // A soft contact shadow on the ground below.
    RayHit ground;
    if (world.arena.RayCast(base + FVec3{0.0f, 0.2f, 0.0f}, FVec3{0.0f, -1.0f, 0.0f}, 12.0f, ground))
    {
        const Float height = ground.distance - 0.2f;
        const Float strength = math::Clamp(1.0f - height / 6.0f, 0.0f, 1.0f);
        renderer.DrawDecal(MeshShape::DISC, base - FVec3{0.0f, height, 0.0f}, {0.0f, 1.0f, 0.0f},
                           1.5f + height * 0.15f, 0.0f, {0.0f, 0.0f, 0.0f}, 0.6f * strength);
    }
    const SurfaceStyle armor{0.55f, renderer.SunVisibility(base + FVec3{0.0f, 1.2f, 0.0f})};
    const SurfaceStyle cloth{0.15f, armor.sun};

    const FVec3 leg_color{0.16f, 0.16f, 0.19f};
    const Float swing_angle = swing * 2.2f;
    // Legs swing from the hip, with knee pads and boots.
    for (const Float side : {1.0f, -1.0f})
    {
        const Float angle = side * swing_angle + tuck * 1.5f;
        const FVec3 hip = place({0.14f * side, 0.86f, 0.0f});
        const FVec3 leg_axis = (up * -std::cos(angle) + forward * std::sin(angle));
        renderer.DrawShape(MeshShape::CUBE, hip + leg_axis * (0.43f - tuck * 0.5f), {0.22f, 0.86f - tuck, 0.26f},
                           leg_color, 0.0f, yaw, angle, 0.0f, cloth);
        renderer.DrawShape(MeshShape::CUBE, hip + leg_axis * 0.5f + forward * 0.13f, {0.18f, 0.16f, 0.06f},
                           color * 0.5f, 0.0f, yaw, angle, 0.0f, armor);
        renderer.DrawShape(MeshShape::CUBE, hip + leg_axis * (0.82f - tuck * 0.5f) + forward * 0.04f,
                           {0.25f, 0.12f, 0.34f}, {0.08f, 0.08f, 0.1f}, 0.0f, yaw, angle, 0.0f, armor);
    }
    renderer.DrawShape(MeshShape::CUBE, place({0.0f, 0.92f, 0.0f}), {0.5f, 0.16f, 0.32f}, {0.12f, 0.12f, 0.14f}, 0.0f,
                       yaw, 0.0f, 0.0f, armor);
    renderer.DrawShape(MeshShape::CUBE, place({0.0f, 1.17f, 0.0f}), {0.62f, 0.64f, 0.36f}, color * 0.75f, 0.0f, yaw,
                       0.0f, 0.0f, armor);
    renderer.DrawShape(MeshShape::CUBE, place({0.0f, 1.22f, -0.2f}), {0.4f, 0.44f, 0.12f}, {0.14f, 0.14f, 0.17f}, 0.0f,
                       yaw, 0.0f, 0.0f, armor);
    renderer.DrawShape(MeshShape::CUBE, place({0.0f, 1.3f, 0.17f}), {0.42f, 0.09f, 0.04f}, color, 1.0f, yaw);
    for (const Float side : {1.0f, -1.0f})
    {
        renderer.DrawShape(MeshShape::CUBE, place({0.36f * side, 1.42f, 0.0f}), {0.24f, 0.14f, 0.4f}, color * 0.9f,
                           0.0f, yaw, 0.0f, -0.25f * side, armor);
        renderer.DrawShape(
            MeshShape::CUBE, place({0.39f * side, 1.13f, -swing * 0.5f * side}), {0.15f, 0.56f, 0.17f},
            color * 0.6f, 0.0f, yaw, 0.0f, 0.0f, cloth
        );
    }
    renderer.DrawShape(MeshShape::CUBE, place({0.0f, 1.66f, 0.0f}), {0.34f, 0.32f, 0.34f}, {0.82f, 0.83f, 0.86f}, 0.0f,
                       yaw, player.renderPitch * 0.5f, 0.0f, armor);
    const FVec3 visor_offset =
        math::GetDirectionFromYawPitch(yaw, player.renderPitch * 0.5f) * 0.16f +
        FVec3{0.0f, 0.02f, 0.0f};
    renderer.DrawShape(
        MeshShape::CUBE, place({0.0f, 1.66f, 0.0f}) + visor_offset, {0.27f, 0.08f, 0.04f}, color,
        1.0f, yaw, player.renderPitch * 0.5f
    );

    const FVec3 gun_position = place({0.26f, 1.27f, 0.0f}) +
                               math::GetDirectionFromYawPitch(yaw, player.renderPitch) * 0.3f;
    DrawWeaponModel(player.weapon, gun_position, yaw, player.renderPitch, 1.4f);
    if (player.fireFlash > 0.0f)
    {
        const FVec3 muzzle =
            gun_position + math::GetDirectionFromYawPitch(yaw, player.renderPitch) * 0.65f;
        renderer.DrawShape(MeshShape::SPHERE, muzzle, {0.25f, 0.25f, 0.25f}, GetWeaponDef(player.weapon).color, 1.0f);
        renderer.DrawGlow(muzzle, 0.9f, GetWeaponDef(player.weapon).color, 0.5f);
    }
}

void GameScene::DrawPickups()
{
    for (const Pickup &pickup : world.pickups)
    {
        const FVec3 color = PickupColor(pickup.kind);
        renderer.DrawShape(
            MeshShape::CUBE, pickup.position + FVec3{0.0f, 0.03f, 0.0f}, {1.0f, 0.06f, 1.0f},
            {0.12f, 0.12f, 0.15f}
        );
        renderer.DrawShape(
            MeshShape::CUBE, pickup.position + FVec3{0.0f, 0.04f, 0.0f}, {0.75f, 0.065f, 0.75f},
            pickup.available ? color : color * 0.2f, 1.0f
        );
        if (!pickup.available)
        {
            continue;
        }
        const Float phase = realTime * 2.0f + pickup.position.x * 0.3f + pickup.position.z * 0.17f;
        const FVec3 center = pickup.position + FVec3{0.0f, 0.75f + std::sin(phase) * 0.12f, 0.0f};
        const Float spin = realTime * 1.6f;
        renderer.DrawGlow(center, pickup.kind == PickupKind::MEGA_HEALTH ? 2.2f : 1.4f, color,
                          0.16f + 0.05f * std::sin(phase * 1.5f));
        switch (pickup.kind)
        {
        case PickupKind::HEALTH:
            renderer.DrawShape(MeshShape::CUBE, center, {0.42f, 0.42f, 0.42f}, {0.92f, 0.92f, 0.94f}, 0.2f, spin);
            renderer.DrawShape(MeshShape::CUBE, center, {0.3f, 0.1f, 0.46f}, color, 1.0f, spin);
            renderer.DrawShape(MeshShape::CUBE, center, {0.1f, 0.3f, 0.46f}, color, 1.0f, spin);
            renderer.DrawShape(MeshShape::CUBE, center, {0.46f, 0.1f, 0.3f}, color, 1.0f, spin);
            renderer.DrawShape(MeshShape::CUBE, center, {0.46f, 0.3f, 0.1f}, color, 1.0f, spin);
            break;
        case PickupKind::MEGA_HEALTH:
        {
            const Float pulse = 0.75f + 0.08f * std::sin(realTime * 5.0f);
            renderer.DrawShape(MeshShape::SPHERE, center, {pulse, pulse, pulse}, color, 1.0f);
            renderer.DrawShape(MeshShape::CUBE, center, {0.95f, 0.08f, 0.08f}, {1, 1, 1}, 1.0f, spin);
            renderer.DrawShape(MeshShape::CUBE, center, {0.08f, 0.95f, 0.08f}, {1, 1, 1}, 1.0f, spin);
            break;
        }
        case PickupKind::ARMOR:
            renderer.DrawShape(
                MeshShape::CUBE, center, {0.62f, 0.5f, 0.22f}, color * 0.8f, 0.3f, spin
            );
            renderer.DrawShape(
                MeshShape::CUBE, center + FVec3{0.0f, 0.3f, 0.0f}, {0.42f, 0.1f, 0.2f}, color, 1.0f,
                spin
            );
            break;
        case PickupKind::WEAPON_NOVA:
            DrawWeaponModel(WeaponId::NOVA, center, spin, 0.0f, 2.2f);
            break;
        case PickupKind::WEAPON_RAIL:
            DrawWeaponModel(WeaponId::RAIL, center, spin, 0.0f, 2.0f);
            break;
        default:
            for (int i = 0; i < 3; ++i)
            {
                const FVec3 offset = ForwardXZ(spin + static_cast<Float>(i) * 2.094f) * 0.18f;
                renderer.DrawShape(
                    MeshShape::CUBE, center + offset, {0.16f, 0.24f, 0.16f}, color, 0.7f, spin
                );
            }
            break;
        }
    }
}

void GameScene::DrawJumpPads()
{
    for (const JumpPad &pad : world.arena.jumpPads)
    {
        const FVec3 center{(pad.trigger.min.x + pad.trigger.max.x) * 0.5f, 0.15f,
                           (pad.trigger.min.z + pad.trigger.max.z) * 0.5f};
        const Float glow = 0.6f + 0.4f * std::sin(realTime * 4.0f);
        renderer.DrawShape(
            MeshShape::CUBE, center + FVec3{0.0f, 0.02f, 0.0f}, {1.4f, 0.03f, 1.4f},
            FVec3{0.3f, 1.0f, 1.0f} * glow, 1.0f
        );
        for (int i = 0; i < 3; ++i)
        {
            const Float t = std::fmod(realTime * 0.8f + static_cast<Float>(i) / 3.0f, 1.0f);
            const Float size = 1.6f * (1.0f - t * 0.6f);
            const FVec3 color = FVec3{0.3f, 0.95f, 1.0f} * (1.0f - t);
            const FVec3 ring = center + FVec3{0.0f, t * 2.6f, 0.0f};
            renderer.DrawShape(
                MeshShape::CUBE, ring + FVec3{size * 0.5f, 0.0f, 0.0f}, {0.05f, 0.05f, size}, color,
                1.0f
            );
            renderer.DrawShape(
                MeshShape::CUBE, ring + FVec3{-size * 0.5f, 0.0f, 0.0f}, {0.05f, 0.05f, size},
                color, 1.0f
            );
            renderer.DrawShape(
                MeshShape::CUBE, ring + FVec3{0.0f, 0.0f, size * 0.5f}, {size, 0.05f, 0.05f}, color,
                1.0f
            );
            renderer.DrawShape(
                MeshShape::CUBE, ring + FVec3{0.0f, 0.0f, -size * 0.5f}, {size, 0.05f, 0.05f},
                color, 1.0f
            );
        }
    }
}

void GameScene::DrawProjectiles()
{
    const FVec3 color = GetWeaponDef(WeaponId::NOVA).color;
    for (const Projectile &projectile : world.projectiles)
    {
        const Float pulse = 0.55f + 0.08f * std::sin(realTime * 30.0f + static_cast<Float>(projectile.id));
        renderer.DrawShape(MeshShape::SPHERE, projectile.position, {pulse, pulse, pulse}, color, 1.0f);
        renderer.DrawShape(MeshShape::SPHERE, projectile.position, {0.3f, 0.3f, 0.3f}, {1.0f, 0.95f, 0.8f}, 1.0f);
        renderer.DrawGlow(projectile.position, 1.8f, color, 0.45f);
    }
}

void GameScene::DrawWorld()
{
    // Lights first: every draw this frame uses them.
    effects.AddLights(renderer);
    for (const Projectile &projectile : world.projectiles)
    {
        renderer.AddLight(DynamicLight{projectile.position, GetWeaponDef(WeaponId::NOVA).color * 1.3f, 7.0f});
    }
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        const Player &player = world.players[i];
        if (player.active && player.alive && player.fireFlash > 0.0f)
        {
            renderer.AddLight(DynamicLight{player.renderPosition + FVec3{0.0f, 1.4f, 0.0f},
                                           GetWeaponDef(player.weapon).color * 1.5f, 6.0f});
        }
    }
    if (recoil > 0.5f)
    {
        if (const Player *local = LocalPlayer())
        {
            renderer.AddLight(DynamicLight{cameraEye, GetWeaponDef(local->weapon).color * (recoil * 0.8f), 5.0f});
        }
    }
    renderer.DrawSky();
    renderer.DrawLevel();
    DrawJumpPads();
    DrawPickups();
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        const Player &player = world.players[i];
        if (!player.active || !player.alive || IsLocal(i))
        {
            continue;
        }
        DrawPlayer(player);
    }
    DrawProjectiles();
    effects.Draw(renderer);
    DrawViewmodel();
}
} // namespace evo
