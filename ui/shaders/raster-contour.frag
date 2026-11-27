#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 qt_SubRect_source;
    vec4 contour; // painted width, height, radius, exponent
    vec2 uvScale; // centered crop in source-local coordinates
    float pixelWidth;
};
layout(binding = 1) uniform sampler2D source;

float contourDistance(vec2 point, vec2 extent, float radius, float power)
{
    vec2 halfExtent = extent * 0.5;
    float r = clamp(radius, 0.0, min(halfExtent.x, halfExtent.y));
    vec2 q = abs(point - halfExtent) - (halfExtent - vec2(r));
    if (q.x > 0.0 && q.y > 0.0 && r > 0.0) {
        float scale = max(r, max(q.x, q.y));
        vec2 u = q / scale;
        float p = max(2.0, power);
        float n = pow(pow(u.x, p) + pow(u.y, p), 1.0 / p);
        float g = length(pow(u, vec2(p - 1.0))) / max(pow(n, p - 1.0), 0.0001);
        return (n * scale - r) / max(g, 0.55);
    }
    return max(q.x, q.y) - r;
}

void main()
{
    vec2 uv = (qt_TexCoord0 - vec2(0.5)) * uvScale + vec2(0.5);
    vec2 atlasUv = qt_SubRect_source.xy + uv * qt_SubRect_source.zw;
    float distance = contourDistance(qt_TexCoord0 * contour.xy, contour.xy,
                                     contour.z, contour.w);
    float aa = max(pixelWidth, 0.001);
    float coverage = 1.0 - smoothstep(-0.5 * aa, 0.5 * aa, distance);
    fragColor = texture(source, atlasUv) * (coverage * qt_Opacity);
}
