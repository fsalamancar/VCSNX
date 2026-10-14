#version 450

// VCSNX: adds the quarter-resolution glow (shaders/bloom.frag's output) onto the frame on the GPU, with the
// sampler's bilinear filter as the upsample. The pipeline blends ONE + ONE on RGB, so this is the same
// additive composite the CPU used to do after reading both images back, without the readback or the loop.
layout(set = 0, binding = 0) uniform sampler2D bloom_glow;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(texture(bloom_glow, v_uv).rgb, 0.0);
}
