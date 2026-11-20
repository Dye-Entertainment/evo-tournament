#pragma once
#include "game/arena.h"
#include "game/math_util.h"
#include "primitive/fvec2.h"
#include "primitive/mat4.h"
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
    SPHERE
};

class SceneRenderer final
{
  public:
    SceneRenderer();
    ~SceneRenderer();
    Bool Initialize(const Arena &arena);
    void BeginFrame();
    void SetCamera(const FVec3 &eye, Float yaw, Float pitch, Float fov_degrees, Float aspect);
    void DrawLevel();
    void DrawSky();
    void DrawShape(MeshShape shape, const FVec3 &center, const FVec3 &size, const FVec3 &color,
                   Float emissive = 0.0f, Float yaw = 0.0f, Float pitch = 0.0f, Float roll = 0.0f);
    void DrawBeam(const FVec3 &start, const FVec3 &end, Float width, const FVec3 &color,
                  Float emissive);
    [[nodiscard]]
    Bool Project(const FVec3 &world, FVec2 &screen, Float screen_width, Float screen_height) const;

    [[nodiscard]]
    const FVec3 &CameraPosition() const
    {
        return cameraPosition;
    }

  private:
    resource::Material *NextMaterial(const FVec3 &color, Float emissive);
    resource::Mesh *CreateMesh(const std::vector<Float> &vertices, const std::vector<UInt32> &indices);

    resource::AssetProxy<resource::ShaderProgram> shader;
    resource::Mesh *levelMesh = nullptr;
    resource::Mesh *cubeMesh = nullptr;
    resource::Mesh *sphereMesh = nullptr;
    resource::Mesh *skyMesh = nullptr;
    std::vector<std::unique_ptr<resource::Material>> materials;
    size_t materialsUsed = 0;
    Mat4 viewProjection;
    FVec3 cameraPosition;
};
} // namespace evo
