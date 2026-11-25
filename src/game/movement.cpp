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

// Wall running: airborne, holding forward and moving along a wall fast enough.
constexpr Float WALL_PROBE = 0.2f;
constexpr Float WALL_RUN_MIN_SPEED = 5.0f;
constexpr Float WALL_RUN_KEEP_SPEED = 3.0f;
constexpr Float WALL_RUN_SPEED = 10.5f;
constexpr Float WALL_RUN_MAX_TIME = 1.4f;
constexpr Float WALL_RUN_START_LIFT = 2.0f;
constexpr Float WALL_RUN_MAX_RISE = 4.0f;
constexpr Float WALL_RUN_MAX_SINK = 3.0f;
constexpr Float WALL_RUN_STICK = 1.0f;
constexpr Float WALL_JUMP_PUSH = 7.5f;
constexpr Float WALL_JUMP_UP = 7.8f;

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
    const Float current_speed = math::GetDotProduct(velocity, wish_direction);
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
    primitive::AxisAlignedBox bounds = PlayerBounds(state.position);
    for (const ArenaBox &box : arena.boxes)
    {
        if (!box.solid || !primitive::Overlaps(bounds, box.bounds))
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
        return -ForwardXZ(yaw);
    case DodgeDirection::LEFT:
        return -RightXZ(yaw);
    case DodgeDirection::RIGHT:
        return RightXZ(yaw);
    default:
        return FVec3{0.0f, 0.0f, 0.0f};
    }
}

FVec3 WallTangent(const FVec3 &normal, const FVec3 &velocity)
{
    const FVec3 tangent{-normal.z, 0.0f, normal.x};
    return math::GetDotProduct(tangent, velocity) >= 0.0f ? tangent : -tangent;
}

