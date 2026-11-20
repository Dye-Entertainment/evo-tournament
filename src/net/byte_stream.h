#pragma once
#include "game/math_util.h"

#include "compiled/warning_off.h"
#include <cstring>
#include <string>
#include <vector>
#include "compiled/warning_on.h"

namespace evo::net
{
class ByteWriter final
{
  public:
    void U8(const UInt8 value)
    {
        data.push_back(value);
    }

    void U16(const UInt16 value)
    {
        U8(static_cast<UInt8>(value & 0xFF));
        U8(static_cast<UInt8>(value >> 8));
    }

    void I16(const Int16 value)
    {
        U16(static_cast<UInt16>(value));
    }

    void U32(const UInt32 value)
    {
        U16(static_cast<UInt16>(value & 0xFFFF));
        U16(static_cast<UInt16>(value >> 16));
    }

    void F32(const Float value)
    {
        UInt32 bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        U32(bits);
    }

    void Vec3(const FVec3 &value)
    {
        F32(value.x);
        F32(value.y);
        F32(value.z);
    }

    void Angle(const Float radians)
    {
        I16(static_cast<Int16>(Clamp(WrapAngle(radians) / PI_F, -1.0f, 1.0f) * 32767.0f));
    }

    void String(const std::string &value)
    {
        const size_t length = std::min<size_t>(value.size(), 255);
        U8(static_cast<UInt8>(length));
        data.insert(data.end(), value.begin(), value.begin() + static_cast<std::ptrdiff_t>(length));
    }

    std::vector<UInt8> data;
};

class ByteReader final
{
  public:
    ByteReader(const UInt8 *bytes, const size_t length) : bytes(bytes), length(length)
    {
    }

    UInt8 U8()
    {
        if (offset + 1 > length)
        {
            failed = true;
            return 0;
        }
        return bytes[offset++];
    }

    UInt16 U16()
    {
        const UInt16 low = U8();
        const UInt16 high = U8();
        return static_cast<UInt16>(low | (high << 8));
    }

    Int16 I16()
    {
        return static_cast<Int16>(U16());
    }

    UInt32 U32()
    {
        const UInt32 low = U16();
        const UInt32 high = U16();
        return low | (high << 16);
    }

    Float F32()
    {
        const UInt32 bits = U32();
        Float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        return std::isfinite(value) ? value : 0.0f;
    }

    FVec3 Vec3()
    {
        const Float x = F32();
        const Float y = F32();
        const Float z = F32();
        return FVec3{x, y, z};
    }

    Float Angle()
    {
        return static_cast<Float>(I16()) / 32767.0f * PI_F;
    }

    std::string String()
    {
        const UInt8 size = U8();
        if (offset + size > length)
        {
            failed = true;
            return {};
        }
        std::string value(reinterpret_cast<const Char *>(bytes + offset), size);
        offset += size;
        return value;
    }

    [[nodiscard]]
    Bool Failed() const
    {
        return failed;
    }

  private:
    const UInt8 *bytes = nullptr;
    size_t length = 0;
    size_t offset = 0;
    Bool failed = false;
};
} // namespace evo::net
