#ifndef SEGMECAM_SHADER_PROGRAM_H
#define SEGMECAM_SHADER_PROGRAM_H

#include <string>
#include <epoxy/gl.h>  // Modern OpenGL function loader (provides OpenGL 3.3+ functions)

namespace segmecam {
namespace render {

/**
 * ShaderProgram class
 * Manages GLSL shader compilation, linking, and uniform variable access
 */
class ShaderProgram {
public:
    ShaderProgram();
    ~ShaderProgram();
    
    // Prevent copying (OpenGL resources shouldn't be copied)
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    
    /**
     * Load and compile shaders from source strings
     * @param vertex_source Vertex shader GLSL source code
     * @param fragment_source Fragment shader GLSL source code
     * @return true on success, false on failure
     */
    bool LoadFromStrings(const std::string& vertex_source,
                        const std::string& fragment_source);
    
    /**
     * Load and compile shaders from files
     * @param vertex_path Path to vertex shader file
     * @param fragment_path Path to fragment shader file
     * @return true on success, false on failure
     */
    bool LoadFromFiles(const std::string& vertex_path,
                      const std::string& fragment_path);
    
    /**
     * Use/activate this shader program for rendering
     */
    void Use() const;
    
    /**
     * Get the OpenGL program ID
     */
    GLuint GetProgramID() const { return program_id_; }
    
    /**
     * Check if shader program is valid and linked
     */
    bool IsValid() const { return program_id_ != 0; }
    
    // ===== UNIFORM SETTERS =====
    
    void SetBool(const std::string& name, bool value) const;
    void SetInt(const std::string& name, int value) const;
    void SetFloat(const std::string& name, float value) const;
    
    void SetVec2(const std::string& name, float x, float y) const;
    void SetVec3(const std::string& name, float x, float y, float z) const;
    void SetVec4(const std::string& name, float x, float y, float z, float w) const;
    
    void SetMat3(const std::string& name, const float* value) const;
    void SetMat4(const std::string& name, const float* value) const;
    
private:
    GLuint program_id_;
    
    /**
     * Compile a shader from source
     * @param source Shader source code
     * @param type GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
     * @param shader_id Output shader ID
     * @return true on success, false on failure
     */
    bool CompileShader(const std::string& source, GLenum type, GLuint& shader_id);
    
    /**
     * Link vertex and fragment shaders into program
     * @param vertex_shader Compiled vertex shader ID
     * @param fragment_shader Compiled fragment shader ID
     * @return true on success, false on failure
     */
    bool LinkProgram(GLuint vertex_shader, GLuint fragment_shader);
    
    /**
     * Read entire file into string
     * @param path File path
     * @return File contents, or empty string on error
     */
    std::string ReadFile(const std::string& path);
    
    /**
     * Get uniform location (cached lookup)
     * @param name Uniform variable name
     * @return Uniform location, or -1 if not found
     */
    GLint GetUniformLocation(const std::string& name) const;
};

} // namespace render
} // namespace segmecam

#endif // SEGMECAM_SHADER_PROGRAM_H