// Looks for a wall beside the player's torso. Walls are axis-aligned, so there are only four
// candidate normals; with two (a corner) the one the player moves along fastest wins.
Bool FindWall(const FVec3 &position, const FVec3 &velocity, const Arena &arena, FVec3 &normal)
{
    const FVec3 directions[4] = {{1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
                                 {0.0f, 0.0f, -1.0f}};
    Float best_speed = -1.0f;
    for (const FVec3 &direction : directions)
    {
        primitive::AxisAlignedBox probe = PlayerBounds(position + direction * WALL_PROBE);
        probe.min.y = position.y + 0.8f;
        probe.max.y = position.y + 1.2f;
        if (!arena.IsBlocked(probe))
        {
            continue;
        }
        const Float along_speed = std::fabs(velocity.x * direction.z - velocity.z * direction.x);
        if (along_speed > best_speed)
        {
            best_speed = along_speed;
            normal = -direction;
        }
    }
    return best_speed >= 0.0f;
}

void EndWallRun(MoveState &state)
{
    if (state.wallRunning)
    {
        state.lastWallNormal = state.wallNormal;
    }
    state.wallRunning = false;
}

void UpdateWallRun(MoveState &state, const MoveInput &input, const FVec3 &wish_direction,
                   const Float delta_seconds, const Arena &arena, MoveEvents &events)
{
    FVec3 normal{0.0f, 0.0f, 0.0f};
    const Bool touching =
        input.allowWallRun && FindWall(state.position, state.velocity, arena, normal);
    const Bool pushing_off = math::GetDotProduct(wish_direction, normal) > 0.5f;
    const Float along_speed =
        touching ? math::GetDotProduct(WallTangent(normal, state.velocity), state.velocity) : 0.0f;

    if (state.wallRunning)
    {
        state.wallRunTime += delta_seconds;
        const Bool same_wall = touching && math::GetDotProduct(normal, state.wallNormal) > 0.9f;
        if (!same_wall || input.forward <= 0.0f || pushing_off ||
            along_speed < WALL_RUN_KEEP_SPEED || state.wallRunTime >= WALL_RUN_MAX_TIME)
        {
            EndWallRun(state);
        }
        return;
    }

    // Waiting until a jump's rise slows keeps its full height; the run then carries it on.
    if (!touching || input.forward <= 0.0f || pushing_off || along_speed < WALL_RUN_MIN_SPEED ||
        state.velocity.y > WALL_RUN_MAX_RISE || math::GetDotProduct(normal, state.lastWallNormal) > 0.9f)
    {
        return;
    }
    state.wallRunning = true;
    state.wallRunTime = 0.0f;
    state.wallNormal = normal;
    state.velocity.y = std::max(state.velocity.y, WALL_RUN_START_LIFT);
    events.wallRunStarted = true;
}

Bool TrySnapToGround(MoveState &state, const Arena &arena)
{
    const primitive::AxisAlignedBox bounds = PlayerBounds(state.position);
    Float best_top = -1e9f;
    for (const ArenaBox &box : arena.boxes)
    {
        if (!box.solid || box.bounds.min.x >= bounds.max.x || box.bounds.max.x <= bounds.min.x ||
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

primitive::AxisAlignedBox PlayerBounds(const FVec3 &feet_position)
{
    return primitive::AxisAlignedBox{
        FVec3{
            feet_position.x - PLAYER_HALF_WIDTH, feet_position.y,
            feet_position.z - PLAYER_HALF_WIDTH
        },
        FVec3{
            feet_position.x + PLAYER_HALF_WIDTH, feet_position.y + PLAYER_HEIGHT,
            feet_position.z + PLAYER_HALF_WIDTH
        }
    };
}

FVec3 EyePosition(const MoveState &state)
{
    return state.position + FVec3{0.0f, PLAYER_EYE_HEIGHT, 0.0f};
}

void StepMovement(
    MoveState &state, const MoveInput &input, const Float delta_seconds, const Arena &arena,
    MoveEvents &events
)
{
    state.dodgeCooldown = std::max(0.0f, state.dodgeCooldown - delta_seconds);
    state.padCooldown = std::max(0.0f, state.padCooldown - delta_seconds);

    FVec3 wish = ForwardXZ(input.yaw) * input.forward + RightXZ(input.yaw) * input.right;
    Float wish_length = LengthXZ(wish);
    if (wish_length > 1.0f)
    {
        wish = wish * (1.0f / wish_length);
        wish_length = 1.0f;
    }
    const FVec3 wish_direction = wish_length > 0.0f ? wish * (1.0f / wish_length) : FVec3{0, 0, 0};
    const Float wish_speed = wish_length * MAX_GROUND_SPEED;

    const Bool was_on_ground = state.onGround;
    Bool left_ground_on_purpose = false;

    if (state.onGround)
    {
        state.wallRunning = false;
        state.lastWallNormal = FVec3{0.0f, 0.0f, 0.0f};
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
        UpdateWallRun(state, input, wish_direction, delta_seconds, arena, events);
        if (state.wallRunning && input.jump)
        {
            const FVec3 normal = state.wallNormal;
            state.velocity.x += normal.x * WALL_JUMP_PUSH;
            state.velocity.z += normal.z * WALL_JUMP_PUSH;
            state.velocity.y = WALL_JUMP_UP;
            state.airJumps = 1;
            EndWallRun(state);
            events.wallJumped = true;
        }
        else if (input.jump && state.airJumps > 0 && state.velocity.y < 6.0f)
        {
            state.velocity.y = DOUBLE_JUMP_SPEED;
            state.airJumps = 0;
            events.doubleJumped = true;
        }

        if (state.wallRunning)
        {
            const FVec3 normal = state.wallNormal;
            const Float outward = math::GetDotProduct(state.velocity, normal);
            state.velocity.x -= normal.x * (outward + WALL_RUN_STICK);
            state.velocity.z -= normal.z * (outward + WALL_RUN_STICK);
            Accelerate(state.velocity, WallTangent(normal, state.velocity), WALL_RUN_SPEED,
                       GROUND_ACCELERATION, delta_seconds);
        }
        else
        {
            Accelerate(state.velocity, wish_direction, wish_speed, AIR_ACCELERATION, delta_seconds);
        }
    }

    if (state.wallRunning)
    {
        // Gravity barely pulls at first, then catches up as the run nears its end.
        const Float progress = state.wallRunTime / WALL_RUN_MAX_TIME;
        state.velocity.y -= GRAVITY * (0.15f + 0.85f * progress * progress) * delta_seconds;
        state.velocity.y = std::max(state.velocity.y, -WALL_RUN_MAX_SINK);
    }
    else
    {
        state.velocity.y -= GRAVITY * delta_seconds;
    }
    const Float falling_speed = -state.velocity.y;

    const FVec3 motion = state.velocity * delta_seconds;
    const int substeps =
        std::max(1, static_cast<int>(std::ceil(math::GetLength(motion) / MAX_SUBSTEP)));
    const FVec3 step = motion * (1.0f / static_cast<Float>(substeps));
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
        const FVec3 probe = state.position + FVec3{0.0f, 0.05f, 0.0f};
        for (const JumpPad &pad : arena.jumpPads)
        {
            if (primitive::Contains(pad.trigger, probe))
            {
                state.velocity = pad.launchVelocity;
                state.onGround = false;
                state.airJumps = 1;
                state.padCooldown = 0.6f;
                EndWallRun(state);
                events.launched = true;
                break;
            }
        }
    }
}
} // namespace evo
