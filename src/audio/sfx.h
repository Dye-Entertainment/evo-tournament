#pragma once
#include "game/math_util.h"

#include "compiled/warning_off.h"
#include <array>
#include <vector>
#include "compiled/warning_on.h"

struct ma_engine;
struct ma_sound;

namespace evo
{
enum class Sound : UInt8
{
    RIFLE,
    NOVA_FIRE,
    EXPLOSION,
    RAIL,
    JUMP,
    LAND,
    PAD,
    DODGE,
    PICKUP_HEALTH,
    PICKUP_ARMOR,
    PICKUP_WEAPON,
    PICKUP_AMMO,
    HIT,
    HURT,
    DEATH,
    FRAG,
    SPAWN,
    MATCH_END,
    EMPTY,
    SWITCH,
    COUNT
};

class Sfx final
{
  public:
    ~Sfx();
    Bool Initialize();
    void Play(Sound sound, Float volume = 1.0f, Float pan = 0.0f);
    void PlayAt(Sound sound, const FVec3 &position, Float volume = 1.0f);
    void SetListener(const FVec3 &position, Float yaw);
    void SetMasterVolume(Float volume);

  private:
    struct Pool
    {
        std::vector<ma_sound *> voices;
        size_t next = 0;
    };

    ma_engine *engine = nullptr;
    std::array<Pool, static_cast<size_t>(Sound::COUNT)> pools;
    FVec3 listenerPosition;
    Float listenerYaw = 0.0f;
    Random random;
};
} // namespace evo
