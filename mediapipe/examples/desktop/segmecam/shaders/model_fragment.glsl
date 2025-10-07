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

// Uniforms - Texture support
uniform bool uHasTexture;       // Whether a diffuse texture is bound
uniform sampler2D uTexture;     // Diffuse texture (color map)
uniform bool uHasOpacityMap;    // Whether an opacity/alpha map is bound
uniform sampler2D uOpacityMap;  // Opacity map (alpha channel)
uniform bool uHasEmissiveMap;   // Whether an emissive map is bound
uniform sampler2D uEmissiveMap; // Emissive map (self-illumination)

// Output
out vec4 FragColor;

void main() {
    // Normalize interpolated vectors
    vec3 norm = normalize(Normal);  // Use normal as-is from vertex shader
    vec3 lightDir = normalize(-uLightDir); // Negate to point FROM surface TO light
    vec3 viewDir = normalize(uCameraPos - FragPos);
    
    // Sample texture color if available, otherwise use material diffuse
    vec4 textureColorWithAlpha = uHasTexture ? texture(uTexture, TexCoord) : vec4(1.0);
    vec3 textureColor = textureColorWithAlpha.rgb;
    vec3 materialDiffuse = uMaterialDiffuse * textureColor;
    
    // Sample opacity map if available, otherwise use material opacity
    float alpha = uMaterialOpacity;
    if (uHasOpacityMap) {
        alpha = texture(uOpacityMap, TexCoord).r;  // Use red channel for opacity
    } else if (uHasTexture) {
        // If no opacity map but texture has alpha, use it
        alpha *= textureColorWithAlpha.a;
    }
    
    // === AMBIENT COMPONENT ===
    // Ambient light is constant, independent of view/light direction
    vec3 ambient = uAmbientColor * (uMaterialAmbient * textureColor);
    
    // === DIFFUSE COMPONENT (Lambert) ===
    // Diffuse light depends on angle between normal and light direction
    // max() prevents negative values when light is behind surface
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = uLightColor * (diff * materialDiffuse);
    
    // === SPECULAR COMPONENT (Blinn-Phong) ===
    // Specular highlights depend on view direction
    // Blinn-Phong uses halfway vector for better performance than Phong
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), uMaterialShininess);
    vec3 specular = uLightColor * (spec * uMaterialSpecular);
    
    // === COMBINE ALL LIGHTING COMPONENTS ===
    vec3 result = ambient + diffuse + specular;
    
    // === ADD EMISSIVE (self-illumination) ===
    if (uHasEmissiveMap) {
        vec3 emissive = texture(uEmissiveMap, TexCoord).rgb;
        result += emissive;  // Emissive is added directly, not affected by lighting
    }
    
    // Apply opacity (alpha channel)
    FragColor = vec4(result, alpha);
}