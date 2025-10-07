#version 330 core

// Vertex attributes
layout(location = 0) in vec3 aPos;       // Vertex position
layout(location = 1) in vec3 aNormal;    // Vertex normal
layout(location = 2) in vec2 aTexCoord;  // Texture coordinate

// Uniforms - Transformation matrices
uniform mat4 uModel;       // Model matrix (local to world)
uniform mat4 uView;        // View matrix (world to camera)
uniform mat4 uProjection;  // Projection matrix (camera to clip space)
uniform mat3 uNormalMatrix; // Normal transformation matrix (inverse transpose of model)

// Outputs to fragment shader
out vec3 FragPos;    // Fragment position in world space
out vec3 Normal;     // Fragment normal in world space
out vec2 TexCoord;   // Texture coordinate

void main() {
    // Transform vertex position to world space
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    
    // Transform normal to world space (handles non-uniform scaling)
    Normal = normalize(uNormalMatrix * aNormal);
    
    // Pass texture coordinate through
    TexCoord = aTexCoord;
    
    // Final position in clip space
    gl_Position = uProjection * uView * worldPos;
}
