#include "render/effects.h"

namespace evo
{
namespace
{
constexpr size_t MAX_PARTICLES = 450;
constexpr size_t MAX_BEAMS = 64;
constexpr size_t MAX_BLASTS = 32;

Bool PointInsideArena(const Arena &arena, const FVec3 &point)
{
    for (const ArenaBox &box : arena.boxes)
    {
        if (Contains(box.bounds, point))
        {
            return true;
        }
    }
    return false;
}
} // namespace

void Effects::Clear()
{
    particles.clear();
    beams.clear();
    blasts.clear();
}

void Effects::AddParticle(const Particle &particle)
{
    if (particles.size() >= MAX_PARTICLES)
    {
        particles.erase(particles.begin());
    }
    particles.push_back(particle);
}

void Effects::AddBeam(
    const FVec3 &start, const FVec3 &end, const FVec3 &color, const Float width, const Float duration
)
{
    if (beams.size() >= MAX_BEAMS)
    {
        beams.erase(beams.begin());
    }
    beams.push_back(Beam{start, end, color, width, duration, duration});
}

void Effects::AddExplosion(const FVec3 &position)
{
    if (blasts.size() >= MAX_BLASTS)
    {
        blasts.erase(blasts.begin());
    }
    blasts.push_back(Blast{position, {1.0f, 0.55f, 0.15f}, 4.2f, 0.45f, 0.45f});
    blasts.push_back(Blast{position, {1.0f, 0.95f, 0.7f}, 2.2f, 0.2f, 0.2f});
    for (int i = 0; i < 26; ++i)
    {
        Particle particle;
        particle.position = position;
        particle.velocity = Scale(
            Normalize(FVec3{random.Range(-1, 1), random.Range(-0.3f, 1.2f), random.Range(-1, 1)}),
            random.Range(6.0f, 16.0f)
        );
        particle.color = random.Unit() < 0.5f ? FVec3{1.0f, 0.75f, 0.2f} : FVec3{1.0f, 0.4f, 0.1f};
        particle.size = random.Range(0.08f, 0.18f);
        particle.life = particle.maxLife = random.Range(0.4f, 0.9f);
        particle.gravity = 14.0f;
        particle.spin = random.Range(-10.0f, 10.0f);
        AddParticle(particle);
    }
    for (int i = 0; i < 8; ++i)
    {
        Particle particle;
        particle.position = position;
        particle.velocity = FVec3{random.Range(-4, 4), random.Range(2, 8), random.Range(-4, 4)};
        particle.color = {0.2f, 0.18f, 0.17f};
        particle.size = random.Range(0.15f, 0.3f);
        particle.life = particle.maxLife = random.Range(1.0f, 1.8f);
        particle.gravity = 18.0f;
        particle.emissive = 0.0f;
        particle.spin = random.Range(-6.0f, 6.0f);
        AddParticle(particle);
    }
}

void Effects::AddSparks(const FVec3 &position, const FVec3 &normal, const FVec3 &color, const int count)
{
    for (int i = 0; i < count; ++i)
    {
        Particle particle;
        particle.position = Add(position, Scale(normal, 0.05f));
        const FVec3 random_direction{random.Range(-1, 1), random.Range(-1, 1), random.Range(-1, 1)};
        particle.velocity = Scale(Normalize(Add(Scale(normal, 1.2f), random_direction)), random.Range(2.0f, 7.0f));
        particle.color = color;
        particle.size = random.Range(0.04f, 0.09f);
        particle.life = particle.maxLife = random.Range(0.2f, 0.45f);
        particle.gravity = 12.0f;
        AddParticle(particle);
    }
}

void Effects::AddGibs(const FVec3 &position, const FVec3 &velocity, const FVec3 &color)
{
    for (int i = 0; i < 16; ++i)
    {
        Particle particle;
        particle.position = Add(position, FVec3{random.Range(-0.3f, 0.3f), random.Range(0.3f, 1.6f), random.Range(-0.3f, 0.3f)});
        particle.velocity = Add(Scale(velocity, 0.5f),
                                FVec3{random.Range(-5, 5), random.Range(3, 9), random.Range(-5, 5)});
        const Bool armor_piece = i % 3 != 0;
        particle.color = armor_piece ? color : FVec3{0.55f, 0.05f, 0.05f};
        particle.size = armor_piece ? random.Range(0.18f, 0.32f) : random.Range(0.1f, 0.18f);
        particle.life = particle.maxLife = random.Range(1.6f, 2.6f);
        particle.gravity = 20.0f;
        particle.emissive = armor_piece ? 0.15f : 0.0f;
        particle.spin = random.Range(-8.0f, 8.0f);
        AddParticle(particle);
    }
    blasts.push_back(Blast{Add(position, FVec3{0, 1, 0}), color, 1.6f, 0.25f, 0.25f});
}

void Effects::AddMuzzleFlash(const FVec3 &position, const FVec3 &color)
{
    if (blasts.size() >= MAX_BLASTS)
    {
        blasts.erase(blasts.begin());
    }
    blasts.push_back(Blast{position, color, 0.35f, 0.06f, 0.06f});
}

void Effects::AddSpawnColumn(const FVec3 &position, const FVec3 &color)
{
    for (int i = 0; i < 24; ++i)
    {
        Particle particle;
        const Float angle = random.Range(0.0f, 2.0f * PI_F);
        particle.position = Add(position, FVec3{std::cos(angle) * 0.6f, random.Range(0.0f, 0.4f), std::sin(angle) * 0.6f});
        particle.velocity = FVec3{0.0f, random.Range(2.0f, 5.0f), 0.0f};
        particle.color = color;
        particle.size = random.Range(0.05f, 0.12f);
        particle.life = particle.maxLife = random.Range(0.5f, 0.9f);
        particle.gravity = 0.0f;
        AddParticle(particle);
    }
}

void Effects::AddPickupBurst(const FVec3 &position, const FVec3 &color)
{
    for (int i = 0; i < 12; ++i)
    {
        Particle particle;
        particle.position = Add(position, FVec3{0.0f, 0.6f, 0.0f});
        particle.velocity = FVec3{random.Range(-2, 2), random.Range(1, 4), random.Range(-2, 2)};
        particle.color = color;
        particle.size = random.Range(0.05f, 0.1f);
        particle.life = particle.maxLife = random.Range(0.3f, 0.6f);
        particle.gravity = 4.0f;
        AddParticle(particle);
    }
}

void Effects::Update(const Float delta_seconds, const Arena &arena)
{
    for (Particle &particle : particles)
    {
        particle.life -= delta_seconds;
        particle.velocity.y -= particle.gravity * delta_seconds;
        const FVec3 next = Add(particle.position, Scale(particle.velocity, delta_seconds));
        if (particle.gravity > 0.0f && PointInsideArena(arena, next))
        {
            particle.velocity = FVec3{particle.velocity.x * 0.5f, -particle.velocity.y * 0.35f,
                                      particle.velocity.z * 0.5f};
            particle.spin *= 0.5f;
            continue;
        }
        particle.position = next;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(),
                                   [](const Particle &p) { return p.life <= 0.0f; }),
                    particles.end());
    for (Beam &beam : beams)
    {
        beam.life -= delta_seconds;
    }
    beams.erase(std::remove_if(beams.begin(), beams.end(), [](const Beam &b) { return b.life <= 0.0f; }),
                beams.end());
    for (Blast &blast : blasts)
    {
        blast.life -= delta_seconds;
    }
    blasts.erase(std::remove_if(blasts.begin(), blasts.end(), [](const Blast &b) { return b.life <= 0.0f; }),
                 blasts.end());
}

