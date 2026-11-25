#version 450
// Color from a fragment uniform: set 3, binding 0 (SDL GPU convention)
layout(set = 3, binding = 0) uniform Params {
    vec4 color;
};
layout(location = 0) out vec4 outColor;

void main() {
    outColor = color;
}
