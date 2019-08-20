#version 330

uniform sampler2D u_tex;
uniform vec4 u_color;

in r_texcoord;

out o_color;

void main() {
    o_color = texture(u_tex, r_texcoord) * u_color;
}