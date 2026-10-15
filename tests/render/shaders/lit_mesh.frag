#version 450
layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inColor;
layout(location = 0) out vec4 outColor;

void main() {
    const vec3 toLight = normalize(vec3(0.4, 1.0, 0.6));
    float diffuse = max(dot(normalize(inNormal), toLight), 0.0);
    outColor = vec4(inColor.rgb * (0.25 + 0.75 * diffuse), inColor.a);
}
