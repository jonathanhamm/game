#version 400

uniform sampler2D tex;

in vec2 fragTexCoord;

out vec4 finalColor;

void main() {
    //finalColor = texture(tex, fragTexCoord);
    finalColor =  texture(tex, fragTexCoord) * vec4(0.5, 0.5, 1.0, 1.0);
		//finalColor = vec4(fragTexCoord, 0, 1) * vec4(0.5, 0.5, 1.0, 1.0);
}

