#version 440
// Source-facing, projected chromatic material with a stationary reflected rim.
// All samples are from a low-resolution blur shared by adjacent panes.
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 sourceSize;
    vec4 samplingRect;
    vec4 surfaceParameters;  // width, height, corner radius, Lame exponent
    vec4 materialParameters; // strength, border inset, reach, sample depth
    vec4 edgeParameters;     // stroke strength, width, inset, light polarity
    float edgeMode;          // bottom divider or full perimeter
};
layout(binding = 1) uniform sampler2D source;

float superellipseDistance(vec2 point, vec2 extent, float radius, float power)
{
    vec2 halfExtent = extent * 0.5;
    float r = clamp(radius, 0.0, min(halfExtent.x, halfExtent.y));
    vec2 q = abs(point - halfExtent) - (halfExtent - vec2(r));
    if (q.x > 0.0 && q.y > 0.0 && r > 0.0) {
        float scale = max(r, max(q.x, q.y));
        vec2 unit = q / scale;
        float p = max(2.0, power);
        float normP = pow(pow(unit.x, p) + pow(unit.y, p), 1.0 / p);
        float gradient = length(pow(unit, vec2(p - 1.0)))
            / max(pow(normP, p - 1.0), 0.0001);
        return (normP * scale - r) / max(gradient, 0.55);
    }
    return max(q.x, q.y) - r;
}

float sourceFacingWeight(vec2 localUv, vec4 receiverRect, vec2 size)
{
    vec2 center = receiverRect.xy + receiverRect.zw * 0.5;
    vec2 towardSource = clamp(center, vec2(0.0), size) - center;
    float separation = length(towardSource);
    if (separation < 0.001) return 1.0;
    vec2 direction = towardSource / separation;
    vec2 halfExtent = max(abs(receiverRect.zw) * 0.5, vec2(0.001));
    float span = dot(abs(direction), halfExtent);
    float facing = dot((localUv - 0.5) * abs(receiverRect.zw), direction) / span;
    return smoothstep(0.0, 1.0, facing);
}

void main()
{
    vec2 samplePoint = samplingRect.xy + qt_TexCoord0 * samplingRect.zw;
    vec2 size = max(sourceSize, vec2(1.0));
    vec2 depth = min(vec2(max(0.0, materialParameters.w)), size * 0.5);
    vec2 projected = clamp(samplePoint, vec2(0.0), size);
    if (samplePoint.x < 0.0) projected.x = depth.x;
    else if (samplePoint.x > size.x) projected.x = size.x - depth.x;
    if (samplePoint.y < 0.0) projected.y = depth.y;
    else if (samplePoint.y > size.y) projected.y = size.y - depth.y;
    vec4 sampled = texture(source, projected / size);
    vec3 color = clamp(sampled.rgb / max(sampled.a, 0.0001), 0.0, 1.0);
    float chroma = max(color.r, max(color.g, color.b))
        - min(color.r, min(color.g, color.b));
    float chromaticCoverage = smoothstep(0.025, 0.16, chroma) * sampled.a;
    vec2 outside = samplePoint - clamp(samplePoint, vec2(0.0), size);
    float distanceFade = exp(-length(outside) / max(1.0, materialParameters.z));

    float inset = materialParameters.y;
    vec2 extent = max(surfaceParameters.xy - vec2(2.0 * inset), vec2(0.001));
    float sdf = superellipseDistance(qt_TexCoord0 * surfaceParameters.xy - vec2(inset),
        extent, max(0.0, surfaceParameters.z - inset * 0.5), surfaceParameters.w);
    float aa = max(fwidth(sdf), 0.001);
    float mask = 1.0 - smoothstep(-0.5 * aa, 0.5 * aa, sdf);
    float facing = sourceFacingWeight(qt_TexCoord0, samplingRect, size);
    float alpha = materialParameters.x * chromaticCoverage
        * distanceFade * facing * mask * qt_Opacity;

    if (edgeParameters.x <= 0.0 || edgeParameters.y <= 0.0) {
        fragColor = vec4(color * alpha, alpha);
        return;
    }

    float edgeInset = min(edgeParameters.z,
        min(surfaceParameters.x, surfaceParameters.y) * 0.5);
    vec2 edgeExtent = max(surfaceParameters.xy - vec2(2.0 * edgeInset), vec2(0.001));
    vec2 pixel = qt_TexCoord0 * surfaceParameters.xy;
    float edgeSdf = edgeMode > 0.5
        ? pixel.y - (surfaceParameters.y - edgeInset)
        : superellipseDistance(pixel - vec2(edgeInset), edgeExtent,
                               surfaceParameters.z, surfaceParameters.w);
    float edgeAA = max(length(vec2(dFdx(edgeSdf), dFdy(edgeSdf))), 0.001);
    float rim = 1.0 - smoothstep(edgeParameters.y * 0.5 - edgeAA * 0.5,
                                edgeParameters.y * 0.5 + edgeAA * 0.5, abs(edgeSdf));
    float minimum = min(color.r, min(color.g, color.b));
    vec3 hue = clamp((color - vec3(minimum)) / max(chroma, 0.0001), 0.0, 1.0);
    vec3 edgeColor = hue * mix(0.62, 0.96, edgeParameters.w);
    float edgeAlpha = edgeParameters.x * rim * smoothstep(0.025, 0.10, chroma)
        * sampled.a * distanceFade * facing * qt_Opacity;
    fragColor = vec4(edgeColor * edgeAlpha + color * alpha * (1.0 - edgeAlpha),
                     edgeAlpha + alpha * (1.0 - edgeAlpha));
}
