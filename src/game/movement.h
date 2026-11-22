#pragma once
#include "game/arena.h"
#include "game/math_util.h"

namespace evo
{
constexpr Float PLAYER_HALF_WIDTH = 0.4f;
constexpr Float PLAYER_HEIGHT = 1.8f;
constexpr Float PLAYER_EYE_HEIGHT = 1.6f;
constexpr Float PLAYER_HEAD_LINE = 1.42f;

enum class DodgeDirection : UInt8
{
    NONE,
    FORWARD,
    BACK,
    LEFT,
    RIGHT
};

struct MoveState
{
    FVec3 position;
    FVec3 velocity;
    Bool onGround = false;
    UInt8 airJumps = 0;
    Float dodgeCooldown = 0.0f;
    Float padCooldown = 0.0f;
};

struct MoveInput
{
    Float forward = 0.0f;
    Float right = 0.0f;
    Float yaw = 0.0f;
    Bool jump = false;
    DodgeDirection dodge = DodgeDirection::NONE;
};

struct MoveEvents
{
    Bool jumped = false;
    Bool doubleJumped = false;
    Bool dodged = false;
    Bool launched = false;
    Float landingSpeed = 0.0f;
};

primitive::AxisAlignedBox PlayerBounds(const FVec3 &feet_position);

FVec3 EyePosition(const MoveState &state);

void StepMovement(
    MoveState &state, const MoveInput &input, Float delta_seconds, const Arena &arena,
    MoveEvents &events
);
} // namespace evo
