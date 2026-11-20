precision highp float;
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;

#include "engine://shaders/camera_data.glsl"

uniform mat4 uModel;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec4 vColor;

void main()
{
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorldPosition = world.xyz;
    vNormal = mat3(uModel) * aNormal;
    vColor = aColor;
    gl_Position = uProjection * uView * world;
}
