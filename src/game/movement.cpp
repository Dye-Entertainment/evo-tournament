#include "game/movement.h"

namespace evo
{
namespace
{
constexpr Float GRAVITY = 24.0f;
constexpr Float JUMP_SPEED = 8.2f;
constexpr Float DOUBLE_JUMP_SPEED = 7.4f;
constexpr Float MAX_GROUND_SPEED = 9.5f;
constexpr Float GROUND_ACCELERATION = 12.0f;
constexpr Float AIR_ACCELERATION = 2.5f;
constexpr Float FRICTION = 8.0f;
constexpr Float STOP_SPEED = 3.0f;
constexpr Float DODGE_SPEED = 13.0f;
constexpr Float DODGE_UP_SPEED = 4.5f;
constexpr Float DODGE_COOLDOWN = 0.75f;
constexpr Float STEP_HEIGHT = 0.55f;
constexpr Float SNAP_DISTANCE = 0.6f;
constexpr Float SKIN = 0.001f;
constexpr Float MAX_SUBSTEP = 0.25f;

Float &AxisValue(FVec3 &v, const int axis)
{
    return axis == 0 ? v.x : (axis == 1 ? v.y : v.z);
}

Float AxisValue(const FVec3 &v, const int axis)
{
    return axis == 0 ? v.x : (axis == 1 ? v.y : v.z);
}

void Accelerate(FVec3 &velocity, const FVec3 &wish_direction, const Float wish_speed,
                const Float acceleration, const Float delta_seconds)
{
    const Float current_speed = Dot(velocity, wish_direction);
    const Float add_speed = wish_speed - current_speed;
    if (add_speed <= 0.0f)
    {
        return;
    }
    const Float accel_speed = std::min(acceleration * delta_seconds * wish_speed, add_speed);
    velocity.x += wish_direction.x * accel_speed;
    velocity.z += wish_direction.z * accel_speed;
}

void ApplyFriction(FVec3 &velocity, const Float delta_seconds)
{
    const Float speed = LengthXZ(velocity);
    if (speed < 0.01f)
    {
        velocity.x = 0.0f;
        velocity.z = 0.0f;
        return;
    }
    const Float control = std::max(speed, STOP_SPEED);
    const Float new_speed = std::max(0.0f, speed - control * FRICTION * delta_seconds);
    const Float ratio = new_speed / speed;
    velocity.x *= ratio;
    velocity.z *= ratio;
}

void MoveAxis(MoveState &state, const int axis, const Float amount, const Bool can_step,
              const Arena &arena)
{
    if (amount == 0.0f)
    {
        return;
    }
    AxisValue(state.position, axis) += amount;
    Aabb bounds = PlayerBounds(state.position);
    for (const ArenaBox &box : arena.boxes)
    {
        if (!Overlaps(bounds, box.bounds))
        {
            continue;
        }
        if (axis != 1 && can_step)
        {
            const Float rise = box.bounds.max.y - state.position.y;
            if (rise > 0.0f && rise <= STEP_HEIGHT)
            {
                FVec3 candidate = state.position;
                candidate.y = box.bounds.max.y + SKIN;
                if (!arena.IsBlocked(PlayerBounds(candidate)))
                {
                    state.position = candidate;
                    bounds = PlayerBounds(state.position);
                    continue;
                }
            }
        }
        if (axis == 1)
        {
            if (amount < 0.0f)
            {
                state.position.y = box.bounds.max.y + SKIN;
                state.onGround = true;
            }
            else
            {
                state.position.y = box.bounds.min.y - PLAYER_HEIGHT - SKIN;
            }
            state.velocity.y = 0.0f;
        }
        else
        {
            if (amount > 0.0f)
            {
                AxisValue(state.position, axis) =
                    AxisValue(box.bounds.min, axis) - PLAYER_HALF_WIDTH - SKIN;
            }
            else
            {
                AxisValue(state.position, axis) =
                    AxisValue(box.bounds.max, axis) + PLAYER_HALF_WIDTH + SKIN;
            }
            AxisValue(state.velocity, axis) = 0.0f;
        }
        bounds = PlayerBounds(state.position);
    }
}

FVec3 DodgeVector(const DodgeDirection direction, const Float yaw)
{
    switch (direction)
    {
    case DodgeDirection::FORWARD:
        return ForwardXZ(yaw);
    case DodgeDirection::BACK:
        return Scale(ForwardXZ(yaw), -1.0f);
    case DodgeDirection::LEFT:
        return Scale(RightXZ(yaw), -1.0f);
    case DodgeDirection::RIGHT:
        return RightXZ(yaw);
    default:
        return FVec3{0.0f, 0.0f, 0.0f};
    }
}

Bool TrySnapToGround(MoveState &state, const Arena &arena)
{
    const Aabb bounds = PlayerBounds(state.position);
    Float best_top = -1e9f;
    for (const ArenaBox &box : arena.boxes)
    {
        if (box.bounds.min.x >= bounds.max.x || box.bounds.max.x <= bounds.min.x ||
            box.bounds.min.z >= bounds.max.z || box.bounds.max.z <= bounds.min.z)
        {
            continue;
        }
        const Float top = box.bounds.max.y;
        if (top <= state.position.y + SKIN && top >= state.position.y - SNAP_DISTANCE)
        {
            best_top = std::max(best_top, top);
        }
    }
    if (best_top < -1e8f)
    {
        return false;
    }
    state.position.y = best_top + SKIN;
    state.velocity.y = 0.0f;
    state.onGround = true;
    return true;
}
} // namespace

Aabb PlayerBounds(const FVec3 &feet_position)
{
    return Aabb{
        FVec3{feet_position.x - PLAYER_HALF_WIDTH, feet_position.y, feet_position.z - PLAYER_HALF_WIDTH},
        FVec3{
            feet_position.x + PLAYER_HALF_WIDTH, feet_position.y + PLAYER_HEIGHT,
            feet_position.z + PLAYER_HALF_WIDTH
        }
    };
}

FVec3 EyePosition(const MoveState &state)
{
    return Add(state.position, FVec3{0.0f, PLAYER_EYE_HEIGHT, 0.0f});
}

void StepMovement(
    MoveState &state, const MoveInput &input, const Float delta_seconds, const Arena &arena,
    MoveEvents &events
)
{
    state.dodgeCooldown = std::max(0.0f, state.dodgeCooldown - delta_seconds);
    state.padCooldown = std::max(0.0f, state.padCooldown - delta_seconds);

    FVec3 wish = Add(Scale(ForwardXZ(input.yaw), input.forward), Scale(RightXZ(input.yaw), input.right));
    Float wish_length = LengthXZ(wish);
    if (wish_length > 1.0f)
    {
        wish = Scale(wish, 1.0f / wish_length);
        wish_length = 1.0f;
    }
    const FVec3 wish_direction = wish_length > 0.0f ? Scale(wish, 1.0f / wish_length) : FVec3{0, 0, 0};
    const Float wish_speed = wish_length * MAX_GROUND_SPEED;

    const Bool was_on_ground = state.onGround;
    Bool left_ground_on_purpose = false;

    if (state.onGround)
    {
        ApplyFriction(state.velocity, delta_seconds);
        if (input.dodge != DodgeDirection::NONE && state.dodgeCooldown <= 0.0f)
        {
            const FVec3 dodge = DodgeVector(input.dodge, input.yaw);
            state.velocity.x = dodge.x * DODGE_SPEED;
            state.velocity.z = dodge.z * DODGE_SPEED;
            state.velocity.y = DODGE_UP_SPEED;
            state.onGround = false;
            state.airJumps = 0;
            state.dodgeCooldown = DODGE_COOLDOWN;
            events.dodged = true;
            left_ground_on_purpose = true;
        }
        else
        {
            Accelerate(state.velocity, wish_direction, wish_speed, GROUND_ACCELERATION, delta_seconds);
            if (input.jump)
            {
                state.velocity.y = JUMP_SPEED;
                state.onGround = false;
                state.airJumps = 1;
                events.jumped = true;
                left_ground_on_purpose = true;
            }
        }
    }
    else
    {
        if (input.jump && state.airJumps > 0 && state.velocity.y < 6.0f)
        {
            state.velocity.y = DOUBLE_JUMP_SPEED;
            state.airJumps = 0;
            events.doubleJumped = true;
        }
        Accelerate(state.velocity, wish_direction, wish_speed, AIR_ACCELERATION, delta_seconds);
    }

    state.velocity.y -= GRAVITY * delta_seconds;
    const Float falling_speed = -state.velocity.y;

    const FVec3 motion = Scale(state.velocity, delta_seconds);
    const int substeps =
        std::max(1, static_cast<int>(std::ceil(Length(motion) / MAX_SUBSTEP)));
    const FVec3 step = Scale(motion, 1.0f / static_cast<Float>(substeps));
    const Bool can_step = state.onGround || was_on_ground;
    for (int i = 0; i < substeps; ++i)
    {
        const Bool step_allowed = can_step && !left_ground_on_purpose;
        MoveAxis(state, 0, step.x, step_allowed, arena);
        MoveAxis(state, 2, step.z, step_allowed, arena);
        state.onGround = false;
        MoveAxis(state, 1, step.y, false, arena);
    }

    if (!state.onGround && was_on_ground && !left_ground_on_purpose && state.velocity.y <= 0.0f)
    {
        TrySnapToGround(state, arena);
    }

    if (state.onGround && !was_on_ground)
    {
        events.landingSpeed = falling_speed;
    }

    if (state.padCooldown <= 0.0f)
    {
        const FVec3 probe = Add(state.position, FVec3{0.0f, 0.05f, 0.0f});
        for (const JumpPad &pad : arena.jumpPads)
        {
            if (Contains(pad.trigger, probe))
            {
                state.velocity = pad.launchVelocity;
                state.onGround = false;
                state.airJumps = 1;
                state.padCooldown = 0.6f;
                events.launched = true;
                break;
            }
        }
    }
}
} // namespace evo
