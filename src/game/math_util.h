#pragma once
#include "primitive/fvec3.h"
#include "primitive/quat.h"
#include "platform/types.h"

#include "compiled/warning_off.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "compiled/warning_on.h"

namespace evo
{
constexpr Float PI_F = 3.14159265358979f;

inline FVec3 Add(const FVec3 &a, const FVec3 &b)
{
    return FVec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

inline FVec3 Sub(const FVec3 &a, const FVec3 &b)
{
    return FVec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

inline FVec3 Scale(const FVec3 &a, const Float s)
{
    return FVec3{a.x * s, a.y * s, a.z * s};
}

inline FVec3 Mul(const FVec3 &a, const FVec3 &b)
{
    return FVec3{a.x * b.x, a.y * b.y, a.z * b.z};
}

inline Float Dot(const FVec3 &a, const FVec3 &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline FVec3 Cross(const FVec3 &a, const FVec3 &b)
{
    return FVec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline Float Length(const FVec3 &a)
{
    return std::sqrt(Dot(a, a));
}

inline Float LengthXZ(const FVec3 &a)
{
    return std::sqrt(a.x * a.x + a.z * a.z);
}

inline FVec3 Normalize(const FVec3 &a)
{
    const Float length = Length(a);
    if (length < 1e-6f)
    {
        return FVec3{0.0f, 0.0f, 0.0f};
    }
    return Scale(a, 1.0f / length);
}

inline FVec3 Lerp(const FVec3 &a, const FVec3 &b, const Float t)
{
    return Add(a, Scale(Sub(b, a), t));
}

inline Float Clamp(const Float v, const Float lo, const Float hi)
{
    return std::max(lo, std::min(hi, v));
}

inline Float WrapAngle(Float a)
{
    while (a > PI_F)
    {
        a -= 2.0f * PI_F;
    }
    while (a < -PI_F)
    {
        a += 2.0f * PI_F;
    }
    return a;
}

inline FVec3 DirectionFromAngles(const Float yaw, const Float pitch)
{
    const Float cos_pitch = std::cos(pitch);
    return FVec3{std::sin(yaw) * cos_pitch, std::sin(pitch), std::cos(yaw) * cos_pitch};
}

inline FVec3 ForwardXZ(const Float yaw)
{
    return FVec3{std::sin(yaw), 0.0f, std::cos(yaw)};
}

inline FVec3 RightXZ(const Float yaw)
{
    return FVec3{std::cos(yaw), 0.0f, -std::sin(yaw)};
}

inline Quat QuatAxisAngle(const FVec3 &axis, const Float angle)
{
    const Float s = std::sin(angle * 0.5f);
    return Quat{axis.x * s, axis.y * s, axis.z * s, std::cos(angle * 0.5f)};
}

inline Quat QuatMul(const Quat &a, const Quat &b)
{
    return Quat{
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    };
}

inline Quat QuatFromYawPitch(const Float yaw, const Float pitch)
{
    return QuatMul(
        QuatAxisAngle(FVec3{0.0f, 1.0f, 0.0f}, yaw), QuatAxisAngle(FVec3{1.0f, 0.0f, 0.0f}, -pitch)
    );
}

inline void AnglesFromDirection(const FVec3 &direction, Float &yaw, Float &pitch)
{
    const FVec3 d = Normalize(direction);
    yaw = std::atan2(d.x, d.z);
    pitch = std::asin(Clamp(d.y, -1.0f, 1.0f));
}

struct Aabb
{
    FVec3 min;
    FVec3 max;
};

inline Bool Overlaps(const Aabb &a, const Aabb &b)
{
    return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y && a.max.y > b.min.y &&
           a.min.z < b.max.z && a.max.z > b.min.z;
}

inline Bool Contains(const Aabb &a, const FVec3 &p)
{
    return p.x >= a.min.x && p.x <= a.max.x && p.y >= a.min.y && p.y <= a.max.y &&
           p.z >= a.min.z && p.z <= a.max.z;
}

inline Aabb Expand(const Aabb &a, const FVec3 &e)
{
    return Aabb{Sub(a.min, e), Add(a.max, e)};
}

inline FVec3 Center(const Aabb &a)
{
    return Scale(Add(a.min, a.max), 0.5f);
}

inline Bool RayAabb(
    const FVec3 &origin, const FVec3 &direction, const Aabb &box, Float max_distance,
    Float &hit_distance, FVec3 &hit_normal
)
{
    Float t_min = 0.0f;
    Float t_max = max_distance;
    FVec3 normal{0.0f, 0.0f, 0.0f};
    const Float o[3] = {origin.x, origin.y, origin.z};
    const Float d[3] = {direction.x, direction.y, direction.z};
    const Float lo[3] = {box.min.x, box.min.y, box.min.z};
    const Float hi[3] = {box.max.x, box.max.y, box.max.z};
    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::fabs(d[axis]) < 1e-8f)
        {
            if (o[axis] < lo[axis] || o[axis] > hi[axis])
            {
                return false;
            }
            continue;
        }
        const Float inverse = 1.0f / d[axis];
        Float t0 = (lo[axis] - o[axis]) * inverse;
        Float t1 = (hi[axis] - o[axis]) * inverse;
        Float sign = -1.0f;
        if (t0 > t1)
        {
            std::swap(t0, t1);
            sign = 1.0f;
        }
        if (t0 > t_min)
        {
            t_min = t0;
            normal = FVec3{0.0f, 0.0f, 0.0f};
            if (axis == 0)
            {
                normal.x = sign;
            }
            else if (axis == 1)
            {
                normal.y = sign;
            }
            else
            {
                normal.z = sign;
            }
        }
        t_max = std::min(t_max, t1);
        if (t_min > t_max)
        {
            return false;
        }
    }
    hit_distance = t_min;
    hit_normal = normal;
    return true;
}

inline UInt32 HashU32(UInt32 x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

struct Random
{
    UInt32 state = 0x12345678U;

    UInt32 Next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    Float Unit()
    {
        return static_cast<Float>(Next() & 0xFFFFFF) / static_cast<Float>(0x1000000);
    }

    Float Range(const Float lo, const Float hi)
    {
        return lo + (hi - lo) * Unit();
    }

    int Int(const int lo, const int hi_inclusive)
    {
        return lo + static_cast<int>(Next() % static_cast<UInt32>(hi_inclusive - lo + 1));
    }
};
} // namespace evo
