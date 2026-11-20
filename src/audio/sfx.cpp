#include "audio/sfx.h"

#include "audio/audio_manager.h"
#include "miniaudio/audio/audio_backend.h"
#include "utils/logger.h"

#include "compiled/warning_off.h"
#include <miniaudio/miniaudio.h>
#include <string>
#include "compiled/warning_on.h"

namespace evo
{
namespace
{
struct SoundInfo
{
    const Char *file;
    int voices;
};

const SoundInfo SOUND_INFO[static_cast<size_t>(Sound::COUNT)] = {
    {"rifle", 10},        {"nova_fire", 4},     {"explosion", 6},    {"rail", 4},
    {"jump", 4},          {"land", 4},          {"pad", 3},          {"dodge", 3},
    {"pickup_health", 2}, {"pickup_armor", 2},  {"pickup_weapon", 2}, {"pickup_ammo", 2},
    {"hit", 4},           {"hurt", 3},          {"death", 4},        {"frag", 2},
    {"spawn", 3},         {"match_end", 1},     {"empty", 2},        {"switch", 2},
};
} // namespace

Sfx::~Sfx()
{
    for (Pool &pool : pools)
    {
        for (ma_sound *voice : pool.voices)
        {
            ma_sound_uninit(voice);
            delete voice;
        }
        pool.voices.clear();
    }
}

Bool Sfx::Initialize()
{
    auto *backend = new miniaudio::audio::AudioBackend();
    auto *manager = audio::AudioManager::CreateInstance();
    if (!manager->Initialize(backend))
    {
        LOGWARNING("Audio unavailable, continuing without sound");
        return false;
    }
    engine = &backend->GetAudioEngine();
    for (size_t index = 0; index < pools.size(); ++index)
    {
        const std::string path = std::string(EVO_RESOURCE_DIR) + "/sounds/" + SOUND_INFO[index].file + ".wav";
        for (int v = 0; v < SOUND_INFO[index].voices; ++v)
        {
            auto *voice = new ma_sound();
            const ma_uint32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION;
            if (ma_sound_init_from_file(engine, path.c_str(), flags, nullptr, nullptr, voice) != MA_SUCCESS)
            {
                LOGWARNING("Failed to load sound %s", path.c_str());
                delete voice;
                break;
            }
            pools[index].voices.push_back(voice);
        }
    }
    return true;
}

void Sfx::Play(const Sound sound, const Float volume, const Float pan)
{
    if (engine == nullptr)
    {
        return;
    }
    Pool &pool = pools[static_cast<size_t>(sound)];
    if (pool.voices.empty() || volume <= 0.01f)
    {
        return;
    }
    ma_sound *voice = pool.voices[pool.next];
    pool.next = (pool.next + 1) % pool.voices.size();
    ma_sound_stop(voice);
    ma_sound_seek_to_pcm_frame(voice, 0);
    ma_sound_set_volume(voice, volume);
    ma_sound_set_pan(voice, Clamp(pan, -1.0f, 1.0f));
    ma_sound_set_pitch(voice, random.Range(0.95f, 1.05f));
    ma_sound_start(voice);
}

void Sfx::SetListener(const FVec3 &position, const Float yaw)
{
    listenerPosition = position;
    listenerYaw = yaw;
}

void Sfx::PlayAt(const Sound sound, const FVec3 &position, const Float volume)
{
    const FVec3 delta = Sub(position, listenerPosition);
    const Float distance = Length(delta);
    const Float attenuation = 1.0f / (1.0f + distance * 0.08f);
    const Float pan = distance > 0.5f ? Dot(Scale(delta, 1.0f / distance), RightXZ(listenerYaw)) * 0.75f : 0.0f;
    Play(sound, volume * attenuation, pan);
}

void Sfx::SetMasterVolume(const Float volume)
{
    if (engine != nullptr)
    {
        ma_engine_set_volume(engine, Clamp(volume, 0.0f, 1.0f));
    }
}
} // namespace evo
