#version 330

uniform sampler2D u_tex;
uniform sampler2D u_mask;
uniform vec4 u_color;
uniform float invert;

in vec2 r_texcoord;

out vec4 o_color;

void main() {
    o_color = abs(vec4(invert) - texture(u_mask, r_texcoord)) * texture(u_tex, r_texcoord) *
        (u_color / 255.0);
}