#version 330 core

// Simple 2D texture shader for compositing
// Used for supersampling: blending high-res AR onto camera video

in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    // Sample the texture and preserve alpha for blending
    FragColor = texture(uTexture, TexCoord);
}
