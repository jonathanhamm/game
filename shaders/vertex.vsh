#version 400

uniform mat4 camera;
uniform mat4 model;

in vec3 vert;
in vec2 vertexCoord;

out vec2 fragTexCoord;


void main() {
    fragTexCoord = vertexCoord;
    gl_Position = camera * model * vec4(vert, 1);
}

