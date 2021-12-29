#version 150


in vec3 vert;
//in vec2 vertexCoord;

out vec2 fragTexCoord;

void main() {
	//fragTexCoord = vertexCoord;
	gl_Position = vec4(vert, 1);
}

