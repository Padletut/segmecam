#version 330 core

// Simple 2D texture shader for compositing
// Used for supersampling: blending high-res AR onto camera video

layout (location = 0) in vec2 aPosition;  // Vertex position in NDC (-1 to 1)
layout (location = 1) in vec2 aTexCoord;  // Texture coordinates (0 to 1)

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
    TexCoord = aTexCoord;
}
