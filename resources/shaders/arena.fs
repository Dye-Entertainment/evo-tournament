precision highp float;
in vec3 vWorldPosition;
in vec3 vNormal;
in vec4 vColor;
in vec4 vBake;

#include "engine://shaders/camera_data.glsl"

// rgb tint, a = how much the surface glows (0..1)
uniform vec4 uTint;
// x = gloss, y = mode (0 object, 1 sky, 2 glow, 3 shade, 4 level), z = sun visibility, w = alpha
uniform vec4 uMaterial;
// [0]: sky zenith + stars, sky horizon + aurora, sky ambient + clouds, ground ambient + embers
// [1]: sun direction + time, sun color + fog density, fog color
uniform mat4 uTheme[2];
// Two lights per matrix: (position, radius), (color, is_static)
uniform mat4 uLights[6];

out vec4 FragColor;

float Hash13(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

float Hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float Noise2(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = Hash12(i);
    float b = Hash12(i + vec2(1.0, 0.0));
    float c = Hash12(i + vec2(0.0, 1.0));
    float d = Hash12(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float Fbm(vec2 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; ++i)
    {
        value += Noise2(p) * amplitude;
        p = p * 2.03 + vec2(17.1, 9.2);
        amplitude *= 0.5;
    }
    return value;
}

vec3 Tonemap(vec3 color)
{
    // ACES fit, with a little exposure so mid tones stay bright.
    color *= 0.95;
    vec3 mapped = (color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14);
    return clamp(mapped, 0.0, 1.0);
}

vec3 SkyColor(vec3 direction)
{
    vec3 zenith = uTheme[0][0].rgb;
    vec3 horizon = uTheme[0][1].rgb;
    float stars = uTheme[0][0].a;
    float aurora = uTheme[0][1].a;
    float clouds = uTheme[0][2].a;
    vec3 sun_direction = uTheme[1][0].xyz;
    float time = uTheme[1][0].w;
    vec3 sun_color = uTheme[1][1].rgb;
    vec3 fog_color = uTheme[1][2].rgb;

    float up = direction.y;
    vec3 color = mix(horizon, zenith, pow(clamp(up, 0.0, 1.0), 0.55));
    color = mix(color, fog_color * 0.8, clamp(-up * 4.0, 0.0, 1.0));

    float sun_amount = max(dot(direction, sun_direction), 0.0);
    color += sun_color * (pow(sun_amount, 6.0) * 0.35 + pow(sun_amount, 64.0) * 0.6);
    color += sun_color * smoothstep(0.9993, 0.9997, sun_amount) * 3.0;

    if (stars > 0.0 && up > 0.0)
    {
        vec3 cell = floor(direction * 160.0);
        vec3 local = fract(direction * 160.0) - 0.5;
        float h = Hash13(cell);
        float star = step(0.985, h) * smoothstep(0.28, 0.0, length(local));
        float twinkle = 0.6 + 0.4 * sin(time * 3.0 + h * 80.0);
        color += vec3(0.9, 0.95, 1.0) * star * twinkle * stars * smoothstep(0.0, 0.25, up) * 1.4;
    }

    if (aurora > 0.0 && up > 0.05)
    {
        vec2 sky_plane = direction.xz / (up + 0.25);
        float curtain = 0.0;
        for (int i = 0; i < 3; ++i)
        {
            float fi = float(i);
            float wave = sin(sky_plane.x * (1.2 + fi * 0.5) + time * (0.15 + fi * 0.07) +
                             sin(sky_plane.y * 1.7 + time * 0.2 + fi) * 1.5);
            float band = smoothstep(0.35, 0.0, abs(sky_plane.y * 0.6 + wave * 0.35 - 0.4 + fi * 0.45));
            curtain += band * (0.6 + 0.4 * Noise2(sky_plane * vec2(3.0, 0.4) + vec2(time * 0.3, fi)));
        }
        float fade = smoothstep(0.05, 0.3, up) * (1.0 - smoothstep(0.75, 1.0, up));
        vec3 aurora_color = mix(vec3(0.15, 1.0, 0.55), vec3(0.55, 0.3, 1.0), smoothstep(0.2, 0.8, up));
        color += aurora_color * curtain * fade * aurora * 0.55;
    }

    if (clouds > 0.0 && up > -0.05)
    {
        vec2 cloud_plane = direction.xz / (max(up, 0.0) + 0.12) * 0.6 + vec2(time * 0.01, time * 0.004);
        float density = smoothstep(0.45, 0.85, Fbm(cloud_plane));
        float lit = 0.55 + 0.45 * sun_amount;
        vec3 cloud_color = mix(zenith * 1.5 + horizon * 0.3, horizon * 1.1 + sun_color * 0.3, lit);
        float fade = smoothstep(-0.05, 0.12, up) * (1.0 - smoothstep(0.5, 0.95, up));
        color = mix(color, cloud_color, density * fade * clouds * 0.75);
    }
    return color;
}

// Panels, seams, per-panel tone and grime for level surfaces. Fine detail fades with distance
// so it does not shimmer.
vec3 PanelDetail(vec3 base, vec3 normal, vec3 position, float view_distance, out float panel_gloss)
{
    vec3 absolute_normal = abs(normal);
    bool floor_like = absolute_normal.y > 0.5;
    vec2 plane = floor_like ? position.xz : (absolute_normal.x > 0.5 ? position.zy : position.xy);
    vec2 size = floor_like ? vec2(2.0, 2.0) : vec2(2.0, 1.0);
    vec2 scaled = plane / size;
    if (!floor_like)
    {
        // Offset every other row like brickwork.
        scaled.x += 0.5 * mod(floor(scaled.y), 2.0);
    }
    vec2 cell = floor(scaled);
    vec2 local = fract(scaled);
    float detail = clamp(1.0 - view_distance / 70.0, 0.0, 1.0);

    vec2 edge = min(local, 1.0 - local) * size;
    float seam = (1.0 - smoothstep(0.012, 0.045, min(edge.x, edge.y))) * detail;
    float bevel_light = (1.0 - smoothstep(0.0, 0.09, local.y * size.y)) * detail;
    float bevel_dark = (1.0 - smoothstep(0.0, 0.09, (1.0 - local.y) * size.y)) * detail;

    float cell_hash = Hash12(cell + floor(position.y * 0.01));
    float tone = 1.0 + (cell_hash - 0.5) * 0.16;
    float grime = mix(0.82, 1.06, Fbm(plane * 0.3));
    float grain = 1.0 + (Noise2(plane * 7.0) - 0.5) * 0.10 * detail;
    // Occasional worn or stained panel.
    float stain = smoothstep(0.55, 0.9, Noise2(plane * 0.6 + 13.0)) * 0.18;

    vec3 color = base * tone * grime * grain * (1.0 - stain);
    color *= 1.0 - seam * 0.55;
    color += base * (bevel_light * 0.10 - bevel_dark * 0.08);
    // Wide structural bands every 8 m.
    vec2 band_cell = abs(fract(plane / 8.0) - 0.5);
    float band = smoothstep(0.5 - 0.012, 0.5, max(band_cell.x, band_cell.y)) * detail;
    color *= 1.0 - band * 0.25;

    panel_gloss = mix(0.12, 0.55, cell_hash) * (1.0 - stain * 2.0);
    return color;
}

vec3 PointLight(vec4 position_radius, vec3 color, vec3 world, vec3 normal, vec3 view_direction,
                float gloss, inout vec3 specular)
{
    if (position_radius.w < 0.01)
    {
        return vec3(0.0);
    }
    vec3 to_light = position_radius.xyz - world;
    float distance_squared = dot(to_light, to_light);
    float radius_squared = position_radius.w * position_radius.w;
    if (distance_squared >= radius_squared)
    {
        return vec3(0.0);
    }
    vec3 direction = to_light * inversesqrt(max(distance_squared, 0.0001));
    float falloff = 1.0 - distance_squared / radius_squared;
    falloff *= falloff;
    float facing = max(dot(normal, direction), 0.0);
    vec3 half_vector = normalize(direction - view_direction);
    specular += color * pow(max(dot(normal, half_vector), 0.0), mix(12.0, 90.0, gloss)) * gloss * falloff * 1.5;
    return color * facing * falloff * 1.4;
}

void main()
{
    float mode = uMaterial.y;
    vec3 view_vector = vWorldPosition - uViewPosition;
    float view_distance = length(view_vector);
    vec3 view_direction = view_vector / max(view_distance, 0.0001);

    if (mode > 0.5 && mode < 1.5)
    {
        FragColor = vec4(Tonemap(SkyColor(view_direction)), 1.0);
        return;
    }

    vec3 normal = normalize(vNormal);

    vec3 fog_color = uTheme[1][2].rgb;
    float fog_density = uTheme[1][1].a;
    vec3 sun_direction = uTheme[1][0].xyz;
    vec3 sun_color = uTheme[1][1].rgb;
    float fog = 1.0 - exp(-view_distance * fog_density);
    // Thinner fog higher up.
    fog *= clamp(1.15 - max(vWorldPosition.y, 0.0) * 0.025, 0.55, 1.0);
    vec3 fog_tint = fog_color + sun_color * pow(max(dot(view_direction, sun_direction), 0.0), 6.0) * 0.2;

    if (mode > 1.5 && mode < 2.5)
    {
        // Soft glowing ball: bright in the middle, fading at the rim.
        float facing = abs(dot(normal, view_direction));
        float alpha = uMaterial.w * pow(facing, 1.6);
        vec3 glow = uTint.rgb * (1.2 + facing);
        FragColor = vec4(Tonemap(mix(glow, fog_tint, fog * 0.4)), clamp(alpha, 0.0, 1.0));
        return;
    }
    if (mode > 2.5 && mode < 3.5)
    {
        // Soft contact shadow; the red channel is 1 in the middle and 0 at the rim.
        float alpha = uMaterial.w * smoothstep(0.0, 1.0, vColor.r) * (1.0 - fog);
        FragColor = vec4(uTint.rgb, alpha);
        return;
    }

    float code = vColor.a;
    float kind = mode > 3.5 ? floor(code + 0.001) : 0.0;
    float ambient_occlusion = clamp((code - kind) / 0.98, 0.0, 1.0);
    vec3 base = vColor.rgb * uTint.rgb;
    float emissive = max(clamp(uTint.a, 0.0, 1.0), kind > 1.5 ? 1.0 : 0.0);
    float gloss = uMaterial.x;
    if (kind > 0.5 && kind < 1.5)
    {
        float panel_gloss = 0.3;
        base = PanelDetail(base, normal, vWorldPosition, view_distance, panel_gloss);
        gloss = panel_gloss;
    }

    bool level = mode > 3.5;
    float sun_visibility = (level ? vBake.a : 1.0) * uMaterial.z;
    vec3 sky_ambient = uTheme[0][2].rgb;
    vec3 ground_ambient = uTheme[0][3].rgb;
    vec3 ambient = mix(ground_ambient, sky_ambient, 0.5 + 0.5 * normal.y) * ambient_occlusion;
    float sun_facing = max(dot(normal, sun_direction), 0.0);
    vec3 diffuse = sun_color * sun_facing * sun_visibility;
    vec3 half_vector = normalize(sun_direction - view_direction);
    vec3 specular = sun_color * pow(max(dot(normal, half_vector), 0.0), mix(10.0, 110.0, gloss)) *
                    gloss * sun_visibility * 0.9;
    vec3 baked = level ? vBake.rgb : vec3(0.0);

    vec3 lights = vec3(0.0);
    for (int i = 0; i < 6; ++i)
    {
        mat4 pair = uLights[i];
        if (!(level && pair[1].w > 0.5))
        {
            lights += PointLight(pair[0], pair[1].rgb, vWorldPosition, normal, view_direction, gloss, specular);
        }
        if (!(level && pair[3].w > 0.5))
        {
            lights += PointLight(pair[2], pair[3].rgb, vWorldPosition, normal, view_direction, gloss, specular);
        }
    }

    vec3 lit = base * (ambient + diffuse + baked + lights) + specular;
    if (!level)
    {
        // A cool rim light keeps characters readable against dark backgrounds.
        float rim = pow(1.0 - max(dot(normal, -view_direction), 0.0), 3.0);
        lit += sky_ambient * rim * 0.45;
    }
    vec3 color = mix(lit, base * 1.3, emissive);
    color = mix(color, fog_tint, fog * (emissive > 0.5 ? 0.55 : 0.92));
    FragColor = vec4(Tonemap(color), 1.0);
}
