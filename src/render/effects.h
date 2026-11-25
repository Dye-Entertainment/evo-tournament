#pragma once
#include "game/arena.h"
#include "render/scene_renderer.h"

#include "compiled/warning_off.h"
#include <vector>
#include "compiled/warning_on.h"

namespace evo
{
class Effects final
{
  public:
    void Clear();
    // With gore off, hits throw sparks and deaths break into armor pieces only.
    void SetGore(const Bool enabled)
    {
        gore = enabled;
    }
    void AddBeam(const FVec3 &start, const FVec3 &end, const FVec3 &color, Float width, Float duration);
    void AddExplosion(const FVec3 &position);
    void AddSparks(const FVec3 &position, const FVec3 &normal, const FVec3 &color, int count);
    // A bullet hit on a player: blood sprays away from `from`.
    void AddBloodHit(const FVec3 &position, const FVec3 &from, int damage);
    // A player dies: chunks, limbs and a lot of blood (or armor pieces with gore off).
    void AddGibs(const FVec3 &position, const FVec3 &velocity, const FVec3 &color, Bool headshot,
                 Bool explosive);
    void AddMuzzleFlash(const FVec3 &position, const FVec3 &color);
    void AddSpawnColumn(const FVec3 &position, const FVec3 &color);
    void AddPickupBurst(const FVec3 &position, const FVec3 &color);
    // Snow, embers or dust drifting around the camera, depending on the level.
    void UpdateAmbient(Float delta_seconds, const Arena &arena, const FVec3 &camera);
    void Update(Float delta_seconds, const Arena &arena);
    void AddLights(SceneRenderer &renderer) const;
    void Draw(SceneRenderer &renderer);

  private:
    enum class Kind : UInt8
    {
        SPARK,
        DEBRIS,
        BLOOD,
        MEAT,
        LIMB,
        EMBER,
        SNOW
    };

    struct Particle
    {
        Kind kind = Kind::SPARK;
        FVec3 position;
        FVec3 velocity;
        FVec3 color;
        FVec3 scale{1.0f, 1.0f, 1.0f};
        Float size = 0.1f;
        Float life = 1.0f;
        Float maxLife = 1.0f;
        Float gravity = 0.0f;
        Float emissive = 1.0f;
        Float spin = 0.0f;
        Float trailTimer = 0.0f;
        Bool landed = false;
    };

    struct Beam
    {
        FVec3 start;
        FVec3 end;
        FVec3 color;
        Float width = 0.05f;
        Float life = 0.2f;
        Float maxLife = 0.2f;
    };

    struct Blast
    {
        FVec3 position;
        FVec3 color;
        Float radius = 1.0f;
        Float life = 0.3f;
        Float maxLife = 0.3f;
        // 0 glows, 1 is a dark blood mist puff.
        Bool mist = false;
    };

    struct Decal
    {
        FVec3 position;
        FVec3 normal;
        FVec3 color;
        Float size = 1.0f;
        Float spin = 0.0f;
        Float life = 30.0f;
        UInt8 shape = 0;
    };

    void AddParticle(const Particle &particle);
    void AddBlast(const Blast &blast);
    void AddDecal(const FVec3 &position, const FVec3 &normal, Float size);
    void AddBloodDrops(const FVec3 &position, const FVec3 &direction, int count, Float speed, Float spread);
    void AddMist(const FVec3 &position, Float radius);

    std::vector<Particle> particles;
    std::vector<Beam> beams;
    std::vector<Blast> blasts;
    std::vector<Decal> decals;
    math::Random random;
    Float ambientTimer = 0.0f;
    Bool gore = true;
};
} // namespace evo
