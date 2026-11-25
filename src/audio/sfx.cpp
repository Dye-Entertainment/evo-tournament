#include "audio/sfx.h"

#include "audio/audio_manager.h"
#include "miniaudio/audio/audio_backend.h"
#include "resource/asset_path.h"
#include "utils/logger.h"

#include "compiled/warning_off.h"
#include <algorithm>
#include <string>
#include "compiled/warning_on.h"

namespace evo
{
namespace
{
struct SoundInfo
{
    const Char *file;
    UInt32 voices;
};

const SoundInfo SOUND_INFO[static_cast<size_t>(Sound::COUNT)] = {
    {"rifle", 10},        {"nova_fire", 4},     {"explosion", 6},    {"rail", 4},
    {"jump", 4},          {"land", 4},          {"pad", 3},          {"dodge", 3},
    {"pickup_health", 2}, {"pickup_armor", 2},  {"pickup_weapon", 2}, {"pickup_ammo", 2},
    {"hit", 4},           {"hurt", 3},          {"death", 4},        {"frag", 2},
    {"spawn", 3},         {"match_end", 1},     {"empty", 2},        {"switch", 2},
    {"scatter", 4},       {"frag_fire", 4},     {"ripper", 6},       {"bounce", 6},
    {"throw", 3},         {"mine_arm", 4},      {"ricochet", 6},
};

audio::AudioManager *GetAudio()
{
    return audio::AudioManager::GetInstance();
}
} // namespace

Sfx::~Sfx()
{
    if (ownsAudioManager && GetAudio() != nullptr)
    {
        GetAudio()->Destroy();
    }
}

Bool Sfx::Initialize()
{
    pools.fill(audio::INVALID_SOUND_POOL);
    auto *manager = audio::AudioManager::CreateInstance();
    ownsAudioManager = true;
    if (!manager->Initialize(new miniaudio::audio::AudioBackend()))
    {
        LOGWARNING("Audio unavailable, continuing without sound");
        manager->Destroy();
        return false;
    }
    for (size_t index = 0; index < pools.size(); ++index)
    {
        const std::string path = std::string("game://sounds/") + SOUND_INFO[index].file + ".wav";
        pools[index] =
            manager->CreateSoundPool(resource::AssetPath(path), SOUND_INFO[index].voices);
    }
    return true;
}

void Sfx::Play(const Sound sound, const Float volume, const Float pan)
{
    if (GetAudio() == nullptr)
    {
        return;
    }
    GetAudio()->PlaySound(
        pools[static_cast<size_t>(sound)],
        audio::SoundParameters{volume, pan, random.NextRange(0.95f, 1.05f)}
    );
}

void Sfx::PlayAt(const Sound sound, const FVec3 &position, const Float volume)
{
    if (GetAudio() == nullptr)
    {
        return;
    }
    GetAudio()->PlaySoundAt(
        pools[static_cast<size_t>(sound)], position,
        audio::SoundParameters{volume, 0.0f, random.NextRange(0.95f, 1.05f)}
    );
}

void Sfx::SetListener(const FVec3 &position, const Float yaw)
{
    if (GetAudio() != nullptr)
    {
        GetAudio()->SetListener(position, yaw);
    }
}

void Sfx::SetMasterVolume(const Float volume)
{
    if (GetAudio() != nullptr)
    {
        GetAudio()->SetMasterVolume(volume);
    }
}

Bool Sfx::AllSoundsLoaded() const
{
    return std::none_of(
        pools.begin(), pools.end(),
        [](const audio::SoundPoolId pool) { return pool == audio::INVALID_SOUND_POOL; }
    );
}
} // namespace evo
