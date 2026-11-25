#pragma once
#include "audio/audio_backend.h"
#include "game/math_util.h"

#include "compiled/warning_off.h"
#include <array>
#include "compiled/warning_on.h"

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
    SCATTER,
    FRAG_FIRE,
    RIPPER,
    BOUNCE,
    THROW,
    MINE_ARM,
    RICOCHET,
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
    [[nodiscard]]
    Bool AllSoundsLoaded() const;

  private:
    std::array<audio::SoundPoolId, static_cast<size_t>(Sound::COUNT)> pools{};
    Bool ownsAudioManager = false;
    math::Random random;
};
} // namespace evo
