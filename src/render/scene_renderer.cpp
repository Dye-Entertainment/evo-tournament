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

namespace evo
{
namespace
{
constexpr int FLOATS_PER_VERTEX = 10;
constexpr Float NEAR_PLANE = 0.05f;
constexpr Float FAR_PLANE = 450.0f;
constexpr Float SKY_SCALE = 250.0f;
const FVec3 SKY_ZENITH{0.07f, 0.06f, 0.20f};
const FVec3 SKY_HORIZON{0.95f, 0.45f, 0.30f};

void PushVertex(std::vector<Float> &vertices, const FVec3 &position, const FVec3 &normal,
                const FVec3 &color, const Float grid)
{
    vertices.insert(vertices.end(), {position.x, position.y, position.z, normal.x, normal.y, normal.z,
                                     color.x, color.y, color.z, grid});
}

void AppendQuad(std::vector<Float> &vertices, std::vector<UInt32> &indices, const FVec3 corners[4],
                const FVec3 &normal, const FVec3 colors[4], const Float grid)
{
    const auto base = static_cast<UInt32>(vertices.size() / FLOATS_PER_VERTEX);
    for (int i = 0; i < 4; ++i)
    {
        PushVertex(vertices, corners[i], normal, colors[i], grid);
    }
    indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

void AppendBox(
    std::vector<Float> &vertices, std::vector<UInt32> &indices,
    const primitive::AxisAlignedBox &box, const FVec3 &color, const Float grid
)
{
    const FVec3 &a = box.min;
    const FVec3 &b = box.max;
    const FVec3 side_color = color * 0.94f;
    const FVec3 bottom_color = color * 0.8f;
    const FVec3 top[4] = {color, color, color, color};
    const FVec3 side[4] = {side_color, side_color, side_color, side_color};
    const FVec3 bottom[4] = {bottom_color, bottom_color, bottom_color, bottom_color};

    const FVec3 px[4] = {{b.x, a.y, a.z}, {b.x, b.y, a.z}, {b.x, b.y, b.z}, {b.x, a.y, b.z}};
    const FVec3 nx[4] = {{a.x, a.y, b.z}, {a.x, b.y, b.z}, {a.x, b.y, a.z}, {a.x, a.y, a.z}};
    const FVec3 py[4] = {{a.x, b.y, a.z}, {a.x, b.y, b.z}, {b.x, b.y, b.z}, {b.x, b.y, a.z}};
    const FVec3 ny[4] = {{a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, a.y, b.z}, {a.x, a.y, b.z}};
    const FVec3 pz[4] = {{b.x, a.y, b.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}, {a.x, a.y, b.z}};
    const FVec3 nz[4] = {{a.x, a.y, a.z}, {a.x, b.y, a.z}, {b.x, b.y, a.z}, {b.x, a.y, a.z}};

    AppendQuad(vertices, indices, px, {1, 0, 0}, side, grid);
    AppendQuad(vertices, indices, nx, {-1, 0, 0}, side, grid);
    AppendQuad(vertices, indices, py, {0, 1, 0}, top, grid);
    AppendQuad(vertices, indices, ny, {0, -1, 0}, bottom, grid);
    AppendQuad(vertices, indices, pz, {0, 0, 1}, side, grid);
    AppendQuad(vertices, indices, nz, {0, 0, -1}, side, grid);
}

FVec3 SkyColor(const Float y)
{
    const Float t = math::Clamp(y * 0.5f + 0.5f, 0.0f, 1.0f);
    return math::GetLerp(SKY_HORIZON, SKY_ZENITH, t);
}

graphics::Transform MakeTransform(const FVec3 &position, const FVec3 &scale, const Quat &rotation)
{
    graphics::Transform transform;
    transform.worldPosition = position;
    transform.worldScale = scale;
    transform.worldRotation = rotation;
    return transform;
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

Bool SceneRenderer::Initialize(const Arena &arena)
{
    shader = resource::AssetManager::GetInstance()->Load<resource::ShaderProgram>(
        resource::AssetPath{"game://shaders/arena.shaderprogram"}
    );

    std::vector<Float> vertices;
    std::vector<UInt32> indices;
    for (const ArenaBox &box : arena.boxes)
    {
        AppendBox(vertices, indices, box.bounds, box.color, box.grid);
    }
    levelMesh = CreateMesh(vertices, indices);

    vertices.clear();
    indices.clear();
    AppendBox(
        vertices, indices, primitive::AxisAlignedBox{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}},
        {1, 1, 1}, 0.0f
    );
    cubeMesh = CreateMesh(vertices, indices);

    vertices.clear();
    indices.clear();
    constexpr int STACKS = 10;
    constexpr int SLICES = 14;
    for (int stack = 0; stack <= STACKS; ++stack)
    {
        const Float phi = math::PI * static_cast<Float>(stack) / STACKS;
        for (int slice = 0; slice <= SLICES; ++slice)
        {
            const Float theta = 2.0f * math::PI * static_cast<Float>(slice) / SLICES;
            const FVec3 normal{std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta)};
            PushVertex(vertices, normal * 0.5f, normal, {1, 1, 1}, 0.0f);
        }
    }
    for (int stack = 0; stack < STACKS; ++stack)
    {
        for (int slice = 0; slice < SLICES; ++slice)
        {
            const auto a = static_cast<UInt32>(stack * (SLICES + 1) + slice);
            const auto b = static_cast<UInt32>(a + SLICES + 1);
            indices.insert(indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }
    sphereMesh = CreateMesh(vertices, indices);

    vertices.clear();
    indices.clear();
    const FVec3 corners[8] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
                              {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}};
    const int faces[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
                             {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}};
    for (const auto &face : faces)
    {
        FVec3 quad[4];
        FVec3 colors[4];
        for (int i = 0; i < 4; ++i)
        {
            quad[i] = corners[face[i]];
            colors[i] = SkyColor(quad[i].y);
        }
        AppendQuad(vertices, indices, quad, {0, 1, 0}, colors, 0.0f);
    }
    skyMesh = CreateMesh(vertices, indices);

    if (levelMesh == nullptr || cubeMesh == nullptr || sphereMesh == nullptr || skyMesh == nullptr)
    {
        LOGERROR("Failed to create arena meshes");
        return false;
    }
    return true;
}

void SceneRenderer::BeginFrame()
{
    materialsUsed = 0;
}

resource::Material *SceneRenderer::NextMaterial(const FVec3 &color, const Float emissive)
{
    if (materialsUsed == materials.size())
    {
        auto material = std::make_unique<resource::Material>(shader);
        MaterialState state;
        state.isDoubleSided = true;
        state.castsShadows = false;
        state.receivesShadows = false;
        material->SetMaterialState(state);
        materials.push_back(std::move(material));
    }
    resource::Material *material = materials[materialsUsed++].get();
    material->Set("uTint", FVec4(color, emissive));
    return material;
}

void SceneRenderer::SetCamera(
    const FVec3 &eye, const Float yaw, const Float pitch, const Float fov_degrees, const Float aspect
)
{
    cameraPosition = eye;
    auto &camera = graphics::Renderer::GetInstance()->GetCamera();
    const FVec3 forward = math::GetDirectionFromYawPitch(yaw, pitch);
    camera.view = math::GetLookAtMatrix(eye, eye + forward, FVec3{0.0f, 1.0f, 0.0f});
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
        levelMesh, NextMaterial({1, 1, 1}, 0.0f),
        MakeTransform({0, 0, 0}, {1, 1, 1}, Quat::GetIdentity())
    );
}

void SceneRenderer::DrawSky()
{
    graphics::Renderer::GetInstance()->SubmitOpaqueMesh(
        skyMesh, NextMaterial({1, 1, 1}, 2.0f),
        MakeTransform(cameraPosition, {SKY_SCALE, SKY_SCALE, SKY_SCALE}, Quat::GetIdentity())
    );
}

void SceneRenderer::DrawShape(
    const MeshShape shape, const FVec3 &center, const FVec3 &size, const FVec3 &color,
    const Float emissive, const Float yaw, const Float pitch, const Float roll
)
{
    Quat rotation = math::GetQuatFromYawPitch(yaw, pitch);
    if (roll != 0.0f)
    {
        rotation = rotation * math::GetAngleAxis(roll, FVec3{0.0f, 0.0f, 1.0f});
    }
    graphics::Renderer::GetInstance()->SubmitOpaqueMesh(
        shape == MeshShape::CUBE ? cubeMesh : sphereMesh, NextMaterial(color, emissive),
        MakeTransform(center, size, rotation)
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
