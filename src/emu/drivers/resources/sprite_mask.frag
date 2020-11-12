#version 330

uniform sampler2D u_tex;
uniform sampler2D u_mask;
uniform float u_invert;
uniform vec4 u_color;

in vec2 r_texcoord;

out vec4 o_color;

void main() {
    float mask_value = abs(u_invert - texture(u_mask, r_texcoord).r);

    o_color =  texture(u_tex, r_texcoord) * (u_color / 255.0);
    o_color.a = o_color.a * mask_value;
}