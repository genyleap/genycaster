#version 440
// Adapted from the user-supplied Genyleap Neon UI Kit v0.4.5.
// Continuous n=4 superellipse-edge reflection with a soft, localized glint.
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 surfaceParameters;
    vec4 focusParameters;
    vec4 lightParameters;
    vec4 tintParameters;
    vec4 cornerFlags;
    vec2 environmentSize;
    vec4 environmentRect;
    vec4 environmentParameters; // hue weight, reach, polarity, sample depth
    float steadyActivity;
};
layout(binding = 1) uniform sampler2D environmentSource;
float superellipseDistance(vec2 point, vec2 extent, float radius, float power,
                           vec4 roundedCorners)
{
    vec2 halfExtent = extent * 0.5;
    // [top-left, top-right, bottom-right, bottom-left]; use the same
    // per-corner shape as the retained QSG Superellipse. A zero-radius
    // corner produces two uninterrupted straight edges, not a masked arc.
    float enabledCorner = point.y < halfExtent.y
        ? (point.x < halfExtent.x ? roundedCorners.x : roundedCorners.y)
        : (point.x >= halfExtent.x ? roundedCorners.z : roundedCorners.w);
    float r = clamp(radius * enabledCorner, 0.0, min(halfExtent.x, halfExtent.y));
    vec2 q = abs(point - halfExtent) - (halfExtent - vec2(r));
    if (q.x > 0.0 && q.y > 0.0 && r > 0.0) {
        float p = max(2.0, power);
        float x = pow(q.x, p);
        float y = pow(q.y, p);
        float normP = pow(x + y, 1.0 / p);
        vec2 derivative = vec2(pow(q.x, p - 1.0), pow(q.y, p - 1.0));
        float gradient = length(derivative) / max(pow(normP, p - 1.0), 0.0001);
        return (normP - r) / max(gradient, 0.55);
    }
    return max(q.x, q.y) - r;
}
// Color only the half of the rim facing the source, with the same spatial
// falloff as the projected body reflection. Neutral hover always remains.
float sourceFacingWeight(vec2 uv, vec4 receiver, vec2 size)
{
    vec2 center = receiver.xy + receiver.zw * 0.5;
    vec2 towardSource = clamp(center, vec2(0.0), size) - center;
    float distance = length(towardSource);
    if (distance < 0.001) return 1.0;
    vec2 direction = towardSource / distance;
    float span = dot(abs(direction), max(abs(receiver.zw) * 0.5, vec2(0.001)));
    float facing = dot((uv - 0.5) * abs(receiver.zw), direction) / span;
    return smoothstep(0.0, 1.0, facing);
}

vec4 reflectedEdgeMaterial(vec3 baseColor, out float colorWeight)
{
    colorWeight = 0.0;
    if (environmentParameters.x <= 0.0)
        return vec4(baseColor, 1.0);
    vec2 size = max(environmentSize, vec2(1.0));
    vec2 point = environmentRect.xy + qt_TexCoord0 * environmentRect.zw;
    vec2 depth = min(vec2(max(0.0, environmentParameters.w)), size * 0.5);
    vec2 projected = clamp(point, vec2(0.0), size);
    if (point.x < 0.0) projected.x = depth.x;
    else if (point.x > size.x) projected.x = size.x - depth.x;
    if (point.y < 0.0) projected.y = depth.y;
    else if (point.y > size.y) projected.y = size.y - depth.y;

    vec4 sampled = texture(environmentSource, projected / size);
    vec3 color = clamp(sampled.rgb / max(sampled.a, 0.0001), 0.0, 1.0);
    float minimum = min(color.r, min(color.g, color.b));
    float maximum = max(color.r, max(color.g, color.b));
    float chroma = maximum - minimum;
    float confidence = smoothstep(0.025, 0.10, chroma) * sampled.a;
    float facing = sourceFacingWeight(qt_TexCoord0, environmentRect, size);
    float contribution = clamp(environmentParameters.x * confidence * facing, 0.0, 1.0);
    vec3 hue = clamp((color - vec3(minimum)) / max(chroma, 0.0001), 0.0, 1.0);
    colorWeight = contribution;
    float distance = length(point - clamp(point, vec2(0.0), size));
    float distanceFade = exp(-distance / max(1.0, environmentParameters.y));
    return vec4(mix(baseColor, hue * mix(0.62, 0.96, environmentParameters.z),
                    contribution), mix(1.0, distanceFade, contribution));
}

void main()
{
    vec2 extent = max(surfaceParameters.xy, vec2(1.0));
    vec2 pixel = qt_TexCoord0 * extent;
    float inset = lightParameters.w;
    float sdf = superellipseDistance(pixel - vec2(inset),
        extent - vec2(2.0 * inset), surfaceParameters.z,
        surfaceParameters.w, cornerFlags);
    // Exact one-pixel superellipse rim: no broad inward halo or raised tube.
    // Pixel-footprint AA preserves subpixel edges at fractional DPR/scales.
    // The core covers a centered one-pixel stroke; the transition spans the
    // full derivative footprint instead of abruptly clipping rounded corners.
    float aa = max(fwidth(sdf), 0.001);
    float borderCore = 1.0 - smoothstep(max(0.0, 0.5 - aa * 0.5),
                                       0.5 + aa * 0.5, abs(sdf));

    vec2 delta = pixel - focusParameters.xy;
    float nearest = focusParameters.w;
    float tangentDistanceSq = max(0.0, dot(delta, delta) - nearest * nearest);
    float sigma = max(1.0, focusParameters.z);
    float shoulder = exp(-0.5 * tangentDistanceSq / (sigma * sigma));
    float glint = exp(-0.5 * tangentDistanceSq / (sigma * sigma * 0.16));
    float approach = exp(-0.5 * pow(nearest / max(1.0, lightParameters.z), 2.0));
    float localized = (0.65 * shoulder + 0.35 * glint) * approach;
    float ambient = mix(0.16, 0.30, 1.0 - qt_TexCoord0.y);
    float rim = mix(localized, ambient, steadyActivity);
    float alpha = clamp(borderCore * rim * lightParameters.y * qt_Opacity, 0.0, 1.0);
    if (alpha <= 0.0) { fragColor = vec4(0.0); return; }

    vec3 base = vec3(lightParameters.x);
    // Explicit semantic tint continues to win over neighboring artwork.
    float tint = clamp(tintParameters.a, 0.0, 1.0);
    base = mix(base, tintParameters.rgb, tint);
    float colorWeight;
    vec4 environment = reflectedEdgeMaterial(base, colorWeight);
    float headroom = clamp(lightParameters.y, 0.0, 1.0);
    float focusedGain = headroom * (1.0 - headroom)
        * mix(1.20, 0.70, environmentParameters.z)
        * glint * approach * colorWeight
        * (1.0 - clamp(steadyActivity, 0.0, 1.0))
        * (1.0 - tint);
    alpha = clamp(alpha + borderCore * qt_Opacity * focusedGain,
                  0.0, borderCore * qt_Opacity);
    alpha *= environment.a;
    vec3 finalColor = mix(environment.rgb, base, tint);
    fragColor = vec4(finalColor * alpha, alpha);
}
