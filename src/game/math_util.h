#pragma once
#include "math/evogine_math.h"
#include "math/random.h"
#include "primitive/axis_aligned_box.h"
#include "primitive/fvec3.h"
#include "platform/types.h"

#include "compiled/warning_off.h"
#include <cmath>
#include "compiled/warning_on.h"

namespace evo
{
inline Float LengthXZ(const FVec3 &a)
{
    return std::sqrt(a.x * a.x + a.z * a.z);
}

inline FVec3 ForwardXZ(const Float yaw)
{
    return FVec3{std::sin(yaw), 0.0f, std::cos(yaw)};
}

inline FVec3 RightXZ(const Float yaw)
{
    return FVec3{std::cos(yaw), 0.0f, -std::sin(yaw)};
}
} // namespace evo
