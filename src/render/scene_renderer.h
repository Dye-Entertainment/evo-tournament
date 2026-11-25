#pragma once
#include "game/arena.h"
#include "game/math_util.h"
#include "primitive/fvec2.h"
#include "primitive/mat4.h"
#include "primitive/quat.h"
#include "resource/asset_proxy.h"

#include "compiled/warning_off.h"
#include <memory>
#include <vector>
#include "compiled/warning_on.h"

namespace resource
{
class Mesh;
class Material;
class ShaderProgram;
} // namespace resource

namespace evo
{
enum class MeshShape : UInt8
{
    CUBE,
    SPHERE,
    // A lumpy rock-like blob, used for gibs and debris.
    CHUNK,
    // Flat splat shapes lying in the XZ plane (normal +Y), used for blood decals and shadows.
    SPLAT_A,
    SPLAT_B,
    SPLAT_C,
    DISC,
    COUNT
};

// Optional surface settings for a draw.
struct SurfaceStyle
{
    // 0 is matte, 1 is wet or polished.
    Float gloss = 0.25f;
    // How much sun reaches the object (0 when it stands in the level's shadow).
    Float sun = 1.0f;
};

// A light that lives for a moment (muzzle flash, explosion, rail beam...).
struct DynamicLight
{
    FVec3 position;
    FVec3 color;
    Float radius = 6.0f;
};

class SceneRenderer final
{
  public:
    static constexpr int MAX_DYNAMIC_LIGHTS = 8;

    SceneRenderer();
    ~SceneRenderer();
    Bool Initialize();
    // Builds the level mesh with baked lighting and adopts the arena's theme. Cheap to call again
    // for the same arena.
    void SetArena(const Arena &arena);
    void BeginFrame(Float time);
    void SetCamera(
        const FVec3 &eye, Float yaw, Float pitch, Float fov_degrees, Float aspect,
        const FVec3 &up = FVec3{0.0f, 1.0f, 0.0f}
    );
    void AddLight(const DynamicLight &light);
    void DrawLevel();
    void DrawSky();
    void DrawShape(MeshShape shape, const FVec3 &center, const FVec3 &size, const FVec3 &color,
                   Float emissive = 0.0f, Float yaw = 0.0f, Float pitch = 0.0f, Float roll = 0.0f,
                   const SurfaceStyle &style = SurfaceStyle{});
    void DrawRotated(MeshShape shape, const FVec3 &center, const FVec3 &size, const Quat &rotation,
                     const FVec3 &color, Float emissive, const SurfaceStyle &style = SurfaceStyle{});
    // A flat shape lying on a surface with the given (axis-aligned) normal.
    void DrawDecal(MeshShape shape, const FVec3 &position, const FVec3 &normal, Float size, Float spin,
                   const FVec3 &color, Float gloss);
    // A soft, see-through glowing ball (halo around flashes, orbs and explosions).
    void DrawGlow(const FVec3 &center, Float radius, const FVec3 &color, Float alpha);
    void DrawBeam(const FVec3 &start, const FVec3 &end, Float width, const FVec3 &color,
                  Float emissive);
    [[nodiscard]]
    Bool Project(const FVec3 &world, FVec2 &screen, Float screen_width, Float screen_height) const;

    [[nodiscard]]
    const FVec3 &CameraPosition() const
    {
        return cameraPosition;
    }

    // 0..1: how much sun reaches `point` (a ray toward the sun against the level).
    [[nodiscard]]
    Float SunVisibility(const FVec3 &point) const;

  private:
    resource::Material *NextMaterial(const FVec3 &color, Float emissive, Float gloss, Float mode,
                                     Float sun, Float alpha);
    resource::Material *CreateMaterial(Bool transparent);
    resource::Mesh *CreateMesh(const std::vector<Float> &vertices, const std::vector<UInt32> &indices);

    resource::AssetProxy<resource::ShaderProgram> shader;
    const Arena *arena = nullptr;
    int bakedArena = -1;
    resource::Mesh *levelMesh = nullptr;
    resource::Mesh *skyMesh = nullptr;
    std::vector<resource::Mesh *> shapes;
    std::vector<std::unique_ptr<resource::Material>> materials;
    std::vector<std::unique_ptr<resource::Material>> glowMaterials;
    size_t materialsUsed = 0;
    size_t glowMaterialsUsed = 0;
    std::vector<DynamicLight> lights;
    std::vector<Mat4> lightData;
    std::vector<Mat4> themeData;
    Mat4 viewProjection;
    FVec3 cameraPosition;
    Float time = 0.0f;
    ArenaTheme theme;
};
} // namespace evo
