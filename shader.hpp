#pragma once

#include <glad/gl.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>

namespace impl {
struct Program {
  GLuint program_id{0};

  static Program getCurrentProgram() {
    GLuint program_id;
    glGetIntegerv(GL_CURRENT_PROGRAM, (GLint *)&program_id);
    return Program{program_id};
  }

  void useProgram() { glUseProgram(program_id); }
  GLuint getUniformLocation(const char *name) {
    return glGetUniformLocation(program_id, name);
  }

  void uniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose,
                        const GLfloat *value) {
    Program prev = Program::getCurrentProgram();
    useProgram();
    glUniformMatrix3fv(location, count, transpose, value);
    prev.useProgram();
  }
  void uniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose,
                        const GLfloat *value) {
    Program prev = Program::getCurrentProgram();
    useProgram();
    glUniformMatrix4fv(location, count, transpose, value);
    prev.useProgram();
  }
  void uniform3fv(GLint location, GLsizei count, const GLfloat *value) {
    Program prev = Program::getCurrentProgram();
    useProgram();
    glUniform3fv(location, count, value);
    prev.useProgram();
  }
  void uniform4fv(GLint location, GLsizei count, const GLfloat *value) {
    Program prev = Program::getCurrentProgram();
    useProgram();
    glUniform4fv(location, count, value);
    prev.useProgram();
  }
  void uniform1i(GLint location, const GLint value) {
    Program prev = Program::getCurrentProgram();
    useProgram();
    glUniform1i(location, value);
    prev.useProgram();
  }
};
} // namespace impl

class Program {
public:
  constexpr Program() : program(0) {}
  constexpr Program(GLuint id) : program(id) {}
  void use() { program.useProgram(); }
  void delete_() { glDeleteProgram(program.program_id); }
  void setUniform(const char *name, bool data) {
    program.uniform1i(program.getUniformLocation(name),
                      static_cast<GLint>(data ? GL_TRUE : GL_FALSE));
  }
  void setUniform(const char *name, GLint data) {
    program.uniform1i(program.getUniformLocation(name), data);
  }
  void setUniform(const char *name, glm::mat3 const &data) {
    program.uniformMatrix3fv(program.getUniformLocation(name), 1, GL_FALSE,
                             glm::value_ptr(data));
  }
  void setUniform(const char *name, glm::mat4 const &data) {
    program.uniformMatrix4fv(program.getUniformLocation(name), 1, GL_FALSE,
                             glm::value_ptr(data));
  }
  void setUniform(const char *name, glm::vec3 const &data) {
    program.uniform3fv(program.getUniformLocation(name), 1,
                       glm::value_ptr(data));
  }
  void setUniform(const char *name, glm::vec4 const &data) {
    program.uniform3fv(program.getUniformLocation(name), 1,
                       glm::value_ptr(data));
  }

private:
  impl::Program program;
};
