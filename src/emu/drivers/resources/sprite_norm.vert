#version 330

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec2 in_texcoord;

out vec2 r_texcoord;
out vec4 r_color;

uniform mat4 u_projection;
uniform mat4 u_model;

void main() {
    gl_Position = u_projection * u_model * vec4(in_position.xy, 0, 0);
    r_texcoord = in_texcoord;
}