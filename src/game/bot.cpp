#include "game/bot.h"

namespace evo
{
namespace
{
const Char *BOT_NAMES[] = {"Vex",   "Kora",  "Drift", "Nyx",    "Rook",  "Sable",
                           "Talon", "Quill", "Ember", "Juniper", "Onyx", "Wren"};

constexpr Float THINK_INTERVAL = 0.12f;
constexpr Float FORGET_SECONDS = 3.0f;

// How far a bot can spot enemies: 45 m for the weakest bots, 75 m for the strongest.
Float SightRange(const Float skill)
{
    return 45.0f + 30.0f * skill;
}

Float TurnToward(const Float current, const Float desired, const Float max_step)
{
    const Float delta = math::GetWrappedAngle(desired - current);
    return math::GetWrappedAngle(current + math::Clamp(delta, -max_step, max_step));
}

WeaponId ChooseWeapon(const Player &bot, const Bool has_target, const Float distance)
{
    const auto usable = [&bot](const WeaponId weapon)
    {
        const int index = static_cast<int>(weapon);
        return bot.hasWeapon[index] && bot.ammo[index] > 0;
    };
    if (has_target)
    {
        if (distance < 7.0f && usable(WeaponId::SCATTER))
        {
            return WeaponId::SCATTER;
        }
        if (distance > 22.0f && usable(WeaponId::RAIL))
        {
            return WeaponId::RAIL;
        }
        if (distance > 5.0f && distance < 30.0f && usable(WeaponId::NOVA))
        {
            return WeaponId::NOVA;
        }
        if (distance > 6.0f && distance < 22.0f && usable(WeaponId::FRAG))
        {
            return WeaponId::FRAG;
        }
        if (distance < 26.0f && usable(WeaponId::RIPPER))
        {
            return WeaponId::RIPPER;
        }
        if (distance < 10.0f && usable(WeaponId::SCATTER))
        {
            return WeaponId::SCATTER;
        }
        if (usable(WeaponId::RIFLE))
        {
            return WeaponId::RIFLE;
        }
        const WeaponId fallback[] = {WeaponId::RAIL, WeaponId::RIPPER, WeaponId::SCATTER};
        for (const WeaponId weapon : fallback)
        {
            if (usable(weapon))
            {
                return weapon;
            }
        }
        if (distance > 4.0f && usable(WeaponId::NOVA))
        {
            return WeaponId::NOVA;
        }
        if (distance > 4.0f && usable(WeaponId::FRAG))
        {
            return WeaponId::FRAG;
        }
        return bot.weapon;
    }
    const WeaponId order[] = {WeaponId::NOVA, WeaponId::SCATTER, WeaponId::RAIL, WeaponId::RIPPER, WeaponId::FRAG};
    for (const WeaponId weapon : order)
    {
        if (usable(weapon))
        {
            return weapon;
        }
    }
    return WeaponId::RIFLE;
}

Float PickupDesire(const Player &bot, const PickupKind kind)
{
    const auto ammo_ratio = [&bot](const WeaponId weapon)
    {
        const int index = static_cast<int>(weapon);
        return static_cast<Float>(bot.ammo[index]) / static_cast<Float>(GetWeaponDef(weapon).maxAmmo);
    };
    const auto weapon_desire = [&](const WeaponId weapon)
    {
        return bot.hasWeapon[static_cast<int>(weapon)] ? (1.0f - ammo_ratio(weapon)) * 2.0f : 7.0f;
    };
    const auto ammo_desire = [&](const WeaponId weapon)
    {
        return bot.hasWeapon[static_cast<int>(weapon)] ? (1.0f - ammo_ratio(weapon)) * 2.5f : 0.3f;
    };
    switch (kind)
    {
    case PickupKind::HEALTH:
        return bot.health < MAX_HEALTH ? (MAX_HEALTH - bot.health) * 0.08f : 0.0f;
    case PickupKind::MEGA_HEALTH:
        return bot.health < MAX_MEGA_HEALTH ? 6.0f : 0.0f;
    case PickupKind::ARMOR:
        return bot.armor < MAX_ARMOR ? 4.0f : 0.0f;
    case PickupKind::WEAPON_NOVA:
        return weapon_desire(WeaponId::NOVA);
    case PickupKind::WEAPON_RAIL:
        return weapon_desire(WeaponId::RAIL);
    case PickupKind::WEAPON_SCATTER:
        return weapon_desire(WeaponId::SCATTER) * 0.9f;
    case PickupKind::WEAPON_FRAG:
        return weapon_desire(WeaponId::FRAG) * 0.8f;
    case PickupKind::WEAPON_RIPPER:
        return weapon_desire(WeaponId::RIPPER) * 0.85f;
    case PickupKind::AMMO_RIFLE:
        return (1.0f - ammo_ratio(WeaponId::RIFLE)) * 2.0f;
    case PickupKind::AMMO_NOVA:
        return ammo_desire(WeaponId::NOVA);
    case PickupKind::AMMO_RAIL:
        return ammo_desire(WeaponId::RAIL);
    case PickupKind::AMMO_SCATTER:
        return ammo_desire(WeaponId::SCATTER);
    case PickupKind::AMMO_FRAG:
        return ammo_desire(WeaponId::FRAG);
    case PickupKind::AMMO_RIPPER:
        return ammo_desire(WeaponId::RIPPER);
    case PickupKind::GRENADES:
        return bot.grenades < MAX_GRENADES ? 1.0f + 0.5f * static_cast<Float>(MAX_GRENADES - bot.grenades) : 0.0f;
    case PickupKind::MINES:
        return bot.mines < MAX_MINES ? 1.5f : 0.0f;
    default:
        return 0.0f;
    }
}

// Picks the throw direction that lands a grenade about `distance` away horizontally and
// `rise` higher (both measured from the thrower's eye).
FVec3 GrenadeThrowDirection(const FVec3 &horizontal, const Float distance, const Float rise)
{
    constexpr Float SPEED = 19.0f;
    constexpr Float LIFT = 4.5f;
    constexpr Float HALF_GRAVITY = 9.0f;
    Float best_pitch = 0.3f;
    Float best_error = 1e9f;
    for (Float pitch = -0.4f; pitch <= 0.9f; pitch += 0.025f)
    {
        const Float vx = SPEED * std::cos(pitch);
        const Float vy = SPEED * std::sin(pitch) + LIFT;
        const Float discriminant = vy * vy - 4.0f * HALF_GRAVITY * rise;
        if (discriminant < 0.0f)
        {
            continue;
        }
        const Float time = (vy + std::sqrt(discriminant)) / (2.0f * HALF_GRAVITY);
        const Float error = std::fabs(vx * time - distance);
        if (error < best_error)
        {
            best_error = error;
            best_pitch = pitch;
        }
    }
    return math::GetNormalized(horizontal * std::cos(best_pitch) + FVec3{0.0f, std::sin(best_pitch), 0.0f});
}

void ChooseGoal(World &world, Player &bot)
{
    BotBrain &brain = bot.brain;
    const Arena &arena = world.arena;
    FVec3 goal =
        arena.waypoints[world.random.NextInt(0, static_cast<int>(arena.waypoints.size()) - 1)]
            .position;
    Float best = 0.6f;

    if (brain.target >= 0 && world.time - brain.targetLastSeen < 5.0f)
    {
        goal = brain.targetLastPosition;
        best = 2.5f;
    }

    for (const Pickup &pickup : world.pickups)
    {
        if (!pickup.available && pickup.respawnTimer > 4.0f)
        {
            continue;
        }
        const Float distance = math::GetLength(pickup.position - bot.move.position);
        const Float score = PickupDesire(bot, pickup.kind) * 20.0f / (distance + 12.0f) +
                            world.random.NextRange(0.0f, 0.4f);
        if (score > best)
        {
            best = score;
            goal = pickup.position;
        }
    }

    int start = arena.FindNearestWaypoint(bot.move.position, true);
    if (start < 0)
    {
        start = arena.FindNearestWaypoint(bot.move.position, false);
    }
    const int end = arena.FindNearestWaypoint(goal, false);
    brain.path = arena.FindPath(start, end);
    brain.pathIndex = 0;
    brain.goalPosition = goal;
    brain.hasGoal = true;
    brain.repathTimer = world.random.NextRange(6.0f, 10.0f);
}

void Perceive(World &world, const int player_id, Player &bot)
{
    BotBrain &brain = bot.brain;
    const FVec3 eye = EyePosition(bot.move);
    const FVec3 view = math::GetDirectionFromYawPitch(bot.yaw, bot.pitch);
    int best = -1;
    Float best_score = 1e9f;
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        const Player &other = world.players[i];
        if (i == player_id || !other.active || !other.alive)
        {
            continue;
        }
        const FVec3 chest = other.move.position + FVec3{0.0f, 1.1f, 0.0f};
        const FVec3 delta = chest - eye;
        const Float distance = math::GetLength(delta);
        if (distance > SightRange(bot.brain.skill))
        {
            continue;
        }
        const Bool attacked_by =
            bot.lastDamageFrom == i && world.time - bot.lastDamageTime < 2.0f;
        const Bool in_view =
            math::GetDotProduct(view, delta * (1.0f / std::max(distance, 0.01f))) > 0.3f;
        if (!in_view && !attacked_by && distance > 6.0f)
        {
            continue;
        }
        if (!world.arena.HasLineOfSight(eye, chest))
        {
            continue;
        }
        const Float score = distance - (i == brain.target ? 12.0f : 0.0f) - (attacked_by ? 15.0f : 0.0f);
        if (score < best_score)
        {
            best_score = score;
            best = i;
        }
    }
    if (best < 0)
    {
        return;
    }
    if (best != brain.target || world.time - brain.targetLastSeen > 1.0f)
    {
        // Weak bots take close to a second to react to a new enemy; strong ones about a third.
        brain.reactionTimer = 0.95f - 0.6f * brain.skill + world.random.NextRange(0.0f, 0.35f);
    }
    brain.target = best;
    brain.targetLastSeen = world.time;
    brain.targetLastPosition = world.players[best].move.position;
}
} // namespace

