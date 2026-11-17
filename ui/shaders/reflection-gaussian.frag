#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 sampleStep;
};
layout(binding = 1) uniform sampler2D source;

// Reject weak visible chroma before the blur. Alpha-unpremultiplication of
// almost transparent text can otherwise create spurious colored edges.
vec4 chromaticSample(vec2 uv)
{
    vec4 pixel = texture(source, uv);
    float visibleChroma = max(pixel.r, max(pixel.g, pixel.b))
                        - min(pixel.r, min(pixel.g, pixel.b));
    return pixel * smoothstep(0.08, 0.22, visibleChroma);
}

void main()
{
    vec4 result = chromaticSample(qt_TexCoord0);
    float total = 1.0;
    for (int i = 1; i <= 47; i += 2) {
        float first = exp(-float(i * i) / 544.5);
        float second = exp(-float((i + 1) * (i + 1)) / 544.5);
        float weight = first + second;
        vec2 offset = (float(i) + second / weight) * sampleStep;
        result += chromaticSample(clamp(qt_TexCoord0 + offset, vec2(0.0), vec2(1.0))) * weight;
        result += chromaticSample(clamp(qt_TexCoord0 - offset, vec2(0.0), vec2(1.0))) * weight;
        total += 2.0 * weight;
    }
    fragColor = (result / total) * qt_Opacity;
}
