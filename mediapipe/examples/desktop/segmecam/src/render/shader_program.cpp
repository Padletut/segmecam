#include "include/render/shader_program.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

#include "absl/log/log.h"
#include "absl/log/absl_log.h"

namespace segmecam {
namespace render {

ShaderProgram::ShaderProgram() : program_id_(0) {}

ShaderProgram::~ShaderProgram() {
    if (program_id_ != 0) {
        glDeleteProgram(program_id_);
        program_id_ = 0;
    }
}

bool ShaderProgram::LoadFromStrings(const std::string& vertex_source,
                                    const std::string& fragment_source) {
    GLuint vertex_shader = 0;
    GLuint fragment_shader = 0;
    
    // Compile vertex shader
    if (!CompileShader(vertex_source, GL_VERTEX_SHADER, vertex_shader)) {
        LOG(ERROR) << "Failed to compile vertex shader";
        return false;
    }
    
    // Compile fragment shader
    if (!CompileShader(fragment_source, GL_FRAGMENT_SHADER, fragment_shader)) {
        LOG(ERROR) << "Failed to compile fragment shader";
        glDeleteShader(vertex_shader);
        return false;
    }
    
    // Link program
    if (!LinkProgram(vertex_shader, fragment_shader)) {
        LOG(ERROR) << "Failed to link shader program";
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return false;
    }
    
    // Clean up shaders (they're now linked into the program)
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    LOG(INFO) << "Shader program created successfully (ID: " << program_id_ << ")";
    return true;
}

bool ShaderProgram::LoadFromFiles(const std::string& vertex_path,
                                  const std::string& fragment_path) {
    // Read shader source files
    std::string vertex_source = ReadFile(vertex_path);
    if (vertex_source.empty()) {
        LOG(ERROR) << "Failed to read vertex shader file: " << vertex_path;
        return false;
    }
    
    std::string fragment_source = ReadFile(fragment_path);
    if (fragment_source.empty()) {
        LOG(ERROR) << "Failed to read fragment shader file: " << fragment_path;
        return false;
    }
    
    LOG(INFO) << "Loaded vertex shader from: " << vertex_path;
    LOG(INFO) << "Loaded fragment shader from: " << fragment_path;
    
    // Compile and link
    return LoadFromStrings(vertex_source, fragment_source);
}

void ShaderProgram::Use() const {
    if (program_id_ != 0) {
        glUseProgram(program_id_);
    } else {
        LOG(WARNING) << "Attempting to use invalid shader program";
    }
}

// ===== UNIFORM SETTERS =====

void ShaderProgram::SetBool(const std::string& name, bool value) const {
    glUniform1i(GetUniformLocation(name), static_cast<int>(value));
}

void ShaderProgram::SetInt(const std::string& name, int value) const {
    glUniform1i(GetUniformLocation(name), value);
}

void ShaderProgram::SetFloat(const std::string& name, float value) const {
    glUniform1f(GetUniformLocation(name), value);
}

void ShaderProgram::SetVec2(const std::string& name, float x, float y) const {
    glUniform2f(GetUniformLocation(name), x, y);
}

void ShaderProgram::SetVec3(const std::string& name, float x, float y, float z) const {
    glUniform3f(GetUniformLocation(name), x, y, z);
}

void ShaderProgram::SetVec4(const std::string& name, float x, float y, float z, float w) const {
    glUniform4f(GetUniformLocation(name), x, y, z, w);
}

void ShaderProgram::SetMat3(const std::string& name, const float* value) const {
    glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, value);
}

void ShaderProgram::SetMat4(const std::string& name, const float* value) const {
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, value);
}

// ===== PRIVATE HELPER FUNCTIONS =====

bool ShaderProgram::CompileShader(const std::string& source, GLenum type, GLuint& shader_id) {
    const char* shader_type_name = (type == GL_VERTEX_SHADER) ? "vertex" : "fragment";
    
    // Create shader
    shader_id = glCreateShader(type);
    if (shader_id == 0) {
        LOG(ERROR) << "Failed to create " << shader_type_name << " shader";
        return false;
    }
    
    // Compile shader
    const char* source_cstr = source.c_str();
    glShaderSource(shader_id, 1, &source_cstr, nullptr);
    glCompileShader(shader_id);
    
    // Check compilation status
    GLint success = 0;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
    
    if (!success) {
        // Get error log
        GLint log_length = 0;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);
        
        std::vector<char> log(log_length);
        glGetShaderInfoLog(shader_id, log_length, &log_length, log.data());
        
        LOG(ERROR) << "Shader compilation failed (" << shader_type_name << "):\n"
                   << log.data();
        
        glDeleteShader(shader_id);
        shader_id = 0;
        return false;
    }
    
    LOG(INFO) << "Compiled " << shader_type_name << " shader successfully (ID: " << shader_id << ")";
    return true;
}

bool ShaderProgram::LinkProgram(GLuint vertex_shader, GLuint fragment_shader) {
    // Create program
    program_id_ = glCreateProgram();
    if (program_id_ == 0) {
        LOG(ERROR) << "Failed to create shader program";
        return false;
    }
    
    // Attach shaders
    glAttachShader(program_id_, vertex_shader);
    glAttachShader(program_id_, fragment_shader);
    
    // Link program
    glLinkProgram(program_id_);
    
    // Check link status
    GLint success = 0;
    glGetProgramiv(program_id_, GL_LINK_STATUS, &success);
    
    if (!success) {
        // Get error log
        GLint log_length = 0;
        glGetProgramiv(program_id_, GL_INFO_LOG_LENGTH, &log_length);
        
        std::vector<char> log(log_length);
        glGetProgramInfoLog(program_id_, log_length, &log_length, log.data());
        
        LOG(ERROR) << "Shader program linking failed:\n" << log.data();
        
        glDeleteProgram(program_id_);
        program_id_ = 0;
        return false;
    }
    
    LOG(INFO) << "Linked shader program successfully (ID: " << program_id_ << ")";
    return true;
}

std::string ShaderProgram::ReadFile(const std::string& path) {
    std::ifstream file(path);
    
    if (!file.is_open()) {
        LOG(ERROR) << "Failed to open file: " << path;
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    return buffer.str();
}

GLint ShaderProgram::GetUniformLocation(const std::string& name) const {
    if (program_id_ == 0) {
        LOG(WARNING) << "Attempting to get uniform location from invalid program";
        return -1;
    }
    
    GLint location = glGetUniformLocation(program_id_, name.c_str());
    
    if (location == -1) {
        LOG(WARNING) << "Uniform '" << name << "' not found in shader program";
    }
    
    return location;
}

} // namespace render
} // namespace segmecam