const Char *BotName(const int index)
{
    return BOT_NAMES[index % (sizeof(BOT_NAMES) / sizeof(BOT_NAMES[0]))];
}

void UpdateBot(
    World &world, const int player_id, const Float delta_seconds, MoveInput &input,
    FireRequest &fire
)
{
    Player &bot = world.players[player_id];
    BotBrain &brain = bot.brain;
    const Arena &arena = world.arena;
    const FVec3 eye = EyePosition(bot.move);

    brain.thinkTimer -= delta_seconds;
    brain.reactionTimer -= delta_seconds;
    brain.repathTimer -= delta_seconds;
    brain.strafeTimer -= delta_seconds;
    brain.dodgeTimer -= delta_seconds;

    if (brain.target >= 0)
    {
        const Player &target = world.players[brain.target];
        if (!target.active || !target.alive || world.time - brain.targetLastSeen > FORGET_SECONDS)
        {
            brain.target = -1;
        }
    }
    if (brain.thinkTimer <= 0.0f)
    {
        brain.thinkTimer = THINK_INTERVAL;
        Perceive(world, player_id, bot);
    }

    const Bool target_visible = brain.target >= 0 && world.time - brain.targetLastSeen < 0.35f;
    const Player *target = brain.target >= 0 ? &world.players[brain.target] : nullptr;
    const Float target_distance =
        target != nullptr ? math::GetLength(target->move.position - bot.move.position) : 0.0f;

    bot.weapon = ChooseWeapon(bot, target_visible, target_distance);

    if (!brain.hasGoal || brain.repathTimer <= 0.0f)
    {
        ChooseGoal(world, bot);
    }

    FVec3 move_target = brain.goalPosition;
    while (brain.pathIndex < static_cast<int>(brain.path.size()))
    {
        const FVec3 node = arena.waypoints[brain.path[brain.pathIndex]].position;
        const Bool reached = LengthXZ(node - bot.move.position) < 1.1f &&
                             std::fabs(node.y - bot.move.position.y) < 1.8f;
        if (!reached)
        {
            move_target = node;
            break;
        }
        ++brain.pathIndex;
    }
    if (brain.pathIndex >= static_cast<int>(brain.path.size()) &&
        LengthXZ(brain.goalPosition - bot.move.position) < 1.0f)
    {
        brain.hasGoal = false;
    }

    FVec3 to_move_target = move_target - bot.move.position;
    to_move_target.y = 0.0f;
    FVec3 move_direction = math::GetNormalized(to_move_target);

    if (target_visible && target != nullptr)
    {
        FVec3 to_enemy = target->move.position - bot.move.position;
        to_enemy.y = 0.0f;
        const FVec3 enemy_direction = math::GetNormalized(to_enemy);
        const FVec3 perpendicular{enemy_direction.z, 0.0f, -enemy_direction.x};
        if (brain.strafeTimer <= 0.0f)
        {
            brain.strafeDirection = world.random.NextUnit() < 0.5f ? -1.0f : 1.0f;
            brain.strafeTimer = world.random.NextRange(0.5f, 1.4f);
        }
        Float preferred = 10.0f;
        if (bot.weapon == WeaponId::RAIL)
        {
            preferred = 25.0f;
        }
        else if (bot.weapon == WeaponId::NOVA || bot.weapon == WeaponId::FRAG)
        {
            preferred = 14.0f;
        }
        else if (bot.weapon == WeaponId::SCATTER)
        {
            preferred = 4.0f;
        }
        else if (bot.weapon == WeaponId::RIPPER)
        {
            preferred = 12.0f;
        }
        const Float horizontal_distance = LengthXZ(to_enemy);
        Float approach = 0.0f;
        if (horizontal_distance > preferred + 4.0f)
        {
            approach = 1.0f;
        }
        else if (horizontal_distance < preferred - 4.0f)
        {
            approach = -1.0f;
        }
        const FVec3 combat = math::GetNormalized(
            perpendicular * brain.strafeDirection + enemy_direction * (approach * 0.8f)
        );
        move_direction = math::GetNormalized(combat * 0.7f + move_direction * 0.3f);

        if (brain.dodgeTimer <= 0.0f && bot.move.onGround)
        {
            brain.dodgeTimer = world.random.NextRange(2.0f, 5.0f) * (1.6f - brain.skill);
            input.dodge = brain.strafeDirection > 0.0f ? DodgeDirection::RIGHT : DodgeDirection::LEFT;
        }
    }

    // Keep clear of lava, spike plates and the Maw's reach; climb out of lava right away.
    for (const Hazard &hazard : arena.hazards)
    {
        FVec3 center = hazard.position;
        Float half_x = (hazard.area.max.x - hazard.area.min.x) * 0.5f;
        Float half_z = (hazard.area.max.z - hazard.area.min.z) * 0.5f;
        if (hazard.kind == HazardKind::MAW)
        {
            center = MawBiteCenter(hazard);
            half_x = half_z = MAW_BITE_RADIUS + 0.6f;
        }
        const FVec3 offset = bot.move.position - center;
        if (std::fabs(offset.y) > 2.5f)
        {
            continue;
        }
        const Float out_x = std::fabs(offset.x) - half_x;
        const Float out_z = std::fabs(offset.z) - half_z;
        const Float gap = std::max(out_x, out_z);
        if (gap > 2.0f)
        {
            continue;
        }
        FVec3 away{offset.x, 0.0f, offset.z};
        if (LengthXZ(away) < 0.01f)
        {
            away = FVec3{1.0f, 0.0f, 0.0f};
        }
        away = math::GetNormalized(away);
        const Float push = gap < 0.0f ? 3.0f : (2.0f - gap) / 2.0f * 1.5f;
        move_direction = math::GetNormalized(move_direction + away * push);
        if (gap < 0.0f && hazard.kind == HazardKind::LAVA && bot.move.onGround)
        {
            input.jump = true;
        }
    }

    // Better bots notice armed enemy mines nearby and steer around them.
    if (world.random.NextUnit() < 0.2f + 0.8f * brain.skill)
    {
        for (const Projectile &mine : world.projectiles)
        {
            if (mine.kind != ProjectileKind::MINE || !mine.armed || mine.owner == player_id)
            {
                continue;
            }
            FVec3 away = bot.move.position - mine.position;
            away.y = 0.0f;
            const Float distance = LengthXZ(away);
            if (distance > 4.5f || distance < 0.01f || std::fabs(mine.position.y - bot.move.position.y) > 2.0f)
            {
                continue;
            }
            const Float push = (4.5f - distance) / 4.5f * (0.5f + brain.skill);
            move_direction = math::GetNormalized(move_direction + away * (push / distance));
        }
    }

    Float desired_yaw = bot.yaw;
    Float desired_pitch = 0.0f;
    FVec3 aim_point;
    if (target_visible && target != nullptr)
    {
        aim_point = target->move.position + FVec3{0.0f, 1.15f, 0.0f};
        if (bot.weapon == WeaponId::NOVA)
        {
            const Float travel = target_distance / GetWeaponDef(WeaponId::NOVA).projectileSpeed;
            FVec3 lead = target->move.velocity * (travel * brain.skill);
            lead.y = 0.0f;
            aim_point = target->move.position + lead;
            aim_point.y += target->move.onGround ? 0.3f : 1.0f;
        }
        else if (bot.weapon == WeaponId::FRAG)
        {
            const Float travel = target_distance / GetWeaponDef(WeaponId::FRAG).projectileSpeed;
            FVec3 lead = target->move.velocity * (travel * brain.skill);
            lead.y = 0.0f;
            aim_point = target->move.position + lead;
            // Aim above the target to make up for the shell dropping on the way.
            aim_point.y += 0.4f + std::max(0.0f, 9.0f * travel * travel - 3.0f * travel);
        }
        else if (bot.weapon == WeaponId::RIPPER)
        {
            const Float travel = target_distance / GetWeaponDef(WeaponId::RIPPER).projectileSpeed;
            FVec3 lead = target->move.velocity * (travel * brain.skill);
            lead.y *= 0.3f;
            aim_point = aim_point + lead;
        }
        else if (bot.weapon == WeaponId::RAIL && brain.skill > 0.6f)
        {
            aim_point.y = target->move.position.y + 1.45f;
        }
        // Aim wanders around the target; weaker bots wander further and faster, and a
        // moving target throws their aim off more.
        const Float target_speed = LengthXZ(target->move.velocity);
        const Float error_limit =
            (0.2f * (1.0f - brain.skill) + 0.03f) * (1.0f + target_speed * 0.04f);
        const Float wobble = 2.2f - 1.2f * brain.skill;
        brain.aimErrorYaw = math::Clamp(
            brain.aimErrorYaw + world.random.NextRange(-1.0f, 1.0f) * delta_seconds * wobble,
            -error_limit, error_limit
        );
        brain.aimErrorPitch = math::Clamp(
            brain.aimErrorPitch + world.random.NextRange(-1.0f, 1.0f) * delta_seconds * wobble * 0.6f,
            -error_limit * 0.6f, error_limit * 0.6f
        );
        math::GetYawPitchFromDirection(aim_point - eye, desired_yaw, desired_pitch);
        desired_yaw += brain.aimErrorYaw;
        desired_pitch += brain.aimErrorPitch;
    }
    else if (LengthXZ(move_direction) > 0.1f)
    {
        desired_yaw = std::atan2(move_direction.x, move_direction.z);
    }

    const Float turn = (1.8f + 5.0f * brain.skill) * delta_seconds;
    bot.yaw = TurnToward(bot.yaw, desired_yaw, turn);
    bot.pitch = math::Clamp(TurnToward(bot.pitch, desired_pitch, turn), -1.45f, 1.45f);

    input.yaw = bot.yaw;
    input.forward = math::GetDotProduct(move_direction, ForwardXZ(bot.yaw));
    input.right = math::GetDotProduct(move_direction, RightXZ(bot.yaw));

    brain.throwTimer -= delta_seconds;
    brain.mineTimer -= delta_seconds;
    if (target_visible && target != nullptr && brain.reactionTimer <= 0.0f && brain.throwTimer <= 0.0f &&
        bot.grenades > 0 && target_distance > 7.0f && target_distance < 26.0f &&
        bot.throwCooldown <= 0.0f)
    {
        brain.throwTimer = world.random.NextRange(4.0f, 9.0f) * (1.4f - 0.6f * brain.skill);
        FVec3 lead = target->move.velocity * (0.8f * brain.skill);
        lead.y = 0.0f;
        const FVec3 landing = target->move.position + lead;
        FVec3 horizontal = landing - eye;
        horizontal.y = 0.0f;
        const Float distance = LengthXZ(horizontal);
        if (distance > 1.0f)
        {
            const Float miss = (1.0f - brain.skill) * 0.12f;
            horizontal = math::GetNormalized(horizontal + RightXZ(bot.yaw) * world.random.NextRange(-miss, miss) * distance);
            fire.throwGrenade = true;
            fire.origin = eye;
            fire.throwDirection = GrenadeThrowDirection(horizontal, distance, landing.y + 0.3f - eye.y);
        }
    }
    else if (!target_visible && brain.mineTimer <= 0.0f && bot.mines > 0 && bot.move.onGround &&
             bot.throwCooldown <= 0.0f)
    {
        brain.mineTimer = world.random.NextRange(20.0f, 40.0f);
        fire.placeMine = true;
        fire.origin = eye;
        fire.throwDirection = math::GetNormalized(ForwardXZ(bot.yaw) * 0.35f + FVec3{0.0f, -1.0f, 0.0f});
    }

    const Bool wants_move = LengthXZ(move_direction) > 0.1f;
    if (wants_move && bot.move.onGround && LengthXZ(bot.move.velocity) < 1.5f)
    {
        brain.stuckTimer += delta_seconds;
    }
    else
    {
        brain.stuckTimer = std::max(0.0f, brain.stuckTimer - delta_seconds);
    }
    if (brain.stuckTimer > 0.5f && bot.move.onGround)
    {
        input.jump = true;
    }
    if (brain.stuckTimer > 2.0f)
    {
        brain.hasGoal = false;
        brain.path.clear();
        brain.stuckTimer = 0.0f;
    }
    if (target_visible && bot.move.onGround && world.random.NextUnit() < 0.004f)
    {
        input.jump = true;
    }

    brain.burstTimer -= delta_seconds;
    brain.burstPause -= delta_seconds;
    if (target_visible && target != nullptr && brain.reactionTimer <= 0.0f && brain.burstPause <= 0.0f)
    {
        if ((bot.weapon == WeaponId::NOVA || bot.weapon == WeaponId::FRAG) && target_distance < 3.5f)
        {
            return;
        }
        const FVec3 view = math::GetDirectionFromYawPitch(bot.yaw, bot.pitch);
        const FVec3 exact = math::GetNormalized(aim_point - eye);
        Float tolerance = 0.12f;
        if (bot.weapon == WeaponId::NOVA || bot.weapon == WeaponId::FRAG)
        {
            tolerance = 0.09f;
        }
        else if (bot.weapon == WeaponId::RIPPER)
        {
            tolerance = 0.07f;
        }
        else if (bot.weapon == WeaponId::RAIL)
        {
            tolerance = 0.04f;
        }
        if (std::acos(math::Clamp(math::GetDotProduct(view, exact), -1.0f, 1.0f)) < tolerance)
        {
            fire.fire = true;
            fire.origin = eye;
            fire.direction = view;
            // Shoot in bursts and stop to re-aim in between, longer pauses for weaker bots.
            if (brain.burstTimer <= -0.5f)
            {
                brain.burstTimer = 0.35f + 0.6f * brain.skill;
            }
            else if (brain.burstTimer <= 0.0f)
            {
                brain.burstPause = world.random.NextRange(0.4f, 0.9f) * (1.3f - brain.skill);
                brain.burstTimer = -1.0f;
            }
        }
    }
}
} // namespace evo
