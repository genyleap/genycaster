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
    float steadyActivity;
};
float superellipseDistance(vec2 point, vec2 extent, float radius, float power)
{
    vec2 halfExtent = extent * 0.5;
    float r = clamp(radius, 0.0, min(halfExtent.x, halfExtent.y));
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
void main()
{
    vec2 extent = max(surfaceParameters.xy, vec2(1.0));
    vec2 pixel = qt_TexCoord0 * extent;
    float inset = lightParameters.w;
    float sdf = superellipseDistance(pixel - vec2(inset), extent - vec2(2.0 * inset),
                                     surfaceParameters.z, surfaceParameters.w);
    float aa = max(fwidth(sdf), 0.001);
    float borderCore = 1.0 - smoothstep(0.5 - 0.5 * aa, 0.5 + 0.5 * aa, abs(sdf));
    float insideFeather = exp(-0.5 * pow((sdf + 1.0) / 1.15, 2.0))
                         * (1.0 - smoothstep(-0.5, 0.5, sdf)) * 0.16;
    vec2 delta = pixel - focusParameters.xy;
    float nearest = focusParameters.w;
    float tangentDistanceSq = max(0.0, dot(delta, delta) - nearest * nearest);
    float sigma = max(1.0, focusParameters.z);
    float shoulder = exp(-0.5 * tangentDistanceSq / (sigma * sigma));
    float glint = exp(-0.5 * tangentDistanceSq / (sigma * sigma * 0.16));
    float approach = exp(-0.5 * pow(nearest / max(1.0, lightParameters.z), 2.0));
    float localized = (0.65 * shoulder + 0.35 * glint) * approach;
    float rim = mix(localized, 0.32, steadyActivity);
    float feather = mix(shoulder * approach, 0.32, steadyActivity);
    float alpha = (borderCore * rim + insideFeather * feather)
                  * lightParameters.y * qt_Opacity;
    alpha = clamp(alpha, 0.0, 1.0);
    vec3 baseColor = vec3(lightParameters.x);
    vec3 haloColor = mix(baseColor, tintParameters.rgb, clamp(tintParameters.a, 0.0, 1.0));
    fragColor = vec4(haloColor * alpha, alpha);
}
