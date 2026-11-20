precision highp float;
in vec3 vWorldPosition;
in vec3 vNormal;
in vec4 vColor;

#include "engine://shaders/camera_data.glsl"

uniform vec4 uTint;

out vec4 FragColor;

float GridLine(vec2 coordinates, float scale, float width)
{
    vec2 cell = abs(fract(coordinates / scale) - 0.5);
    float edge = max(cell.x, cell.y);
    return smoothstep(0.5 - width, 0.5, edge);
}

void main()
{
    vec3 normal = normalize(vNormal);
    vec3 base = vColor.rgb * uTint.rgb;
    float emissive = clamp(uTint.a, 0.0, 1.0);

    vec3 absolute_normal = abs(normal);
    vec2 plane = absolute_normal.y > 0.5 ? vWorldPosition.xz
               : (absolute_normal.x > 0.5 ? vWorldPosition.zy : vWorldPosition.xy);
    float fine = GridLine(plane, 1.0, 0.03);
    float coarse = GridLine(plane, 4.0, 0.012);
    float grid = clamp(fine * 0.35 + coarse * 0.8, 0.0, 1.0) * vColor.a;
    base *= 1.0 - grid * 0.35;
    base += vec3(0.05, 0.12, 0.16) * coarse * vColor.a;

    vec3 sun_direction = normalize(vec3(0.35, 0.85, 0.25));
    float sun = max(dot(normal, sun_direction), 0.0);
    float sky = 0.5 + 0.5 * normal.y;
    vec3 lit = base * (vec3(0.30, 0.32, 0.40) * sky + vec3(0.20, 0.17, 0.15) + vec3(0.85, 0.78, 0.68) * sun);

    vec3 color = mix(lit, base * 1.25, emissive);

    float distance_to_eye = length(vWorldPosition - uViewPosition);
    float fog = 1.0 - exp(-distance_to_eye * 0.012);
    vec3 fog_color = vec3(0.32, 0.24, 0.30);
    if (uTint.a < 1.5)
    {
        color = mix(color, fog_color, fog * 0.55);
    }
    FragColor = vec4(color, 1.0);
}
