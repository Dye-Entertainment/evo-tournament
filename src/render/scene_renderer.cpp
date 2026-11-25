#include "render/scene_renderer.h"

#include "graphics/renderer.h"
#include "graphics/transform.h"
#include "graphics/vertex_layout.h"
#include "math/evogine_math.h"
#include "resource/asset_manager.h"
#include "resource/material.h"
#include "resource/mesh.h"
#include "resource/shader_program.h"
#include "utils/logger.h"

#include "compiled/warning_off.h"
#include <algorithm>
#include <chrono>
#include "compiled/warning_on.h"

namespace evo
{
namespace
{
// Vertex: position (3), normal (3), color (4: albedo rgb, surface code), bake (4: static light rgb, sun).
// The surface code is the surface kind (0 plain, 1 panels, 2 glowing) plus 0.98 * ambient occlusion.
constexpr int FLOATS_PER_VERTEX = 14;
constexpr Float NEAR_PLANE = 0.05f;
constexpr Float FAR_PLANE = 450.0f;
constexpr Float SKY_SCALE = 250.0f;
constexpr Float BAKE_CELL = 1.5f;
constexpr int MAX_STATIC_LIGHTS = 4;
constexpr int LIGHT_SLOTS = SceneRenderer::MAX_DYNAMIC_LIGHTS + MAX_STATIC_LIGHTS;

constexpr Float MODE_OBJECT = 0.0f;
constexpr Float MODE_SKY = 1.0f;
constexpr Float MODE_GLOW = 2.0f;
constexpr Float MODE_SHADE = 3.0f;
constexpr Float MODE_LEVEL = 4.0f;

struct Vertex
{
    FVec3 position;
    FVec3 normal;
    FVec3 color;
    Float code = 0.98f;
    FVec3 light;
    Float sun = 1.0f;
};

void Push(std::vector<Float> &vertices, const Vertex &v)
{
    vertices.insert(vertices.end(), {v.position.x, v.position.y, v.position.z, v.normal.x, v.normal.y,
                                     v.normal.z, v.color.x, v.color.y, v.color.z, v.code, v.light.x,
                                     v.light.y, v.light.z, v.sun});
}

UInt32 VertexCount(const std::vector<Float> &vertices)
{
    return static_cast<UInt32>(vertices.size() / FLOATS_PER_VERTEX);
}

void AppendQuad(std::vector<Float> &vertices, std::vector<UInt32> &indices, const FVec3 corners[4],
                const FVec3 &normal, const FVec3 &color)
{
    const UInt32 base = VertexCount(vertices);
    for (int i = 0; i < 4; ++i)
    {
        Push(vertices, Vertex{corners[i], normal, color});
    }
    indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

void AppendUnitCube(std::vector<Float> &vertices, std::vector<UInt32> &indices)
{
    const FVec3 a{-0.5f, -0.5f, -0.5f};
    const FVec3 b{0.5f, 0.5f, 0.5f};
    const FVec3 px[4] = {{b.x, a.y, a.z}, {b.x, b.y, a.z}, {b.x, b.y, b.z}, {b.x, a.y, b.z}};
    const FVec3 nx[4] = {{a.x, a.y, b.z}, {a.x, b.y, b.z}, {a.x, b.y, a.z}, {a.x, a.y, a.z}};
    const FVec3 py[4] = {{a.x, b.y, a.z}, {a.x, b.y, b.z}, {b.x, b.y, b.z}, {b.x, b.y, a.z}};
    const FVec3 ny[4] = {{a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, a.y, b.z}, {a.x, a.y, b.z}};
    const FVec3 pz[4] = {{b.x, a.y, b.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}, {a.x, a.y, b.z}};
    const FVec3 nz[4] = {{a.x, a.y, a.z}, {a.x, b.y, a.z}, {b.x, b.y, a.z}, {b.x, a.y, a.z}};
    const FVec3 white{1, 1, 1};
    AppendQuad(vertices, indices, px, {1, 0, 0}, white);
    AppendQuad(vertices, indices, nx, {-1, 0, 0}, white);
    AppendQuad(vertices, indices, py, {0, 1, 0}, white);
    AppendQuad(vertices, indices, ny, {0, -1, 0}, white * 0.85f);
    AppendQuad(vertices, indices, pz, {0, 0, 1}, white);
    AppendQuad(vertices, indices, nz, {0, 0, -1}, white);
}

Float Hash(const Float x)
{
    const Float s = std::sin(x * 12.9898f) * 43758.5453f;
    return s - std::floor(s);
}

// A sphere whose radius is nudged per vertex so it looks like a lumpy chunk.
void AppendSphere(std::vector<Float> &vertices, std::vector<UInt32> &indices, const Float lumpiness,
                  const int stacks, const int slices)
{
    const UInt32 base = VertexCount(vertices);
    for (int stack = 0; stack <= stacks; ++stack)
    {
        const Float phi = math::PI * static_cast<Float>(stack) / static_cast<Float>(stacks);
        for (int slice = 0; slice <= slices; ++slice)
        {
            const Float theta = 2.0f * math::PI * static_cast<Float>(slice % slices) / static_cast<Float>(slices);
            const FVec3 normal{std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta)};
            const Float key = static_cast<Float>((stack == 0 || stack == stacks) ? stack * 100 : stack * 100 + slice % slices);
            const Float radius = 0.5f * (1.0f - lumpiness + lumpiness * 2.0f * Hash(key + 1.7f));
            Push(vertices, Vertex{normal * radius, normal, {1, 1, 1}});
        }
    }
    for (int stack = 0; stack < stacks; ++stack)
    {
        for (int slice = 0; slice < slices; ++slice)
        {
            const auto a = base + static_cast<UInt32>(stack * (slices + 1) + slice);
            const auto b = static_cast<UInt32>(a + slices + 1);
            indices.insert(indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }
}

// A flat, irregular splat in the XZ plane. The red channel fades from 1 in the middle to 0 at
// the rim (the shadow shading uses it for a soft edge).
void AppendDisc(std::vector<Float> &vertices, std::vector<UInt32> &indices, const FVec3 &center,
                const Float radius, const Float seed, const Float wobble)
{
    constexpr int SEGMENTS = 22;
    const UInt32 base = VertexCount(vertices);
    Push(vertices, Vertex{center, {0, 1, 0}, {1, 1, 1}});
    const Float phase_a = Hash(seed) * 6.28f;
    const Float phase_b = Hash(seed + 3.1f) * 6.28f;
    for (int i = 0; i < SEGMENTS; ++i)
    {
        const Float angle = 2.0f * math::PI * static_cast<Float>(i) / SEGMENTS;
        const Float r = radius * (1.0f + wobble * (0.22f * std::sin(3.0f * angle + phase_a) +
                                                   0.14f * std::sin(7.0f * angle + phase_b) +
                                                   0.10f * (Hash(seed + static_cast<Float>(i)) - 0.5f)));
        Push(vertices, Vertex{center + FVec3{std::cos(angle) * r, 0.0f, std::sin(angle) * r}, {0, 1, 0}, {0, 0, 0}});
    }
    for (int i = 0; i < SEGMENTS; ++i)
    {
        indices.insert(indices.end(), {base, base + 1 + static_cast<UInt32>((i + 1) % SEGMENTS),
                                       base + 1 + static_cast<UInt32>(i)});
    }
}

void AppendSplat(std::vector<Float> &vertices, std::vector<UInt32> &indices, const Float seed)
{
    AppendDisc(vertices, indices, {0, 0, 0}, 0.36f, seed, 1.0f);
    const int droplets = 4 + static_cast<int>(Hash(seed + 9.0f) * 4.0f);
    for (int i = 0; i < droplets; ++i)
    {
        const Float angle = Hash(seed + 20.0f + static_cast<Float>(i)) * 6.28f;
        const Float distance = 0.38f + Hash(seed + 40.0f + static_cast<Float>(i)) * 0.14f;
        const Float size = 0.03f + Hash(seed + 60.0f + static_cast<Float>(i)) * 0.06f;
        AppendDisc(vertices, indices, FVec3{std::cos(angle), 0.0f, std::sin(angle)} * distance, size,
                   seed + static_cast<Float>(i), 0.4f);
    }
}

Quat AlignUpTo(const FVec3 &normal)
{
    if (normal.y > 0.5f)
    {
        return Quat::GetIdentity();
    }
    if (normal.y < -0.5f)
    {
        return math::GetAngleAxis(math::PI, FVec3{1, 0, 0});
    }
    if (std::fabs(normal.x) > 0.5f)
    {
        return math::GetAngleAxis(normal.x > 0.0f ? -math::PI * 0.5f : math::PI * 0.5f, FVec3{0, 0, 1});
    }
    return math::GetAngleAxis(normal.z > 0.0f ? math::PI * 0.5f : -math::PI * 0.5f, FVec3{1, 0, 0});
}

graphics::Transform MakeTransform(const FVec3 &position, const FVec3 &scale, const Quat &rotation)
{
    graphics::Transform transform;
    transform.worldPosition = position;
    transform.worldScale = scale;
    transform.worldRotation = rotation;
    return transform;
}

// ---- Light baking --------------------------------------------------------------------------

Bool InsideSolid(const Arena &arena, const FVec3 &point)
{
    for (const ArenaBox &box : arena.boxes)
    {
        if (box.glow == 0.0f && primitive::Contains(box.bounds, point))
        {
            return true;
        }
    }
    return false;
}

Bool Occluded(const Arena &arena, const FVec3 &origin, const FVec3 &direction, const Float max_distance)
{
    for (const ArenaBox &box : arena.boxes)
    {
        if (box.glow > 0.0f)
        {
            continue;
        }
        Float distance = 0.0f;
        FVec3 normal;
        if (primitive::IntersectRay(box.bounds, origin, direction, max_distance, distance, normal) &&
            distance < max_distance && !primitive::Contains(box.bounds, origin))
        {
            return true;
        }
    }
    return false;
}

struct FaceFrame
{
    FVec3 normal;
    FVec3 u;
    FVec3 v;
};

Float BakeOcclusion(const Arena &arena, const FVec3 &p, const FaceFrame &frame)
{
    int hits = 0;
    int samples = 0;
    for (const Float reach : {0.5f, 1.3f})
    {
        for (const Float su : {-1.0f, 1.0f})
        {
            for (const Float sv : {-1.0f, 1.0f})
            {
                const FVec3 q = p + frame.normal * (reach * 0.6f) + frame.u * (su * reach) + frame.v * (sv * reach);
                hits += InsideSolid(arena, q) ? 1 : 0;
                ++samples;
            }
        }
        hits += InsideSolid(arena, p + frame.normal * (reach * 1.4f)) ? 1 : 0;
        ++samples;
    }
    const Float occlusion = static_cast<Float>(hits) / static_cast<Float>(samples);
    return math::Clamp(1.0f - occlusion * 0.9f, 0.15f, 1.0f);
}

Float BakeSun(const Arena &arena, const FVec3 &p, const FVec3 &normal, const FVec3 &sun)
{
    if (math::GetDotProduct(normal, sun) <= 0.0f)
    {
        return 0.0f;
    }
    // A few slightly spread rays give a soft shadow edge.
    const FVec3 side = math::GetNormalized(math::GetCrossProduct(sun, FVec3{0.0f, 1.0f, 0.001f}));
    const FVec3 up = math::GetCrossProduct(side, sun);
    const FVec3 origin = p + normal * 0.04f;
    Float lit = 0.0f;
    const FVec3 offsets[4] = {side * 0.035f, side * -0.035f, up * 0.035f, up * -0.035f};
    for (const FVec3 &offset : offsets)
    {
        lit += Occluded(arena, origin, math::GetNormalized(sun + offset), 120.0f) ? 0.0f : 0.25f;
    }
    return lit;
}

FVec3 BakeLights(const Arena &arena, const FVec3 &p, const FVec3 &normal)
{
    FVec3 total{0.0f, 0.0f, 0.0f};
    const FVec3 origin = p + normal * 0.04f;
    for (const ArenaLight &light : arena.lights)
    {
        const FVec3 to_light = light.position - origin;
        const Float distance = math::GetLength(to_light);
        if (distance >= light.radius || distance < 1e-3f)
        {
            continue;
        }
        const FVec3 direction = to_light * (1.0f / distance);
        const Float facing = math::GetDotProduct(normal, direction);
        if (facing <= 0.0f)
        {
            continue;
        }
        const Float falloff = 1.0f - (distance * distance) / (light.radius * light.radius);
        if (Occluded(arena, origin, direction, distance - 0.05f))
        {
            continue;
        }
        total = total + light.color * (facing * falloff * falloff * 1.4f);
    }
    return total;
}

Bool FaceHidden(const Arena &arena, size_t self, const FVec3 corners[4], const FVec3 &normal)
{
    // A face is hidden when its center and corners all sit inside another solid box.
    FVec3 center{0, 0, 0};
    for (int i = 0; i < 4; ++i)
    {
        center = center + corners[i] * 0.25f;
    }
    const auto covered = [&](const FVec3 &point)
    {
        const FVec3 probe = point + normal * 0.01f;
        for (size_t i = 0; i < arena.boxes.size(); ++i)
        {
            if (i != self && arena.boxes[i].glow == 0.0f && primitive::Contains(arena.boxes[i].bounds, probe))
            {
                return true;
            }
        }
        return false;
    };
    if (!covered(center))
    {
        return false;
    }
    for (int i = 0; i < 4; ++i)
    {
        if (!covered(math::GetLerp(corners[i], center, 0.02f)))
        {
            return false;
        }
    }
    return true;
}

void BakeLevel(const Arena &arena, std::vector<Float> &vertices, std::vector<UInt32> &indices)
{
    if (arena.boxes.empty())
    {
        return;
    }
    const primitive::AxisAlignedBox &floor = arena.boxes[0].bounds;
    const FVec3 sun = math::GetNormalized(arena.theme.sunDirection);
    for (size_t index = 0; index < arena.boxes.size(); ++index)
    {
        const ArenaBox &box = arena.boxes[index];
        const FVec3 &a = box.bounds.min;
        const FVec3 &b = box.bounds.max;
        // A small per-box tint keeps big surfaces from looking flat.
        const Float tint = 0.94f + 0.12f * Hash(a.x * 0.37f + a.y * 1.91f + a.z * 0.73f + b.x * 0.11f);
        const FVec3 color = box.glow > 0.0f ? box.color : box.color * tint;
        const Float kind = box.glow > 0.0f ? 2.0f : (box.grid > 0.0f ? 1.0f : 0.0f);

        struct Face
        {
            FVec3 origin;
            FVec3 u;
            FVec3 v;
            FVec3 normal;
        };
        const FVec3 size = b - a;
        const Face faces[6] = {
            {{b.x, a.y, a.z}, {0, 0, size.z}, {0, size.y, 0}, {1, 0, 0}},
            {{a.x, a.y, b.z}, {0, 0, -size.z}, {0, size.y, 0}, {-1, 0, 0}},
            {{a.x, b.y, a.z}, {0, 0, size.z}, {size.x, 0, 0}, {0, 1, 0}},
            {{a.x, a.y, a.z}, {size.x, 0, 0}, {0, 0, size.z}, {0, -1, 0}},
            {{b.x, a.y, b.z}, {-size.x, 0, 0}, {0, size.y, 0}, {0, 0, 1}},
            {{a.x, a.y, a.z}, {0, size.y, 0}, {size.x, 0, 0}, {0, 0, -1}},
        };
        for (const Face &face : faces)
        {
            const FVec3 corners[4] = {face.origin, face.origin + face.u, face.origin + face.u + face.v,
                                      face.origin + face.v};
            FVec3 center = face.origin + face.u * 0.5f + face.v * 0.5f;
            // Skip the underside of the floor and faces that point out of the arena.
            if (face.normal.y < -0.5f && a.y <= floor.min.y + 0.01f)
            {
                continue;
            }
            const FVec3 outside = center + face.normal * 0.5f;
            if (outside.x < floor.min.x || outside.x > floor.max.x || outside.z < floor.min.z ||
                outside.z > floor.max.z)
            {
                continue;
            }
            if (FaceHidden(arena, index, corners, face.normal))
            {
                continue;
            }
            const Float u_length = math::GetLength(face.u);
            const Float v_length = math::GetLength(face.v);
            const int nu = box.glow > 0.0f ? 1 : std::max(1, static_cast<int>(std::ceil(u_length / BAKE_CELL)));
            const int nv = box.glow > 0.0f ? 1 : std::max(1, static_cast<int>(std::ceil(v_length / BAKE_CELL)));
            const FaceFrame frame{face.normal, face.u * (1.0f / std::max(u_length, 1e-4f)),
                                  face.v * (1.0f / std::max(v_length, 1e-4f))};
            const UInt32 base = VertexCount(vertices);
            for (int j = 0; j <= nv; ++j)
            {
                for (int i = 0; i <= nu; ++i)
                {
                    const FVec3 p = face.origin + face.u * (static_cast<Float>(i) / static_cast<Float>(nu)) +
                                    face.v * (static_cast<Float>(j) / static_cast<Float>(nv));
                    Vertex vertex{p, face.normal, color};
                    if (box.glow > 0.0f)
                    {
                        vertex.code = kind + 0.98f;
                        vertex.light = {0, 0, 0};
                        vertex.sun = 1.0f;
                    }
                    else
                    {
                        vertex.code = kind + 0.98f * BakeOcclusion(arena, p, frame);
                        vertex.sun = BakeSun(arena, p, face.normal, sun);
                        vertex.light = BakeLights(arena, p, face.normal);
                    }
                    Push(vertices, vertex);
                }
            }
            for (int j = 0; j < nv; ++j)
            {
                for (int i = 0; i < nu; ++i)
                {
                    const auto row = static_cast<UInt32>(nu + 1);
                    const UInt32 p00 = base + static_cast<UInt32>(j) * row + static_cast<UInt32>(i);
                    const UInt32 p10 = p00 + 1;
                    const UInt32 p01 = p00 + row;
                    const UInt32 p11 = p01 + 1;
                    indices.insert(indices.end(), {p00, p10, p11, p00, p11, p01});
                }
            }
        }
    }
}
} // namespace

resource::Mesh *SceneRenderer::CreateMesh(
    const std::vector<Float> &vertices, const std::vector<UInt32> &indices
)
{
    const VertexLayout layout({
        VertexAttribute{VertexAttribute::Semantic::Position, VertexAttribute::Type::Float3},
        VertexAttribute{VertexAttribute::Semantic::Normal, VertexAttribute::Type::Float3},
        VertexAttribute{VertexAttribute::Semantic::Tangent, VertexAttribute::Type::Float4},
        VertexAttribute{VertexAttribute::Semantic::Color, VertexAttribute::Type::Float4},
    });
    return graphics::Renderer::GetInstance()->SetupMesh(
        reinterpret_cast<const Byte *>(vertices.data()),
        static_cast<UInt32>(vertices.size() * sizeof(Float)),
        reinterpret_cast<const Byte *>(indices.data()), static_cast<UInt32>(indices.size()),
        graphics::IndexType::UnsignedInt, layout
    );
}

SceneRenderer::SceneRenderer() = default;
SceneRenderer::~SceneRenderer() = default;

Bool SceneRenderer::Initialize()
{
    shader = resource::AssetManager::GetInstance()->Load<resource::ShaderProgram>(
        resource::AssetPath{"game://shaders/arena.shaderprogram"}
    );

    std::vector<Float> vertices;
    std::vector<UInt32> indices;
    shapes.assign(static_cast<size_t>(MeshShape::COUNT), nullptr);
    const auto build = [&](const MeshShape shape, const auto &append)
    {
        vertices.clear();
        indices.clear();
        append();
        shapes[static_cast<size_t>(shape)] = CreateMesh(vertices, indices);
    };
    build(MeshShape::CUBE, [&] { AppendUnitCube(vertices, indices); });
    build(MeshShape::SPHERE, [&] { AppendSphere(vertices, indices, 0.0f, 12, 18); });
    build(MeshShape::CHUNK, [&] { AppendSphere(vertices, indices, 0.35f, 5, 7); });
    build(MeshShape::SPLAT_A, [&] { AppendSplat(vertices, indices, 1.0f); });
    build(MeshShape::SPLAT_B, [&] { AppendSplat(vertices, indices, 7.0f); });
    build(MeshShape::SPLAT_C, [&] { AppendSplat(vertices, indices, 13.0f); });
    build(MeshShape::DISC, [&] { AppendDisc(vertices, indices, {0, 0, 0}, 0.5f, 0.0f, 0.0f); });

    vertices.clear();
    indices.clear();
    const FVec3 corners[8] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
                              {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}};
    const int faces[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
                             {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}};
    for (const auto &face : faces)
    {
        FVec3 quad[4];
        for (int i = 0; i < 4; ++i)
        {
            quad[i] = corners[face[i]];
        }
        AppendQuad(vertices, indices, quad, {0, 1, 0}, {1, 1, 1});
    }
    skyMesh = CreateMesh(vertices, indices);

    // The level mesh starts empty and is filled by SetArena.
    vertices.clear();
    indices.clear();
    Push(vertices, Vertex{});
    Push(vertices, Vertex{});
    Push(vertices, Vertex{});
    indices = {0, 1, 2};
    levelMesh = CreateMesh(vertices, indices);

    for (resource::Mesh *mesh : shapes)
    {
        if (mesh == nullptr)
        {
            LOGERROR("Failed to create shape meshes");
            return false;
        }
    }
    if (levelMesh == nullptr || skyMesh == nullptr)
    {
        LOGERROR("Failed to create arena meshes");
        return false;
    }
    lightData.assign(LIGHT_SLOTS / 2, Mat4{});
    return true;
}

void SceneRenderer::SetArena(const Arena &new_arena)
{
    arena = &new_arena;
    theme = new_arena.theme;
    if (bakedArena == static_cast<int>(new_arena.id) || levelMesh == nullptr)
    {
        return;
    }
    bakedArena = static_cast<int>(new_arena.id);
    std::vector<Float> vertices;
    std::vector<UInt32> indices;
    const auto start = std::chrono::steady_clock::now();
    BakeLevel(new_arena, vertices, indices);
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    LOGINFO("Baked %s: %u vertices in %d ms", new_arena.name.c_str(), VertexCount(vertices),
            static_cast<int>(milliseconds));
    graphics::Renderer::GetInstance()->UpdateMesh(
        levelMesh, reinterpret_cast<const Byte *>(vertices.data()),
        static_cast<UInt32>(vertices.size() * sizeof(Float)),
        reinterpret_cast<const Byte *>(indices.data()), static_cast<UInt32>(indices.size())
    );
}

Float SceneRenderer::SunVisibility(const FVec3 &point) const
{
    if (arena == nullptr)
    {
        return 1.0f;
    }
    return Occluded(*arena, point, math::GetNormalized(theme.sunDirection), 120.0f) ? 0.0f : 1.0f;
}

void SceneRenderer::BeginFrame(const Float frame_time)
{
    materialsUsed = 0;
    glowMaterialsUsed = 0;
    lights.clear();
    time = frame_time;
    Mat4 theme_a;
    Mat4 theme_b;
    theme_a[0] = FVec4(theme.skyZenith, theme.stars);
    theme_a[1] = FVec4(theme.skyHorizon, theme.aurora);
    theme_a[2] = FVec4(theme.skyAmbient, theme.clouds);
    theme_a[3] = FVec4(theme.groundAmbient, theme.embers);
    theme_b[0] = FVec4(math::GetNormalized(theme.sunDirection), time);
    theme_b[1] = FVec4(theme.sunColor, theme.fogDensity);
    theme_b[2] = FVec4(theme.fogColor, 0.0f);
    theme_b[3] = FVec4(0.0f, 0.0f, 0.0f, 0.0f);
    themeData = {theme_a, theme_b};
}

void SceneRenderer::AddLight(const DynamicLight &light)
{
    lights.push_back(light);
}

resource::Material *SceneRenderer::CreateMaterial(const Bool transparent)
{
    auto material = std::make_unique<resource::Material>(shader);
    MaterialState state;
    state.isDoubleSided = true;
    state.castsShadows = false;
    state.receivesShadows = false;
    if (transparent)
    {
        state.alphaMode = graphics::AlphaMode::Blend;
    }
    material->SetMaterialState(state);
    auto &pool = transparent ? glowMaterials : materials;
    pool.push_back(std::move(material));
    return pool.back().get();
}

resource::Material *SceneRenderer::NextMaterial(
    const FVec3 &color, const Float emissive, const Float gloss, const Float mode, const Float sun,
    const Float alpha
)
{
    const Bool transparent = mode == MODE_GLOW || mode == MODE_SHADE;
    auto &pool = transparent ? glowMaterials : materials;
    size_t &used = transparent ? glowMaterialsUsed : materialsUsed;

    // Pack the theme and this frame's lights once per frame (the first material of the frame).
    if (materialsUsed == 0 && glowMaterialsUsed == 0)
    {
        // Choose lights: the brightest dynamic ones near the camera, then the nearest level lights.
        std::vector<DynamicLight> chosen = lights;
        const auto weight = [&](const DynamicLight &light)
        {
            const Float distance = math::GetLength(light.position - cameraPosition);
            return (light.color.x + light.color.y + light.color.z) * light.radius / (distance + 4.0f);
        };
        std::sort(chosen.begin(), chosen.end(),
                  [&](const DynamicLight &a, const DynamicLight &b) { return weight(a) > weight(b); });
        if (chosen.size() > static_cast<size_t>(MAX_DYNAMIC_LIGHTS))
        {
            chosen.resize(MAX_DYNAMIC_LIGHTS);
        }
        const size_t dynamic_count = chosen.size();
        if (arena != nullptr)
        {
            std::vector<ArenaLight> statics = arena->lights;
            std::sort(statics.begin(), statics.end(),
                      [&](const ArenaLight &a, const ArenaLight &b)
                      {
                          return math::GetLength(a.position - cameraPosition) - a.radius <
                                 math::GetLength(b.position - cameraPosition) - b.radius;
                      });
            for (size_t i = 0; i < statics.size() && i < static_cast<size_t>(MAX_STATIC_LIGHTS); ++i)
            {
                chosen.push_back(DynamicLight{statics[i].position, statics[i].color, statics[i].radius});
            }
        }
        for (Mat4 &m : lightData)
        {
            m = Mat4{};
            for (int c = 0; c < 4; ++c)
            {
                m[c] = FVec4(0.0f, 0.0f, 0.0f, 0.0f);
            }
        }
        for (size_t i = 0; i < chosen.size() && i < static_cast<size_t>(LIGHT_SLOTS); ++i)
        {
            Mat4 &m = lightData[i / 2];
            const int column = static_cast<int>(i % 2) * 2;
            const Float is_static = i >= dynamic_count ? 1.0f : 0.0f;
            m[column] = FVec4(chosen[i].position, chosen[i].radius);
            m[column + 1] = FVec4(chosen[i].color, is_static);
        }
    }

    if (used == pool.size())
    {
        CreateMaterial(transparent);
    }
    resource::Material *material = pool[used++].get();
    material->Set("uTheme", themeData);
    material->Set("uLights", lightData);
    material->Set("uTint", FVec4(color, emissive));
    material->Set("uMaterial", FVec4(gloss, mode, sun, alpha));
    return material;
}

void SceneRenderer::SetCamera(
    const FVec3 &eye, const Float yaw, const Float pitch, const Float fov_degrees, const Float aspect,
    const FVec3 &up
)
{
    cameraPosition = eye;
    auto &camera = graphics::Renderer::GetInstance()->GetCamera();
    const FVec3 forward = math::GetDirectionFromYawPitch(yaw, pitch);
    camera.view = math::GetLookAtMatrix(eye, eye + forward, up);
    camera.projection = math::GetPerspectiveProjectionMatrix(fov_degrees, aspect, NEAR_PLANE, FAR_PLANE);
    // The engine starts from identity and leaves m[3][3] = 1, which makes clip w = z + 1.
    camera.projection[3][3] = 0.0f;
    camera.position = FVec4(eye, 0.0f);
    camera.previousPosition = FVec4(eye, 0.0f);
    viewProjection = camera.projection * camera.view;
}

void SceneRenderer::DrawLevel()
{
    graphics::Renderer::GetInstance()->SubmitOpaqueMesh(
        levelMesh, NextMaterial({1, 1, 1}, 0.0f, 0.3f, MODE_LEVEL, 1.0f, 1.0f),
        MakeTransform({0, 0, 0}, {1, 1, 1}, Quat::GetIdentity())
    );
}

void SceneRenderer::DrawSky()
{
    graphics::Renderer::GetInstance()->SubmitOpaqueMesh(
        skyMesh, NextMaterial({1, 1, 1}, 0.0f, 0.0f, MODE_SKY, 1.0f, 1.0f),
        MakeTransform(cameraPosition, {SKY_SCALE, SKY_SCALE, SKY_SCALE}, Quat::GetIdentity())
    );
}

void SceneRenderer::DrawShape(
    const MeshShape shape, const FVec3 &center, const FVec3 &size, const FVec3 &color,
    const Float emissive, const Float yaw, const Float pitch, const Float roll, const SurfaceStyle &style
)
{
    Quat rotation = math::GetQuatFromYawPitch(yaw, pitch);
    if (roll != 0.0f)
    {
        rotation = rotation * math::GetAngleAxis(roll, FVec3{0.0f, 0.0f, 1.0f});
    }
    DrawRotated(shape, center, size, rotation, color, emissive, style);
}

void SceneRenderer::DrawRotated(
    const MeshShape shape, const FVec3 &center, const FVec3 &size, const Quat &rotation,
    const FVec3 &color, const Float emissive, const SurfaceStyle &style
)
{
    graphics::Renderer::GetInstance()->SubmitOpaqueMesh(
        shapes[static_cast<size_t>(shape)],
        NextMaterial(color, emissive, style.gloss, MODE_OBJECT, style.sun, 1.0f),
        MakeTransform(center, size, rotation)
    );
}

void SceneRenderer::DrawDecal(
    const MeshShape shape, const FVec3 &position, const FVec3 &normal, const Float size, const Float spin,
    const FVec3 &color, const Float gloss
)
{
    const Quat rotation = AlignUpTo(normal) * math::GetAngleAxis(spin, FVec3{0.0f, 1.0f, 0.0f});
    const Bool shade = shape == MeshShape::DISC;
    resource::Material *material =
        shade ? NextMaterial(color, 0.0f, 0.0f, MODE_SHADE, 1.0f, gloss)
              : NextMaterial(color, 0.0f, gloss, MODE_OBJECT, 1.0f, 1.0f);
    const graphics::Transform transform =
        MakeTransform(position + normal * 0.015f, {size, size, size}, rotation);
    if (shade)
    {
        graphics::Renderer::GetInstance()->SubmitTransparentMesh(shapes[static_cast<size_t>(shape)], material, transform);
    }
    else
    {
        graphics::Renderer::GetInstance()->SubmitOpaqueMesh(shapes[static_cast<size_t>(shape)], material, transform);
    }
}

void SceneRenderer::DrawGlow(const FVec3 &center, const Float radius, const FVec3 &color, const Float alpha)
{
    graphics::Renderer::GetInstance()->SubmitTransparentMesh(
        shapes[static_cast<size_t>(MeshShape::SPHERE)],
        NextMaterial(color, 1.0f, 0.0f, MODE_GLOW, 1.0f, alpha),
        MakeTransform(center, {radius, radius, radius}, Quat::GetIdentity())
    );
}

void SceneRenderer::DrawBeam(
    const FVec3 &start, const FVec3 &end, const Float width, const FVec3 &color, const Float emissive
)
{
    const FVec3 delta = end - start;
    const Float length = math::GetLength(delta);
    if (length < 0.01f)
    {
        return;
    }
    Float yaw = 0.0f;
    Float pitch = 0.0f;
    math::GetYawPitchFromDirection(delta, yaw, pitch);
    DrawShape(
        MeshShape::CUBE, math::GetLerp(start, end, 0.5f), {width, width, length}, color, emissive,
        yaw, pitch
    );
}

Bool SceneRenderer::Project(
    const FVec3 &world, FVec2 &screen, const Float screen_width, const Float screen_height
) const
{
    const FVec4 clip = viewProjection * FVec4(world, 1.0f);
    if (clip.w <= 0.05f)
    {
        return false;
    }
    const Float x = clip.x / clip.w;
    const Float y = clip.y / clip.w;
    screen.x = (x * 0.5f + 0.5f) * screen_width;
    screen.y = (1.0f - (y * 0.5f + 0.5f)) * screen_height;
    return x > -1.2f && x < 1.2f && y > -1.2f && y < 1.2f;
}
} // namespace evo
