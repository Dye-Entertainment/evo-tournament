#include "render/effects.h"

#include "compiled/warning_off.h"
#include <algorithm>
#include "compiled/warning_on.h"

namespace evo
{
namespace
{
constexpr size_t MAX_PARTICLES = 520;
constexpr size_t MAX_BEAMS = 64;
constexpr size_t MAX_BLASTS = 48;
constexpr size_t MAX_DECALS = 120;
constexpr Float DECAL_LIFE = 30.0f;
constexpr Float DECAL_FADE = 3.0f;
constexpr int MAX_AMBIENT = 70;

const FVec3 BLOOD{0.42f, 0.015f, 0.02f};
const FVec3 BLOOD_DARK{0.25f, 0.01f, 0.015f};
const FVec3 MEAT{0.55f, 0.10f, 0.09f};
const FVec3 BONE{0.85f, 0.80f, 0.70f};

Bool PointInsideArena(const Arena &arena, const FVec3 &point)
{
    for (const ArenaBox &box : arena.boxes)
    {
        if (box.solid && primitive::Contains(box.bounds, point))
        {
            return true;
        }
    }
    return false;
}

// The first level surface on the segment from `from` to `to`.
Bool SegmentHit(const Arena &arena, const FVec3 &from, const FVec3 &to, FVec3 &point, FVec3 &normal)
{
    const FVec3 delta = to - from;
    const Float length = math::GetLength(delta);
    if (length < 1e-5f)
    {
        return false;
    }
    RayHit hit;
    const FVec3 direction = delta * (1.0f / length);
    if (!arena.RayCast(from, direction, length, hit))
    {
        return false;
    }
    point = from + direction * hit.distance;
    normal = hit.normal;
    return true;
}

FVec3 RandomDirection(math::Random &random)
{
    return math::GetNormalized(FVec3{random.NextRange(-1, 1), random.NextRange(-1, 1), random.NextRange(-1, 1)});
}
} // namespace

void Effects::Clear()
{
    particles.clear();
    beams.clear();
    blasts.clear();
    decals.clear();
}

void Effects::AddParticle(const Particle &particle)
{
    if (particles.size() >= MAX_PARTICLES)
    {
        // Drop the oldest ambient or spark particle first so gore survives a busy fight.
        auto victim = std::find_if(particles.begin(), particles.end(), [](const Particle &p)
                                   { return p.kind == Kind::SNOW || p.kind == Kind::EMBER || p.kind == Kind::SPARK; });
        particles.erase(victim != particles.end() ? victim : particles.begin());
    }
    particles.push_back(particle);
}

void Effects::AddBlast(const Blast &blast)
{
    if (blasts.size() >= MAX_BLASTS)
    {
        blasts.erase(blasts.begin());
    }
    blasts.push_back(blast);
}

void Effects::AddDecal(const FVec3 &position, const FVec3 &normal, const Float size)
{
    if (!gore)
    {
        return;
    }
    // Merge into a nearby splat instead of stacking dozens in one spot.
    for (Decal &decal : decals)
    {
        if (math::GetDotProduct(decal.normal, normal) > 0.9f &&
            math::GetLength(decal.position - position) < decal.size * 0.25f)
        {
            decal.size = std::min(decal.size + size * 0.15f, 2.6f);
            decal.life = DECAL_LIFE;
            return;
        }
    }
    if (decals.size() >= MAX_DECALS)
    {
        decals.erase(decals.begin());
    }
    Decal decal;
    decal.position = position;
    decal.normal = normal;
    decal.size = size;
    decal.spin = random.NextRange(0.0f, 6.28f);
    decal.shape = static_cast<UInt8>(random.NextInt(0, 2));
    decal.color = math::GetLerp(BLOOD, BLOOD_DARK, random.NextUnit());
    decal.life = DECAL_LIFE;
    decals.push_back(decal);
}

void Effects::AddBloodDrops(
    const FVec3 &position, const FVec3 &direction, const int count, const Float speed, const Float spread
)
{
    for (int i = 0; i < count; ++i)
    {
        Particle particle;
        particle.kind = Kind::BLOOD;
        particle.position = position + RandomDirection(random) * 0.08f;
        particle.velocity =
            math::GetNormalized(direction + RandomDirection(random) * spread) * (speed * random.NextRange(0.4f, 1.2f));
        particle.color = math::GetLerp(BLOOD, MEAT, random.NextUnit() * 0.4f);
        particle.size = random.NextRange(0.04f, 0.11f);
        particle.life = particle.maxLife = random.NextRange(0.8f, 1.6f);
        particle.gravity = 20.0f;
        particle.emissive = 0.0f;
        AddParticle(particle);
    }
}

void Effects::AddMist(const FVec3 &position, const Float radius)
{
    Blast mist;
    mist.position = position;
    mist.color = {0.35f, 0.01f, 0.01f};
    mist.radius = radius;
    mist.life = mist.maxLife = 0.45f;
    mist.mist = true;
    AddBlast(mist);
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
    AddBlast(Blast{position, {1.0f, 0.55f, 0.15f}, 4.2f, 0.45f, 0.45f, false});
    AddBlast(Blast{position, {1.0f, 0.95f, 0.7f}, 2.2f, 0.2f, 0.2f, false});
    for (int i = 0; i < 26; ++i)
    {
        Particle particle;
        particle.position = position;
        particle.velocity =
            math::GetNormalized(
                FVec3{random.NextRange(-1, 1), random.NextRange(-0.3f, 1.2f), random.NextRange(-1, 1)}
            ) *
            random.NextRange(6.0f, 16.0f);
        particle.color =
            random.NextUnit() < 0.5f ? FVec3{1.0f, 0.75f, 0.2f} : FVec3{1.0f, 0.4f, 0.1f};
        particle.size = random.NextRange(0.08f, 0.18f);
        particle.life = particle.maxLife = random.NextRange(0.4f, 0.9f);
        particle.gravity = 14.0f;
        particle.spin = random.NextRange(-10.0f, 10.0f);
        AddParticle(particle);
    }
    for (int i = 0; i < 8; ++i)
    {
        Particle particle;
        particle.kind = Kind::DEBRIS;
        particle.position = position;
        particle.velocity =
            FVec3{random.NextRange(-4, 4), random.NextRange(2, 8), random.NextRange(-4, 4)};
        particle.color = {0.2f, 0.18f, 0.17f};
        particle.size = random.NextRange(0.15f, 0.3f);
        particle.life = particle.maxLife = random.NextRange(1.0f, 1.8f);
        particle.gravity = 18.0f;
        particle.emissive = 0.0f;
        particle.spin = random.NextRange(-6.0f, 6.0f);
        AddParticle(particle);
    }
}

void Effects::AddSparks(const FVec3 &position, const FVec3 &normal, const FVec3 &color, const int count)
{
    for (int i = 0; i < count; ++i)
    {
        Particle particle;
        particle.position = position + normal * 0.05f;
        particle.velocity =
            math::GetNormalized(normal * 1.2f + RandomDirection(random)) * random.NextRange(2.0f, 7.0f);
        particle.color = color;
        particle.size = random.NextRange(0.04f, 0.09f);
        particle.life = particle.maxLife = random.NextRange(0.2f, 0.45f);
        particle.gravity = 12.0f;
        AddParticle(particle);
    }
}

void Effects::AddBloodHit(const FVec3 &position, const FVec3 &from, const int damage)
{
    if (!gore)
    {
        AddSparks(position, {0.0f, 1.0f, 0.0f}, {0.8f, 0.05f, 0.05f}, 5);
        return;
    }
    FVec3 direction = position - from;
    if (math::GetLength(direction) < 0.01f)
    {
        direction = {0.0f, 1.0f, 0.0f};
    }
    direction = math::GetNormalized(direction);
    const int count = std::clamp(damage / 3, 5, 26);
    // The exit spray, a little back-spatter toward the shooter, and a puff of mist.
    AddBloodDrops(position, direction + FVec3{0.0f, 0.25f, 0.0f}, count, 7.0f, 0.45f);
    AddBloodDrops(position, direction * -1.0f + FVec3{0.0f, 0.4f, 0.0f}, count / 3, 3.5f, 0.6f);
    AddMist(position + direction * 0.25f, 0.5f + static_cast<Float>(damage) * 0.008f);
}

void Effects::AddGibs(
    const FVec3 &position, const FVec3 &velocity, const FVec3 &color, const Bool headshot, const Bool explosive
)
{
    const FVec3 inherited = velocity * 0.5f;
    const Float force = explosive ? 1.6f : 1.0f;

    // Armor plates in the player's color.
    for (int i = 0; i < (gore ? 7 : 14); ++i)
    {
        Particle particle;
        particle.kind = Kind::DEBRIS;
        particle.position = position + FVec3{random.NextRange(-0.3f, 0.3f), random.NextRange(0.6f, 1.6f),
                                             random.NextRange(-0.3f, 0.3f)};
        particle.velocity = inherited + FVec3{random.NextRange(-5, 5), random.NextRange(3, 9), random.NextRange(-5, 5)} * force;
        particle.color = color * random.NextRange(0.6f, 1.0f);
        particle.size = random.NextRange(0.16f, 0.3f);
        particle.scale = {1.0f, 0.35f, 1.3f};
        particle.life = particle.maxLife = random.NextRange(3.0f, 4.5f);
        particle.gravity = 20.0f;
        particle.emissive = 0.15f;
        particle.spin = random.NextRange(-8.0f, 8.0f);
        AddParticle(particle);
    }
    AddBlast(Blast{position + FVec3{0, 1, 0}, color, 1.4f, 0.22f, 0.22f, false});
    if (!gore)
    {
        return;
    }

    // Meat chunks that tumble, bleed as they fly, and splat where they land.
    const int chunks = explosive ? 12 : 8;
    for (int i = 0; i < chunks; ++i)
    {
        Particle particle;
        particle.kind = Kind::MEAT;
        particle.position = position + FVec3{random.NextRange(-0.3f, 0.3f), random.NextRange(0.5f, 1.5f),
                                             random.NextRange(-0.3f, 0.3f)};
        particle.velocity = inherited + FVec3{random.NextRange(-6, 6), random.NextRange(4, 10), random.NextRange(-6, 6)} * force;
        particle.color = math::GetLerp(MEAT, BLOOD, random.NextUnit() * 0.6f);
        particle.size = random.NextRange(0.16f, 0.32f);
        particle.scale = {random.NextRange(0.8f, 1.3f), random.NextRange(0.7f, 1.1f), random.NextRange(0.9f, 1.4f)};
        particle.life = particle.maxLife = random.NextRange(5.0f, 7.0f);
        particle.gravity = 20.0f;
        particle.emissive = 0.0f;
        particle.spin = random.NextRange(-9.0f, 9.0f);
        AddParticle(particle);
    }
    // Limbs and bones.
    const FVec3 limb_colors[4] = {{0.16f, 0.16f, 0.19f}, {0.16f, 0.16f, 0.19f}, color * 0.6f, color * 0.6f};
    for (int i = 0; i < 4; ++i)
    {
        Particle particle;
        particle.kind = Kind::LIMB;
        particle.position = position + FVec3{random.NextRange(-0.3f, 0.3f), i < 2 ? 0.5f : 1.2f, random.NextRange(-0.3f, 0.3f)};
        particle.velocity = inherited + FVec3{random.NextRange(-5, 5), random.NextRange(3, 8), random.NextRange(-5, 5)} * force;
        particle.color = limb_colors[i];
        particle.size = i < 2 ? 0.5f : 0.4f;
        particle.scale = {0.4f, 0.4f, 1.0f};
        particle.life = particle.maxLife = random.NextRange(6.0f, 8.0f);
        particle.gravity = 20.0f;
        particle.emissive = 0.0f;
        particle.spin = random.NextRange(-7.0f, 7.0f);
        AddParticle(particle);
    }
    for (int i = 0; i < 3; ++i)
    {
        Particle particle;
        particle.kind = Kind::DEBRIS;
        particle.position = position + FVec3{0.0f, 1.0f, 0.0f};
        particle.velocity = inherited + FVec3{random.NextRange(-4, 4), random.NextRange(4, 9), random.NextRange(-4, 4)} * force;
        particle.color = BONE;
        particle.size = 0.3f;
        particle.scale = {0.18f, 0.18f, 1.0f};
        particle.life = particle.maxLife = random.NextRange(5.0f, 7.0f);
        particle.gravity = 20.0f;
        particle.emissive = 0.0f;
        particle.spin = random.NextRange(-10.0f, 10.0f);
        AddParticle(particle);
    }

    const FVec3 head = position + FVec3{0.0f, 1.65f, 0.0f};
    if (headshot)
    {
        // The head bursts: a fountain of blood and bits, no helmet left to fly.
        AddBloodDrops(head, {0.0f, 1.0f, 0.0f}, 40, 9.0f, 0.8f);
        AddMist(head, 1.4f);
        for (int i = 0; i < 4; ++i)
        {
            Particle particle;
            particle.kind = Kind::MEAT;
            particle.position = head;
            particle.velocity = FVec3{random.NextRange(-4, 4), random.NextRange(5, 10), random.NextRange(-4, 4)};
            particle.color = i == 0 ? FVec3{0.82f, 0.83f, 0.86f} : MEAT;
            particle.size = random.NextRange(0.1f, 0.18f);
            particle.life = particle.maxLife = 5.0f;
            particle.gravity = 20.0f;
            particle.emissive = 0.0f;
            particle.spin = random.NextRange(-12.0f, 12.0f);
            AddParticle(particle);
        }
    }
    else
    {
        Particle helmet;
        helmet.kind = Kind::LIMB;
        helmet.position = head;
        helmet.velocity = inherited + FVec3{random.NextRange(-3, 3), random.NextRange(6, 10), random.NextRange(-3, 3)} * force;
        helmet.color = {0.82f, 0.83f, 0.86f};
        helmet.size = 0.34f;
        helmet.life = helmet.maxLife = 7.0f;
        helmet.gravity = 20.0f;
        helmet.emissive = 0.0f;
        helmet.spin = random.NextRange(-8.0f, 8.0f);
        AddParticle(helmet);
    }

    // The main burst, a cloud of mist, and a pool on the ground below.
    AddBloodDrops(position + FVec3{0.0f, 1.0f, 0.0f}, FVec3{0.0f, 1.0f, 0.0f} + velocity * 0.05f,
                  explosive ? 50 : 34, 8.0f * force, 1.0f);
    AddMist(position + FVec3{0.0f, 1.0f, 0.0f}, explosive ? 2.6f : 1.8f);
    AddMist(position + FVec3{0.0f, 0.5f, 0.0f}, 1.2f);
    AddBlast(Blast{position + FVec3{0, 1, 0}, {0.5f, 0.02f, 0.02f}, 1.2f, 0.3f, 0.3f, true});
}

void Effects::AddMuzzleFlash(const FVec3 &position, const FVec3 &color)
{
    AddBlast(Blast{position, color, 0.35f, 0.06f, 0.06f, false});
}

void Effects::AddSpawnColumn(const FVec3 &position, const FVec3 &color)
{
    for (int i = 0; i < 24; ++i)
    {
        Particle particle;
        const Float angle = random.NextRange(0.0f, 2.0f * math::PI);
        particle.position =
            position +
            FVec3{std::cos(angle) * 0.6f, random.NextRange(0.0f, 0.4f), std::sin(angle) * 0.6f};
        particle.velocity = FVec3{0.0f, random.NextRange(2.0f, 5.0f), 0.0f};
        particle.color = color;
        particle.size = random.NextRange(0.05f, 0.12f);
        particle.life = particle.maxLife = random.NextRange(0.5f, 0.9f);
        particle.gravity = 0.0f;
        AddParticle(particle);
    }
    AddBlast(Blast{position + FVec3{0.0f, 1.0f, 0.0f}, color, 1.6f, 0.4f, 0.4f, false});
}

void Effects::AddPickupBurst(const FVec3 &position, const FVec3 &color)
{
    for (int i = 0; i < 12; ++i)
    {
        Particle particle;
        particle.position = position + FVec3{0.0f, 0.6f, 0.0f};
        particle.velocity =
            FVec3{random.NextRange(-2, 2), random.NextRange(1, 4), random.NextRange(-2, 2)};
        particle.color = color;
        particle.size = random.NextRange(0.05f, 0.1f);
        particle.life = particle.maxLife = random.NextRange(0.3f, 0.6f);
        particle.gravity = 4.0f;
        AddParticle(particle);
    }
}

void Effects::UpdateAmbient(const Float delta_seconds, const Arena &arena, const FVec3 &camera)
{
    const Bool snow = arena.id == ArenaId::FROSTSPIRE;
    const Bool embers = arena.theme.embers > 0.0f;
    int ambient = 0;
    for (const Particle &particle : particles)
    {
        ambient += (particle.kind == Kind::SNOW || particle.kind == Kind::EMBER) ? 1 : 0;
    }
    ambientTimer += delta_seconds;
    const Float interval = snow ? 0.03f : 0.06f;
    while (ambientTimer > interval)
    {
        ambientTimer -= interval;
        if (ambient >= MAX_AMBIENT)
        {
            continue;
        }
        ++ambient;
        Particle particle;
        particle.emissive = 0.0f;
        const FVec3 offset{random.NextRange(-18, 18), 0.0f, random.NextRange(-18, 18)};
        if (snow)
        {
            particle.kind = Kind::SNOW;
            particle.position = camera + offset + FVec3{0.0f, random.NextRange(4.0f, 10.0f), 0.0f};
            particle.velocity = {random.NextRange(-0.6f, 0.6f), -random.NextRange(1.2f, 2.2f), random.NextRange(-0.6f, 0.6f)};
            particle.color = {0.9f, 0.95f, 1.0f};
            particle.size = random.NextRange(0.04f, 0.08f);
            particle.life = particle.maxLife = 7.0f;
            particle.emissive = 0.35f;
        }
        else if (embers)
        {
            particle.kind = Kind::EMBER;
            particle.position = camera + offset + FVec3{0.0f, random.NextRange(-3.0f, 2.0f), 0.0f};
            particle.position.y = std::max(particle.position.y, 0.2f);
            particle.velocity = {random.NextRange(-0.5f, 0.5f), random.NextRange(0.6f, 1.8f), random.NextRange(-0.5f, 0.5f)};
            particle.color = random.NextUnit() < 0.5f ? FVec3{1.0f, 0.55f, 0.15f} : FVec3{1.0f, 0.35f, 0.08f};
            particle.size = random.NextRange(0.03f, 0.06f);
            particle.life = particle.maxLife = random.NextRange(3.0f, 5.0f);
            particle.emissive = 1.0f;
        }
        else
        {
            // Warm dust motes in the evening light.
            particle.kind = Kind::EMBER;
            particle.position = camera + offset + FVec3{0.0f, random.NextRange(-1.0f, 4.0f), 0.0f};
            particle.velocity = {random.NextRange(-0.3f, 0.3f), random.NextRange(-0.1f, 0.25f), random.NextRange(-0.3f, 0.3f)};
            particle.color = {1.0f, 0.8f, 0.55f};
            particle.size = 0.025f;
            particle.life = particle.maxLife = random.NextRange(3.0f, 5.0f);
            particle.emissive = 0.6f;
            if (ambient > MAX_AMBIENT / 2)
            {
                continue;
            }
        }
        AddParticle(particle);
    }
}

void Effects::Update(const Float delta_seconds, const Arena &arena)
{
    std::vector<Particle> spawned;
    for (Particle &particle : particles)
    {
        particle.life -= delta_seconds;
        particle.velocity.y -= particle.gravity * delta_seconds;
        if (particle.kind == Kind::SNOW || particle.kind == Kind::EMBER)
        {
            particle.velocity.x += std::sin(particle.life * 1.7f + particle.position.z) * delta_seconds * 0.6f;
        }
        const FVec3 next = particle.position + particle.velocity * delta_seconds;
        const Bool gory = particle.kind == Kind::BLOOD || particle.kind == Kind::MEAT || particle.kind == Kind::LIMB;

        if (gory)
        {
            // Bleeding chunks leave a trail.
            if (particle.kind != Kind::BLOOD && gore && !particle.landed)
            {
                particle.trailTimer -= delta_seconds;
                if (particle.trailTimer <= 0.0f && math::GetLength(particle.velocity) > 2.0f)
                {
                    particle.trailTimer = 0.04f;
                    Particle drop;
                    drop.kind = Kind::BLOOD;
                    drop.position = particle.position;
                    drop.velocity = particle.velocity * 0.3f + RandomDirection(random) * 0.6f;
                    drop.color = BLOOD;
                    drop.size = random.NextRange(0.03f, 0.07f);
                    drop.life = drop.maxLife = 1.2f;
                    drop.gravity = 20.0f;
                    drop.emissive = 0.0f;
                    spawned.push_back(drop);
                }
            }
            FVec3 hit_point;
            FVec3 hit_normal;
            if (SegmentHit(arena, particle.position, next, hit_point, hit_normal))
            {
                if (particle.kind == Kind::BLOOD)
                {
                    AddDecal(hit_point, hit_normal, random.NextRange(0.35f, 0.8f));
                    particle.life = 0.0f;
                    continue;
                }
                if (!particle.landed || math::GetLength(particle.velocity) > 5.0f)
                {
                    AddDecal(hit_point, hit_normal, particle.kind == Kind::MEAT ? random.NextRange(0.7f, 1.3f) : 0.9f);
                }
                particle.landed = true;
                particle.position = hit_point + hit_normal * (particle.size * 0.4f);
                const FVec3 n = hit_normal;
                const Float into = math::GetDotProduct(particle.velocity, n);
                particle.velocity = (particle.velocity - n * (into * 1.3f)) * 0.45f;
                particle.spin *= 0.4f;
                continue;
            }
            particle.position = next;
            continue;
        }

        if (particle.gravity > 0.0f && PointInsideArena(arena, next))
        {
            particle.velocity = FVec3{particle.velocity.x * 0.5f, -particle.velocity.y * 0.35f,
                                      particle.velocity.z * 0.5f};
            particle.spin *= 0.5f;
            continue;
        }
        if ((particle.kind == Kind::SNOW) && PointInsideArena(arena, next))
        {
            particle.life = std::min(particle.life, 0.4f);
            particle.velocity = {0.0f, 0.0f, 0.0f};
            continue;
        }
        particle.position = next;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(),
                                   [](const Particle &p) { return p.life <= 0.0f; }),
                    particles.end());
    for (const Particle &particle : spawned)
    {
        AddParticle(particle);
    }
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
    for (Decal &decal : decals)
    {
        decal.life -= delta_seconds;
    }
    decals.erase(std::remove_if(decals.begin(), decals.end(), [](const Decal &d) { return d.life <= 0.0f; }),
                 decals.end());
}

