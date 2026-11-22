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
    void AddBeam(const FVec3 &start, const FVec3 &end, const FVec3 &color, Float width, Float duration);
    void AddExplosion(const FVec3 &position);
    void AddSparks(const FVec3 &position, const FVec3 &normal, const FVec3 &color, int count);
    void AddGibs(const FVec3 &position, const FVec3 &velocity, const FVec3 &color);
    void AddMuzzleFlash(const FVec3 &position, const FVec3 &color);
    void AddSpawnColumn(const FVec3 &position, const FVec3 &color);
    void AddPickupBurst(const FVec3 &position, const FVec3 &color);
    void Update(Float delta_seconds, const Arena &arena);
    void Draw(SceneRenderer &renderer) const;

  private:
    struct Particle
    {
        FVec3 position;
        FVec3 velocity;
        FVec3 color;
        Float size = 0.1f;
        Float life = 1.0f;
        Float maxLife = 1.0f;
        Float gravity = 0.0f;
        Float emissive = 1.0f;
        Float spin = 0.0f;
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
    };

    void AddParticle(const Particle &particle);

    std::vector<Particle> particles;
    std::vector<Beam> beams;
    std::vector<Blast> blasts;
    math::Random random;
};
} // namespace evo
