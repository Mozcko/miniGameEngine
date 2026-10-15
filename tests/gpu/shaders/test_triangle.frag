#version 450
// Color desde un uniform del fragmento: set 3, binding 0 (convención de SDL GPU)
layout(set = 3, binding = 0) uniform Params {
    vec4 color;
};
layout(location = 0) out vec4 outColor;

void main() {
    outColor = color;
}
