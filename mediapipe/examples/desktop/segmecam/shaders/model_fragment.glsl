#version 330 core

// Inputs from vertex shader
in vec3 FragPos;   // Fragment position in world space
in vec3 Normal;    // Normal in world space
in vec2 TexCoord;  // Texture coordinate

// Uniforms - Camera and lighting
uniform vec3 uCameraPos;        // Camera position in world space
uniform vec3 uLightDir;         // Directional light direction (normalized)
uniform vec3 uLightColor;       // Light color
uniform vec3 uAmbientColor;     // Ambient light color

// Uniforms - Material properties (from MTL file)
uniform vec3 uMaterialAmbient;  // Ka - Ambient reflectivity
uniform vec3 uMaterialDiffuse;  // Kd - Diffuse reflectivity
uniform vec3 uMaterialSpecular; // Ks - Specular reflectivity
uniform float uMaterialShininess; // Ns - Specular exponent (shininess)
uniform float uMaterialOpacity;   // d - Opacity (1.0 = opaque, 0.0 = transparent)

// Output
out vec4 FragColor;

void main() {
    // Normalize interpolated vectors
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(-uLightDir); // Negate to point FROM surface TO light
    vec3 viewDir = normalize(uCameraPos - FragPos);
    
    // === AMBIENT COMPONENT ===
    // Ambient light is constant, independent of view/light direction
    vec3 ambient = uAmbientColor * uMaterialAmbient;
    
    // === DIFFUSE COMPONENT (Lambert) ===
    // Diffuse light depends on angle between normal and light direction
    // max() prevents negative values when light is behind surface
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = uLightColor * (diff * uMaterialDiffuse);
    
    // === SPECULAR COMPONENT (Blinn-Phong) ===
    // Specular highlights depend on view direction
    // Blinn-Phong uses halfway vector for better performance than Phong
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), uMaterialShininess);
    vec3 specular = uLightColor * (spec * uMaterialSpecular);
    
    // === COMBINE ALL LIGHTING COMPONENTS ===
    vec3 result = ambient + diffuse + specular;
    
    // Apply opacity (alpha channel)
    FragColor = vec4(result, uMaterialOpacity);
}