void Effects::Draw(SceneRenderer &renderer) const
{
    for (const Particle &particle : particles)
    {
        const Float t = Clamp(particle.life / particle.maxLife, 0.0f, 1.0f);
        const Float size = particle.size * (0.4f + 0.6f * t);
        const Float age = particle.maxLife - particle.life;
        const FVec3 color = particle.emissive > 0.5f ? Scale(particle.color, 0.35f + 0.65f * t) : particle.color;
        renderer.DrawShape(MeshShape::CUBE, particle.position, {size, size, size}, color, particle.emissive,
                           particle.spin * age, particle.spin * age * 0.7f);
    }
    for (const Beam &beam : beams)
    {
        const Float t = Clamp(beam.life / beam.maxLife, 0.0f, 1.0f);
        renderer.DrawBeam(beam.start, beam.end, beam.width * (0.25f + 0.75f * t),
                          Scale(beam.color, 0.4f + 0.6f * t), 1.0f);
    }
    for (const Blast &blast : blasts)
    {
        const Float t = Clamp(blast.life / blast.maxLife, 0.0f, 1.0f);
        const Float radius = blast.radius * (0.35f + 0.65f * std::sqrt(1.0f - t));
        renderer.DrawShape(MeshShape::SPHERE, blast.position, {radius, radius, radius},
                           Scale(blast.color, 0.3f + 0.7f * t), 1.0f);
    }
}
} // namespace evo
