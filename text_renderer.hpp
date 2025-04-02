#pragma once

#include "shader.hpp"
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string_view>
#include <unordered_map>

namespace impl {
struct character_details {
  GLuint TextureID;     // ID handle of the glyph texture
  glm::ivec2 Size;      // Size of glyph
  glm::ivec2 Bearing;   // Offset from baseline to left/top of glyph
  unsigned int Advance; // Offset to advance to next glyph
};

} // namespace impl

class text_renderer {
public:
  void allocate();
  void destroy();

  void set_window_size(int width, int height);
  void set_color(glm::vec3 color);
  void draw(std::string_view text, float x, float y, float scale);

private:
  GLuint vao, vbo;
  Program program;
  std::unordered_map<char, impl::character_details> ch;

  void load_characters();
};
