// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// Filter Object Implementation - Phase 2 Step 5
// Primitive geometry generators for AR filter testing

#include "mediapipe/examples/desktop/segmecam/include/ar_filters/filter_object.h"

#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace segmecam {

// Generate unique filter ID using timestamp + counter
std::string GenerateFilterId() {
  static int counter = 0;
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()).count();
  
  std::ostringstream oss;
  oss << "filter_" << timestamp << "_" << std::setfill('0') << std::setw(4) << counter++;
  return oss.str();
}

// Create a cube primitive
FilterObject CreateCube(const std::string& anchor_name,
                       const std::string& name,
                       float size) {
  FilterObject filter;
  filter.id = GenerateFilterId();
  filter.name = name;
  filter.anchor_name = anchor_name;
  
  float half = size * 0.5f;
  
  // Cube vertices (8 corners)
  filter.vertices = {
    // Front face
    {-half, -half,  half},  // 0: bottom-left-front
    { half, -half,  half},  // 1: bottom-right-front
    { half,  half,  half},  // 2: top-right-front
    {-half,  half,  half},  // 3: top-left-front
    // Back face
    {-half, -half, -half},  // 4: bottom-left-back
    { half, -half, -half},  // 5: bottom-right-back
    { half,  half, -half},  // 6: top-right-back
    {-half,  half, -half}   // 7: top-left-back
  };
  
  // Cube indices (12 triangles, 2 per face)
  filter.indices = {
    // Front face
    0, 1, 2,  0, 2, 3,
    // Right face
    1, 5, 6,  1, 6, 2,
    // Back face
    5, 4, 7,  5, 7, 6,
    // Left face
    4, 0, 3,  4, 3, 7,
    // Top face
    3, 2, 6,  3, 6, 7,
    // Bottom face
    4, 5, 1,  4, 1, 0
  };
  
  // Normals (per-vertex, averaged from adjacent faces)
  filter.normals = {
    {-0.577f, -0.577f,  0.577f},  // 0
    { 0.577f, -0.577f,  0.577f},  // 1
    { 0.577f,  0.577f,  0.577f},  // 2
    {-0.577f,  0.577f,  0.577f},  // 3
    {-0.577f, -0.577f, -0.577f},  // 4
    { 0.577f, -0.577f, -0.577f},  // 5
    { 0.577f,  0.577f, -0.577f},  // 6
    {-0.577f,  0.577f, -0.577f}   // 7
  };
  
  return filter;
}

// Create a sphere primitive using UV sphere algorithm
FilterObject CreateSphere(const std::string& anchor_name,
                         const std::string& name,
                         int segments,
                         float radius) {
  FilterObject filter;
  filter.id = GenerateFilterId();
  filter.name = name;
  filter.anchor_name = anchor_name;
  
  const float PI = 3.14159265359f;
  
  // Generate vertices
  for (int lat = 0; lat <= segments; ++lat) {
    float theta = lat * PI / segments;
    float sin_theta = std::sin(theta);
    float cos_theta = std::cos(theta);
    
    for (int lon = 0; lon <= segments; ++lon) {
      float phi = lon * 2.0f * PI / segments;
      float sin_phi = std::sin(phi);
      float cos_phi = std::cos(phi);
      
      float x = cos_phi * sin_theta;
      float y = cos_theta;
      float z = sin_phi * sin_theta;
      
      filter.vertices.push_back({x * radius, y * radius, z * radius});
      filter.normals.push_back({x, y, z});  // Normalized normal
    }
  }
  
  // Generate indices
  for (int lat = 0; lat < segments; ++lat) {
    for (int lon = 0; lon < segments; ++lon) {
      int first = (lat * (segments + 1)) + lon;
      int second = first + segments + 1;
      
      // First triangle
      filter.indices.push_back(first);
      filter.indices.push_back(second);
      filter.indices.push_back(first + 1);
      
      // Second triangle
      filter.indices.push_back(second);
      filter.indices.push_back(second + 1);
      filter.indices.push_back(first + 1);
    }
  }
  
  return filter;
}

