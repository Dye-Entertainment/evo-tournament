precision highp float;
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec4 aBake;

#include "engine://shaders/camera_data.glsl"

uniform mat4 uModel;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec4 vColor;
out vec4 vBake;

void main()
{
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorldPosition = world.xyz;
    vNormal = mat3(uModel) * aNormal;
    vColor = aColor;
    vBake = aBake;
    gl_Position = uProjection * uView * world;
}