void Effects::AddLights(SceneRenderer &renderer) const
{
    for (const Blast &blast : blasts)
    {
        if (blast.mist)
        {
            continue;
        }
        const Float t = math::Clamp(blast.life / blast.maxLife, 0.0f, 1.0f);
        renderer.AddLight(DynamicLight{blast.position, blast.color * (1.6f * t), blast.radius * 3.0f + 1.5f});
    }
    for (const Beam &beam : beams)
    {
        const Float t = math::Clamp(beam.life / beam.maxLife, 0.0f, 1.0f);
        const Float length = math::GetLength(beam.end - beam.start);
        if (beam.width < 0.1f || length < 0.5f)
        {
            continue;
        }
        renderer.AddLight(DynamicLight{math::GetLerp(beam.start, beam.end, 0.5f), beam.color * (0.9f * t),
                                       std::min(length * 0.6f, 14.0f)});
        renderer.AddLight(DynamicLight{beam.end, beam.color * (1.2f * t), 5.0f});
    }
}

void Effects::Draw(SceneRenderer &renderer)
{
    static constexpr MeshShape SPLATS[3] = {MeshShape::SPLAT_A, MeshShape::SPLAT_B, MeshShape::SPLAT_C};
    for (const Decal &decal : decals)
    {
        const Float fade = math::Clamp(decal.life / DECAL_FADE, 0.0f, 1.0f);
        // Fresh blood is glossy; it dries and darkens.
        const Float age = math::Clamp(1.0f - (decal.life / DECAL_LIFE), 0.0f, 1.0f);
        renderer.DrawDecal(SPLATS[decal.shape % 3], decal.position, decal.normal, decal.size * (0.4f + 0.6f * fade),
                           decal.spin, decal.color * (1.0f - age * 0.35f), 0.85f - age * 0.6f);
    }
    for (const Particle &particle : particles)
    {
        const Float t = math::Clamp(particle.life / particle.maxLife, 0.0f, 1.0f);
        const Float age = particle.maxLife - particle.life;
        switch (particle.kind)
        {
        case Kind::BLOOD:
        {
            // Stretched along the direction of travel so drops read as streaks.
            const Float speed = math::GetLength(particle.velocity);
            Float yaw = 0.0f;
            Float pitch = 0.0f;
            if (speed > 0.1f)
            {
                math::GetYawPitchFromDirection(particle.velocity, yaw, pitch);
            }
            const Float s = particle.size * (0.5f + 0.5f * t);
            renderer.DrawShape(MeshShape::SPHERE, particle.position, {s, s, s * (1.0f + speed * 0.06f)}, particle.color,
                               0.0f, yaw, pitch, 0.0f, SurfaceStyle{0.9f, 1.0f});
            break;
        }
        case Kind::MEAT:
        case Kind::LIMB:
        {
            const Float shrink = math::Clamp(particle.life / 0.6f, 0.0f, 1.0f);
            const Float s = particle.size * shrink;
            const FVec3 size = particle.kind == Kind::LIMB ? FVec3{particle.size * particle.scale.x, particle.size * particle.scale.y, particle.size} * shrink
                                                           : particle.scale * s;
            renderer.DrawShape(particle.kind == Kind::MEAT ? MeshShape::CHUNK : MeshShape::CUBE, particle.position, size,
                               particle.color, 0.0f, particle.spin * age, particle.spin * age * 0.7f, 0.0f,
                               SurfaceStyle{particle.kind == Kind::MEAT ? 0.8f : 0.3f, 1.0f});
            break;
        }
        case Kind::EMBER:
        case Kind::SNOW:
        {
            const Float fade = math::Clamp(std::min(particle.life, age) / 0.6f, 0.0f, 1.0f);
            const Float s = particle.size * fade;
            const Float flicker = particle.kind == Kind::EMBER ? 0.7f + 0.3f * std::sin(age * 13.0f + particle.position.x) : 1.0f;
            renderer.DrawShape(MeshShape::CUBE, particle.position, {s, s, s}, particle.color * flicker, particle.emissive,
                               age * 2.0f, age);
            break;
        }
        default:
        {
            const Float shrink = particle.kind == Kind::DEBRIS ? math::Clamp(particle.life / 0.6f, 0.0f, 1.0f) : (0.4f + 0.6f * t);
            const Float size = particle.size * shrink;
            const FVec3 color =
                particle.emissive > 0.5f ? particle.color * (0.35f + 0.65f * t) : particle.color;
            renderer.DrawShape(MeshShape::CUBE, particle.position, particle.scale * size, color, particle.emissive,
                               particle.spin * age, particle.spin * age * 0.7f);
            break;
        }
        }
    }
    for (const Beam &beam : beams)
    {
        const Float t = math::Clamp(beam.life / beam.maxLife, 0.0f, 1.0f);
        renderer.DrawBeam(
            beam.start, beam.end, beam.width * (0.25f + 0.75f * t), beam.color * (0.4f + 0.6f * t),
            1.0f
        );
    }
    for (const Blast &blast : blasts)
    {
        const Float t = math::Clamp(blast.life / blast.maxLife, 0.0f, 1.0f);
        const Float radius = blast.radius * (0.35f + 0.65f * std::sqrt(1.0f - t));
        if (blast.mist)
        {
            renderer.DrawGlow(blast.position, radius * 1.3f, blast.color * 0.5f, 0.55f * t);
            continue;
        }
        renderer.DrawShape(
            MeshShape::SPHERE, blast.position, {radius * 0.7f, radius * 0.7f, radius * 0.7f},
            blast.color * (0.3f + 0.7f * t), 1.0f
        );
        renderer.DrawGlow(blast.position, radius * 1.6f, blast.color, 0.5f * t);
    }
}
} // namespace evo