// Create a cylinder primitive
FilterObject CreateCylinder(const std::string& anchor_name,
                           const std::string& name,
                           float radius,
                           float height,
                           int segments) {
  FilterObject filter;
  filter.id = GenerateFilterId();
  filter.name = name;
  filter.anchor_name = anchor_name;
  
  const float PI = 3.14159265359f;
  float half_height = height * 0.5f;
  
  // Generate vertices for top and bottom circles
  for (int i = 0; i <= segments; ++i) {
    float angle = 2.0f * PI * i / segments;
    float x = radius * std::cos(angle);
    float z = radius * std::sin(angle);
    
    // Bottom circle
    filter.vertices.push_back({x, -half_height, z});
    filter.normals.push_back({x / radius, 0.0f, z / radius});
    
    // Top circle
    filter.vertices.push_back({x, half_height, z});
    filter.normals.push_back({x / radius, 0.0f, z / radius});
  }
  
  // Add center vertices for caps
  int bottom_center = filter.vertices.size();
  filter.vertices.push_back({0.0f, -half_height, 0.0f});
  filter.normals.push_back({0.0f, -1.0f, 0.0f});
  
  int top_center = filter.vertices.size();
  filter.vertices.push_back({0.0f, half_height, 0.0f});
  filter.normals.push_back({0.0f, 1.0f, 0.0f});
  
  // Generate indices for side faces
  for (int i = 0; i < segments; ++i) {
    int bottom_current = i * 2;
    int bottom_next = ((i + 1) % segments) * 2;
    int top_current = bottom_current + 1;
    int top_next = bottom_next + 1;
    
    // Side quad (2 triangles)
    filter.indices.push_back(bottom_current);
    filter.indices.push_back(top_current);
    filter.indices.push_back(top_next);
    
    filter.indices.push_back(bottom_current);
    filter.indices.push_back(top_next);
    filter.indices.push_back(bottom_next);
  }
  
  // Generate indices for bottom cap
  for (int i = 0; i < segments; ++i) {
    int current = i * 2;
    int next = ((i + 1) % segments) * 2;
    
    filter.indices.push_back(bottom_center);
    filter.indices.push_back(next);
    filter.indices.push_back(current);
  }
  
  // Generate indices for top cap
  for (int i = 0; i < segments; ++i) {
    int current = i * 2 + 1;
    int next = ((i + 1) % segments) * 2 + 1;
    
    filter.indices.push_back(top_center);
    filter.indices.push_back(current);
    filter.indices.push_back(next);
  }
  
  return filter;
}

// Create a cone primitive
FilterObject CreateCone(const std::string& anchor_name,
                       const std::string& name,
                       float base_radius,
                       float height,
                       int segments) {
  FilterObject filter;
  filter.id = GenerateFilterId();
  filter.name = name;
  filter.anchor_name = anchor_name;
  
  const float PI = 3.14159265359f;
  
  // Apex vertex at top
  filter.vertices.push_back({0.0f, height, 0.0f});
  filter.normals.push_back({0.0f, 1.0f, 0.0f});
  
  // Base circle vertices
  for (int i = 0; i <= segments; ++i) {
    float angle = 2.0f * PI * i / segments;
    float x = base_radius * std::cos(angle);
    float z = base_radius * std::sin(angle);
    
    filter.vertices.push_back({x, 0.0f, z});
    
    // Calculate normal (pointing outward from cone surface)
    float nx = x / base_radius;
    float nz = z / base_radius;
    float ny = base_radius / height;  // Slope component
    float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    filter.normals.push_back({nx / len, ny / len, nz / len});
  }
  
  // Base center vertex
  int base_center = filter.vertices.size();
  filter.vertices.push_back({0.0f, 0.0f, 0.0f});
  filter.normals.push_back({0.0f, -1.0f, 0.0f});
  
  // Generate indices for cone sides
  for (int i = 0; i < segments; ++i) {
    int current = i + 1;
    int next = ((i + 1) % segments) + 1;
    
    filter.indices.push_back(0);      // Apex
    filter.indices.push_back(current);
    filter.indices.push_back(next);
  }
  
  // Generate indices for base cap
  for (int i = 0; i < segments; ++i) {
    int current = i + 1;
    int next = ((i + 1) % segments) + 1;
    
    filter.indices.push_back(base_center);
    filter.indices.push_back(next);
    filter.indices.push_back(current);
  }
  
  return filter;
}

}  // namespace segmecam
