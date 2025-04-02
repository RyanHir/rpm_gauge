#include "text_renderer.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <glm/glm.hpp>

#if __APPLE__
static const char FONT_PATH[] =
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf";
#endif

static constexpr bool DEBUG_DISABLE_BLENDING = false;

static constexpr glm::vec2 verts[] = {
    glm::vec2{0, 0},
    glm::vec2{0, 1},
    glm::vec2{1, 0},
    glm::vec2{1, 1},
};

static const char *vert = R"GLSL(#version 410 core
layout(location=0) in vec2 position;

uniform mat4 transform;
uniform mat4 projection;

out vec2 TexCoords;

void main()
{
  gl_Position = projection * transform * vec4(position.xy, 0.0, 1.0);
  TexCoords = position.xy;
  TexCoords.y = 1.0f - TexCoords.y;
}
)GLSL";

static const char *frag = R"GLSL(#version 410 core
in vec2 TexCoords;
out vec4 outColor;

uniform vec3 color;
uniform sampler2D text;

void main()
{
  outColor = vec4(color, texture(text, TexCoords).r);
}
)GLSL";

void text_renderer::allocate() {
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  GLint success = 0;

  GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vert_shader, 1, &vert, NULL);
  glCompileShader(vert_shader);
  glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &success);
  assert(success);

  GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(frag_shader, 1, &frag, NULL);
  glCompileShader(frag_shader);
  glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &success);
  assert(success);

  GLuint program_id = glCreateProgram();

  glAttachShader(program_id, vert_shader);
  glAttachShader(program_id, frag_shader);
  glLinkProgram(program_id);
  glDetachShader(program_id, vert_shader);
  glDetachShader(program_id, frag_shader);

  glDeleteShader(vert_shader);
  glDeleteShader(frag_shader);

  glGetProgramiv(program_id, GL_LINK_STATUS, (int *)&success);
  assert(success);

  program = program_id;

  load_characters();
}

void text_renderer::destroy() {
  program.delete_();
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
}

void text_renderer::set_window_size(int width, int height) {
  glm::mat4 projection = glm::ortho<float>(0, width, 0, height);
  program.setUniform("projection", projection);
}

void text_renderer::set_color(glm::vec3 color) {
  program.setUniform("color", color);
}

void text_renderer::draw(std::string_view text, float x, float y, float scale) {
  GLint last_blend;
  GLint last_blend_src_alpha;
  GLint last_program;
  GLint last_texture_2D;
  GLint last_vertex_array;
  glGetIntegerv(GL_BLEND, &last_blend);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &last_blend_src_alpha);
  glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture_2D);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

  if constexpr (!DEBUG_DISABLE_BLENDING) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }

  // activate corresponding render state
  program.use();
  glBindVertexArray(vao);

  // iterate through all characters
  for (char c : text) {
    const impl::character_details &details = ch[c];

    if (c == ' ') {
      x += (details.Advance >> 6) * scale;
    } else {
      float xpos = x + details.Bearing.x * scale;
      float ypos = y - (details.Size.y - details.Bearing.y) * scale;

      glm::mat4 transform =
          glm::translate(glm::mat4{1.0f}, glm::vec3{xpos, ypos, 0}) *
          glm::scale(glm::mat4{1.0f}, glm::vec3{details.Size.x * scale,
                                                details.Size.y * scale, 1});
      // render quad
      program.setUniform("transform", transform);

      // render glyph texture over quad
      // glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, 1);
      glBindTexture(GL_TEXTURE_2D, details.TextureID);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      // now advance cursors for next glyph (note that advance is number of 1/64
      // pixels)
      // bitshift by 6 to get value in pixels (2^6 = 64)
      x += (details.Advance >> 6) * scale;
    }
  }
  glBindVertexArray(last_vertex_array);
  glBindTexture(GL_TEXTURE_2D, last_texture_2D);
  glUseProgram(last_program);
  glBlendFunc(GL_SRC_ALPHA, last_blend_src_alpha);
  last_blend ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
}

void text_renderer::load_characters() {
  GLint last_unpack_alignment;
  GLint last_texture_2D;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &last_unpack_alignment);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture_2D);

  FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
    exit(1);
  }
  FT_Face face;
  if (FT_New_Face(ft, FONT_PATH, 0, &face)) {
    exit(1);
  }
  FT_Set_Pixel_Sizes(face, 0, 48);

  GLuint textures[127];
  glGenTextures(127, textures);

  // disable byte-alignment restriction
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  for (unsigned char c = 0; c < 128; c++) {
    // load character glyph
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
      continue;
    }
    // generate texture
    glBindTexture(GL_TEXTURE_2D, textures[c]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, face->glyph->bitmap.width,
                 face->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE,
                 face->glyph->bitmap.buffer);
    // set texture options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // now store character for later use
    impl::character_details character = {
        textures[c],
        glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
        glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
        static_cast<unsigned int>(face->glyph->advance.x)};
    ch.insert(std::pair<char, impl::character_details>(c, character));
  }
  FT_Done_Face(face);
  FT_Done_FreeType(ft);

  glBindTexture(GL_TEXTURE_2D, last_texture_2D);
  glPixelStorei(GL_UNPACK_ALIGNMENT, last_unpack_alignment);
}